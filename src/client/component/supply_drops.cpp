#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "game/game.hpp"

#include "auth.hpp"
#include "command.hpp"
#include "console.hpp"
#include "filesystem.hpp"
#include "scheduler.hpp"
#include "supply_drops.hpp"

#include "resource.hpp"

#include <utils/hook.hpp>
#include <utils/io.hpp>
#include <utils/nt.hpp>
#include <utils/string.hpp>

#include <algorithm>
#include <array>
#include <map>
#include <random>
#include <set>

namespace supply_drops
{
	namespace
	{
		constexpr auto file = "players2/user/supply_drops.json";
		constexpr size_t max_unopened = 50;
		constexpr size_t max_unopened_advanced = 1000;

		constexpr size_t packages_per_group = 2;

		struct loot_item
		{
			uint32_t guid;
			int rarity;
		};

		std::recursive_mutex mutex;
		bool loaded = false;
		std::vector<batch> batches{};
		std::vector<uint32_t> inventory{};
		std::map<uint8_t, uint32_t> wallet{};
		std::map<uint32_t, std::string> item_data{};
		uint32_t play_time = 0;
		uint32_t advanced_play_time = 0;
		uint32_t codpoints_play_time = 0;

		constexpr uint8_t codpoints_currency = 2;

		constexpr uint32_t duplicate_codpoints[] = {10, 25, 50, 100, 200};

		game::dvar_t* loot_codpoints = nullptr;
		game::dvar_t* loot_codpoints_interval = nullptr;
		game::dvar_t* loot_codpoints_per_interval = nullptr;
		game::dvar_t* loot_supply_drop_interval = nullptr;
		game::dvar_t* loot_advanced_supply_drop_interval = nullptr;
		game::dvar_t* loot_armory_slots = nullptr;

		constexpr int base_armory_slots = 84;

		void save();

		std::string to_hex(const std::string& data)
		{
			std::string result{};
			for (const auto c : data)
			{
				result += utils::string::va("%02X", static_cast<uint8_t>(c));
			}

			return result;
		}

		std::string from_hex(const std::string& text)
		{
			std::string result{};
			for (size_t i = 0; i + 1 < text.size(); i += 2)
			{
				result.push_back(static_cast<char>(std::strtoul(text.substr(i, 2).data(), nullptr, 16)));
			}

			return result;
		}

		std::mt19937& rng()
		{
			static std::mt19937 engine{std::random_device{}()};
			return engine;
		}

		const std::vector<loot_item>& get_loot_table()
		{
			static std::vector<loot_item> table{};
			static auto parsed = false;
			if (parsed)
			{
				return table;
			}

			parsed = true;

			const std::string path = "dw/lootConfig_tu22.csv";
			const auto data = filesystem::exists(path)
				? filesystem::read_file(path)
				: utils::nt::load_resource(DW_LOOT_CONFIG);

			for (auto line : utils::string::split(data, '\n'))
			{
				if (!line.empty() && line.back() == '\r')
				{
					line.pop_back();
				}

				if (line.empty() || line[0] == '#')
				{
					continue;
				}

				const auto columns = utils::string::split(line, ',');
				if (columns.size() < 5 || columns[2].empty()
					|| (columns[4] == "Never" && columns[0].rfind("iw5_dlcgun", 0) != 0))
				{
					continue;
				}

				const auto guid = static_cast<uint32_t>(std::strtoul(columns[2].data(), nullptr, 16));
				if (guid)
				{
					table.push_back({guid, std::atoi(columns[3].data())});
				}
			}

			return table;
		}

		constexpr uint32_t advanced_type = 4;

		int roll_rarity(const bool advanced, const int min_rarity)
		{
			constexpr int weights[] = {55, 30, 12, 2, 1};
			constexpr int advanced_weights[] = {30, 40, 22, 5, 3};

			int w[5]{};
			for (auto i = 0; i < 5; ++i)
			{
				w[i] = i < min_rarity ? 0 : (advanced ? advanced_weights[i] : weights[i]);
			}

			std::discrete_distribution<int> distribution(std::begin(w), std::end(w));
			return distribution(rng());
		}

		bool is_owned(const uint32_t guid)
		{
			if (std::find(inventory.begin(), inventory.end(), guid) != inventory.end())
			{
				return true;
			}

			for (const auto& b : batches)
			{
				if (!b.opened && std::find(b.items.begin(), b.items.end(), guid) != b.items.end())
				{
					return true;
				}
			}

			return false;
		}

		uint32_t roll_item(const std::vector<uint32_t>& exclude, const bool advanced = false, const int min_rarity = 0)
		{
			const auto& table = get_loot_table();
			if (table.empty())
			{
				return 0;
			}

			const auto rarity = roll_rarity(advanced, min_rarity);
			const auto not_excluded = [&](const loot_item& item)
			{
				return std::find(exclude.begin(), exclude.end(), item.guid) == exclude.end();
			};

			std::vector<uint32_t> candidates{};
			for (const auto& item : table)
			{
				if (item.rarity == rarity && !is_owned(item.guid) && not_excluded(item))
				{
					candidates.push_back(item.guid);
				}
			}

			if (candidates.empty())
			{
				for (const auto& item : table)
				{
					if (item.rarity == rarity && not_excluded(item))
					{
						candidates.push_back(item.guid);
					}
				}
			}

			if (!candidates.empty())
			{
				std::uniform_int_distribution<size_t> distribution(0, candidates.size() - 1);
				return candidates[distribution(rng())];
			}

			std::uniform_int_distribution<size_t> distribution(0, table.size() - 1);
			return table[distribution(rng())].guid;
		}

		void fill_items(batch& b)
		{
			while (b.items.size() < b.coupons.size())
			{
				const auto advanced = b.type == advanced_type;
				b.items.push_back(roll_item(b.items, advanced, advanced && b.items.empty() ? 2 : 0));
			}

			b.items.resize(b.coupons.size());
		}

		size_t count_group(const uint32_t type, const uint64_t group)
		{
			return static_cast<size_t>(std::count_if(batches.begin(), batches.end(), [&](const batch& b)
			{
				return !b.opened && b.type == type && b.group == group;
			}));
		}

		uint64_t find_group(const uint32_t type)
		{
			uint64_t newest = 0;
			uint64_t highest = 0;
			for (const auto& b : batches)
			{
				highest = std::max(highest, b.group);
				if (!b.opened && b.type == type)
				{
					newest = std::max(newest, b.group);
				}
			}

			if (newest && count_group(type, newest) < packages_per_group)
			{
				return newest;
			}

			return highest + 1;
		}

		uint64_t next_id()
		{
			uint64_t id = 1000;
			for (const auto& b : batches)
			{
				if (b.id < 0x7FFFFFFF && b.id >= id)
				{
					id = b.id + 1;
				}
			}

			return id;
		}

		void load()
		{
			std::lock_guard _(mutex);
			if (loaded)
			{
				return;
			}

			loaded = true;

			std::string data{};
			if (!utils::io::read_file(file, &data))
			{
				return;
			}

			rapidjson::Document doc{};
			if (doc.Parse(data.data(), data.size()).HasParseError() || !doc.IsObject())
			{
				return;
			}

			if (doc.HasMember("play_time") && doc["play_time"].IsUint())
			{
				play_time = doc["play_time"].GetUint();
			}

			if (doc.HasMember("advanced_play_time") && doc["advanced_play_time"].IsUint())
			{
				advanced_play_time = doc["advanced_play_time"].GetUint();
			}

			if (doc.HasMember("codpoints_play_time") && doc["codpoints_play_time"].IsUint())
			{
				codpoints_play_time = doc["codpoints_play_time"].GetUint();
			}

			if (doc.HasMember("inventory") && doc["inventory"].IsArray())
			{
				for (const auto& item : doc["inventory"].GetArray())
				{
					if (item.IsUint())
					{
						inventory.push_back(item.GetUint());
					}
				}
			}

			if (doc.HasMember("wallet") && doc["wallet"].IsObject())
			{
				const auto& entries = doc["wallet"];
				for (auto entry = entries.MemberBegin(); entry != entries.MemberEnd(); ++entry)
				{
					if (entry->value.IsUint())
					{
						wallet[static_cast<uint8_t>(std::atoi(entry->name.GetString()))] = entry->value.GetUint();
					}
				}
			}

			if (doc.HasMember("item_data") && doc["item_data"].IsObject())
			{
				const auto& entries = doc["item_data"];
				for (auto entry = entries.MemberBegin(); entry != entries.MemberEnd(); ++entry)
				{
					if (entry->value.IsString())
					{
						const auto item = static_cast<uint32_t>(std::strtoul(entry->name.GetString(), nullptr, 16));
						item_data[item] = from_hex(entry->value.GetString());
					}
				}
			}

			const auto migrated = !doc.HasMember("item_data_migrated");
			if (migrated)
			{
				for (const auto item : inventory)
				{
					auto& seen = item_data[item];
					seen.resize(64);
					seen[0] = static_cast<char>(seen[0] | 0x02);
				}
			}

			if (!doc.HasMember("batches") || !doc["batches"].IsArray())
			{
				if (migrated)
				{
					save();
				}

				return;
			}

			for (const auto& entry : doc["batches"].GetArray())
			{
				if (!entry.IsObject())
				{
					continue;
				}

				batch b{};
				b.type = entry.HasMember("type") && entry["type"].IsUint() ? entry["type"].GetUint() : 2;
				b.id = entry.HasMember("id") && entry["id"].IsUint64() ? entry["id"].GetUint64() : 0;
				b.claimed = entry.HasMember("claimed") && entry["claimed"].IsUint() ? entry["claimed"].GetUint() : 0;
				b.opened = entry.HasMember("opened") && entry["opened"].IsBool() && entry["opened"].GetBool();
				b.group = entry.HasMember("group") && entry["group"].IsUint64() ? entry["group"].GetUint64() : 0;
				b.bonus = entry.HasMember("bonus") && entry["bonus"].IsBool() && entry["bonus"].GetBool();

				if (entry.HasMember("coupons") && entry["coupons"].IsArray())
				{
					for (const auto& coupon : entry["coupons"].GetArray())
					{
						if (coupon.IsString())
						{
							b.coupons.emplace_back(coupon.GetString());
						}
					}
				}

				if (entry.HasMember("items") && entry["items"].IsArray())
				{
					for (const auto& item : entry["items"].GetArray())
					{
						if (item.IsUint())
						{
							b.items.push_back(item.GetUint());
						}
					}
				}

				if (b.id)
				{
					batches.push_back(std::move(b));
				}
			}

			auto changed = false;
			for (auto& b : batches)
			{
				if (b.id >= 0x7FFFFFFF)
				{
					b.id = next_id();
					changed = true;
				}

				if (b.type == 3 && !b.opened)
				{
					b.type = advanced_type;
					b.group = 0;
					changed = true;
				}

				if (b.items.size() != b.coupons.size())
				{
					fill_items(b);
					changed = true;
				}
			}

			std::map<std::pair<uint32_t, uint64_t>, size_t> group_sizes{};
			for (const auto& b : batches)
			{
				if (!b.opened && b.group)
				{
					++group_sizes[{b.type, b.group}];
				}
			}

			for (const auto& [key, size] : group_sizes)
			{
				if (size <= packages_per_group)
				{
					continue;
				}

				for (auto& b : batches)
				{
					if (!b.opened && b.type == key.first && b.group == key.second)
					{
						b.group = 0;
					}
				}
			}

			for (auto& b : batches)
			{
				if (!b.group)
				{
					b.group = b.opened ? 0 : find_group(b.type);
					changed = changed || !b.opened;
				}
			}

			if (changed || migrated)
			{
				save();
			}
		}

		void save()
		{
			std::lock_guard _(mutex);

			rapidjson::StringBuffer buffer{};
			rapidjson::PrettyWriter writer(buffer);

			writer.StartObject();
			writer.Key("play_time");
			writer.Uint(play_time);
			writer.Key("advanced_play_time");
			writer.Uint(advanced_play_time);
			writer.Key("codpoints_play_time");
			writer.Uint(codpoints_play_time);

			writer.Key("inventory");
			writer.StartArray();
			for (const auto item : inventory)
			{
				writer.Uint(item);
			}
			writer.EndArray();

			writer.Key("wallet");
			writer.StartObject();
			for (const auto& [currency, amount] : wallet)
			{
				const auto key = std::to_string(currency);
				writer.Key(key.data());
				writer.Uint(amount);
			}
			writer.EndObject();

			writer.Key("item_data_migrated");
			writer.Bool(true);

			writer.Key("item_data");
			writer.StartObject();
			for (const auto& [item, data] : item_data)
			{
				const auto key = utils::string::va("%X", item);
				writer.Key(key);
				const auto value = to_hex(data);
				writer.String(value.data());
			}
			writer.EndObject();

			writer.Key("batches");
			writer.StartArray();

			for (const auto& b : batches)
			{
				writer.StartObject();
				writer.Key("type");
				writer.Uint(b.type);
				writer.Key("id");
				writer.Uint64(b.id);
				writer.Key("group");
				writer.Uint64(b.group);
				writer.Key("claimed");
				writer.Uint(b.claimed);
				writer.Key("opened");
				writer.Bool(b.opened);
				if (b.bonus)
				{
					writer.Key("bonus");
					writer.Bool(true);
				}
				writer.Key("coupons");
				writer.StartArray();
				for (const auto& coupon : b.coupons)
				{
					writer.String(coupon.data());
				}
				writer.EndArray();
				writer.Key("items");
				writer.StartArray();
				for (const auto item : b.items)
				{
					writer.Uint(item);
				}
				writer.EndArray();
				writer.EndObject();
			}

			writer.EndArray();
			writer.EndObject();

			utils::io::write_file(file, std::string(buffer.GetString(), buffer.GetSize()));
		}

		std::string make_coupon_id()
		{
			static const char charset[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
			std::uniform_int_distribution<size_t> distribution(0, sizeof(charset) - 2);

			std::string id{};
			for (auto i = 0; i < 24; ++i)
			{
				id += charset[distribution(rng())];
			}

			return id;
		}

		std::array<bool, 4> refresh_pending{};

		size_t get_request_slot(const uint32_t type)
		{
			if (type == 4)
			{
				return 2;
			}

			if (type == 3)
			{
				return 3;
			}

			return type - 1;
		}

		void request_refresh(const uint32_t type)
		{
			if (type >= 1 && type <= refresh_pending.size())
			{
				refresh_pending[get_request_slot(type)] = true;
			}
		}

		void apply_pending_refresh()
		{
			for (size_t i = 0; i < refresh_pending.size(); ++i)
			{
				if (!refresh_pending[i])
				{
					continue;
				}

				const auto request = 0x1416A3720 + i * 0x120;
				if (*reinterpret_cast<uint8_t*>(request))
				{
					continue;
				}

				*reinterpret_cast<uint32_t*>(request + 0x4) = 0;
				*reinterpret_cast<uint32_t*>(request + 0xC) = 1;
				refresh_pending[i] = false;
			}
		}

		bool is_in_match();

		void notify_new_batch(const batch& b)
		{
			request_refresh(b.type);

			if (is_in_match())
			{
				utils::hook::invoke<void>(0x1400C8960, 0, 1, 1);
			}
		}

		bool is_in_match()
		{
			return game::CL_IsCgameInitialized() && !game::VirtualLobby_Loaded();
		}

		uint32_t get_interval(const game::dvar_t* dvar, const uint32_t fallback)
		{
			return dvar ? static_cast<uint32_t>(std::max(dvar->current.integer, 60)) : fallback;
		}

		void earn_supply_drop(const uint32_t type, const bool bonus = false)
		{
			const auto unopened = static_cast<size_t>(std::count_if(batches.begin(), batches.end(), [&](const batch& b)
			{
				return b.type == type && !b.opened;
			}));

			if (unopened >= (type == advanced_type ? max_unopened_advanced : max_unopened))
			{
				save();
				return;
			}

			const auto b = add_batch(type, 3, bonus);
			console::info("%s earned (batch %llu)\n", type == advanced_type ? "Advanced supply drop" : "Supply drop", b.id);
			notify_new_batch(b);
		}

		void update_codpoints_dvar()
		{
			if (loot_codpoints)
			{
				loot_codpoints->current.integer = static_cast<int>(wallet[codpoints_currency]);
			}
		}

		void update_play_time()
		{
			apply_pending_refresh();

			std::lock_guard _(mutex);
			load();
			update_codpoints_dvar();

			if (!is_in_match())
			{
				return;
			}

			++play_time;
			++advanced_play_time;
			++codpoints_play_time;

			if (codpoints_play_time >= get_interval(loot_codpoints_interval, 60))
			{
				codpoints_play_time = 0;
				const auto amount = loot_codpoints_per_interval ? static_cast<uint32_t>(std::max(loot_codpoints_per_interval->current.integer, 0)) : 10u;
				if (amount)
				{
					wallet[codpoints_currency] += amount;
					update_codpoints_dvar();
					save();
				}
			}

			auto earned = false;

			if (play_time >= get_interval(loot_supply_drop_interval, 1800))
			{
				play_time = 0;
				earn_supply_drop(2);
				earned = true;
			}

			if (advanced_play_time >= get_interval(loot_advanced_supply_drop_interval, 600))
			{
				advanced_play_time = 0;
				earn_supply_drop(advanced_type);
				earned = true;
			}

			if (!earned && play_time % 60 == 0)
			{
				save();
			}
		}
	}

	std::vector<group_info> get_groups(const uint32_t type, const size_t max_results)
	{
		std::lock_guard _(mutex);
		load();

		std::map<uint64_t, group_info> groups{};
		for (const auto& b : batches)
		{
			if (b.type != type || b.opened || !b.group)
			{
				continue;
			}

			auto& group = groups[b.group];
			group.type = type;
			group.id = b.group;

			const auto claimed = std::min(b.claimed, static_cast<uint32_t>(b.coupons.size()));
			group.claimed += claimed;
			group.pending += static_cast<uint32_t>(b.coupons.size()) - claimed;
		}

		std::vector<group_info> result{};
		for (const auto& [id, group] : groups)
		{
			result.push_back(group);
		}

		if (max_results && result.size() > max_results)
		{
			result.resize(max_results);
		}

		return result;
	}

	std::vector<uint32_t> get_loot_items()
	{
		std::vector<uint32_t> result{};
		for (const auto& item : get_loot_table())
		{
			result.push_back(item.guid);
		}

		return result;
	}

	size_t count_unopened_packages(const uint32_t type)
	{
		std::lock_guard _(mutex);
		load();

		return static_cast<size_t>(std::count_if(batches.begin(), batches.end(), [&](const batch& b)
		{
			return b.type == type && !b.opened;
		}));
	}

	std::vector<batch> get_group_packages(const uint32_t type, const uint64_t group)
	{
		std::lock_guard _(mutex);
		load();

		std::vector<batch> result{};
		for (const auto& b : batches)
		{
			if (b.type == type && b.group == group && !b.opened)
			{
				result.push_back(b);
			}
		}

		return result;
	}

	std::vector<batch> claim_coupons(const std::vector<std::string>& coupon_ids)
	{
		std::lock_guard _(mutex);
		load();

		std::vector<batch> result{};
		for (auto& b : batches)
		{
			auto touched = false;
			for (size_t i = 0; i < b.coupons.size(); ++i)
			{
				if (std::find(coupon_ids.begin(), coupon_ids.end(), b.coupons[i]) == coupon_ids.end())
				{
					continue;
				}

				touched = true;

				b.claimed = std::max(b.claimed, static_cast<uint32_t>(i + 1));
			}

			if (touched)
			{
				result.push_back(b);
			}
		}

		save();
		return result;
	}

	std::vector<uint32_t> redeem_coupons(const std::vector<std::string>& coupon_ids)
	{
		std::lock_guard _(mutex);
		load();

		std::vector<uint32_t> result{};
		std::set<std::pair<uint32_t, uint64_t>> opened_groups{};
		for (auto& b : batches)
		{
			const auto was_opened = b.opened;

			for (size_t i = 0; i < b.coupons.size() && i < b.items.size(); ++i)
			{
				if (std::find(coupon_ids.begin(), coupon_ids.end(), b.coupons[i]) == coupon_ids.end())
				{
					continue;
				}

				b.opened = true;
				opened_groups.insert({b.type, b.group});

				const auto item = b.items[i];
				result.push_back(item);

				if (!item)
				{
					continue;
				}

				if (std::find(inventory.begin(), inventory.end(), item) == inventory.end())
				{
					inventory.push_back(item);
				}
				else if (!was_opened && !b.bonus)
				{
					const auto rarity = get_item_rarity(item);
					if (rarity >= 0)
					{
						const auto amount = duplicate_codpoints[std::min(rarity, 4)];
						wallet[codpoints_currency] += amount;
						console::info("Duplicate item 0x%X: +%u CoD Points\n", item, amount);
					}
				}
			}
		}

		for (const auto& [type, group] : opened_groups)
		{
			const auto remaining = std::any_of(batches.begin(), batches.end(), [&](const batch& b)
			{
				return b.type == type && b.group == group && !b.opened;
			});

			if (!remaining || type == advanced_type)
			{
				request_refresh(type);
			}
		}

		update_codpoints_dvar();
		save();
		return result;
	}

	bool is_pending_sku(const uint32_t sku)
	{
		std::lock_guard _(mutex);
		load();

		if (!sku)
		{
			return false;
		}

		for (const auto& b : batches)
		{
			if (!b.opened && std::find(b.items.begin(), b.items.end(), sku) != b.items.end())
			{
				return true;
			}
		}

		return false;
	}

	std::vector<uint32_t> get_inventory()
	{
		std::lock_guard _(mutex);
		load();
		return inventory;
	}

	bool remove_item(const uint32_t item)
	{
		std::lock_guard _(mutex);
		load();

		const auto it = std::find(inventory.begin(), inventory.end(), item);
		if (it == inventory.end())
		{
			return false;
		}

		inventory.erase(it);
		item_data.erase(item);
		save();
		return true;
	}

	std::string get_item_data(const uint32_t item)
	{
		std::lock_guard _(mutex);
		load();

		const auto it = item_data.find(item);
		return it == item_data.end() ? std::string{} : it->second;
	}

	void set_item_data(const uint32_t item, const std::string& data)
	{
		std::lock_guard _(mutex);
		load();

		item_data[item] = data.substr(0, 64);
		save();
	}

	int get_item_rarity(const uint32_t item)
	{
		for (const auto& entry : get_loot_table())
		{
			if (entry.guid == item)
			{
				return entry.rarity;
			}
		}

		return -1;
	}

	uint32_t add_currency(const uint8_t currency, const uint32_t amount)
	{
		std::lock_guard _(mutex);
		load();

		auto& balance = wallet[currency];
		balance += amount;
		update_codpoints_dvar();
		save();
		return balance;
	}

	batch add_batch(const uint32_t type, const size_t coupon_count, const bool bonus)
	{
		std::lock_guard _(mutex);
		load();

		batch b{};
		b.type = type;
		b.id = next_id();
		b.group = find_group(type);
		b.bonus = bonus;

		for (size_t i = 0; i < coupon_count; ++i)
		{
			b.coupons.push_back(make_coupon_id());
		}

		fill_items(b);

		batches.push_back(b);
		save();
		return b;
	}

	uint32_t get_armory_slot_items()
	{
		const auto slots = loot_armory_slots ? loot_armory_slots->current.integer : base_armory_slots;
		if (slots <= base_armory_slots)
		{
			return 0;
		}

		return static_cast<uint32_t>((slots - base_armory_slots + 2) / 3);
	}

	class component final : public component_interface
	{
	public:
		void post_unpack() override
		{
			if (!game::environment::is_mp())
			{
				return;
			}

			loot_armory_slots = game::Dvar_RegisterInt("loot_armorySlots", 1200, base_armory_slots, 1200,
			                                           game::DVAR_FLAG_SAVED,
			                                           "Armory slots (84 = default). Applied on the next inventory load");

			loot_supply_drop_interval = game::Dvar_RegisterInt("loot_supplyDropInterval", 1800, 60, 86400,
			                                                   game::DVAR_FLAG_SAVED,
			                                                   "Seconds of match play time needed to earn a supply drop");

			loot_advanced_supply_drop_interval = game::Dvar_RegisterInt("loot_advancedSupplyDropInterval", 600, 60, 86400,
			                                                            game::DVAR_FLAG_SAVED,
			                                                            "Seconds of match play time needed to earn an advanced supply drop");

			loot_codpoints = game::Dvar_RegisterInt("loot_codPoints", 0, 0, std::numeric_limits<int>::max(),
			                                        game::DVAR_FLAG_NONE,
			                                        "CoD Points balance (read only, from players2/user/supply_drops.json)");

			loot_codpoints_interval = game::Dvar_RegisterInt("loot_codPointsInterval", 60, 60, 86400,
			                                                 game::DVAR_FLAG_SAVED,
			                                                 "Seconds of match play time needed to earn CoD Points");

			loot_codpoints_per_interval = game::Dvar_RegisterInt("loot_codPointsPerInterval", 10, 0, 100000,
			                                                     game::DVAR_FLAG_SAVED,
			                                                     "CoD Points earned every loot_codPointsInterval seconds of match play time");

			scheduler::loop(update_play_time, scheduler::pipeline::main, 1s);

			command::add("buysupplydrop", [](const command::params& params)
			{
				if (params.size() < 4)
				{
					console::info("usage: buysupplydrop <type> <count> <cost>\n");
					return;
				}

				const auto type = static_cast<uint32_t>(std::atoi(params.get(1))) == advanced_type ? advanced_type : 2u;
				const auto count = std::clamp(std::atoi(params.get(2)), 1, 100);
				const auto cost = static_cast<uint32_t>(std::max(std::atoi(params.get(3)), 0));

				std::lock_guard _(mutex);
				load();

				auto& balance = wallet[codpoints_currency];
				if (balance < cost)
				{
					console::info("Not enough CoD Points (%u / %u)\n", balance, cost);
					return;
				}

				balance -= cost;
				update_codpoints_dvar();

				for (auto i = 0; i < count; ++i)
				{
					notify_new_batch(add_batch(type));
				}

				save();
				console::info("Bought %d supply drop(s) (type %u) for %u CoD Points, balance %u\n", count, type, cost, balance);
			});

			command::add("givecodpoints", [](const command::params& params)
			{
				const auto amount = params.size() > 1 ? static_cast<uint32_t>(std::max(std::atoi(params.get(1)), 0)) : 1000u;
				console::info("CoD Points: %u\n", add_currency(codpoints_currency, amount));
			});

			command::add("givesupplydrop", [](const command::params& params)
			{
				auto type = params.size() > 1 ? static_cast<uint32_t>(std::atoi(params.get(1))) : 2u;
				if (type == 3)
				{
					type = advanced_type;
				}

				const auto b = add_batch(type ? type : 2u);
				console::info("Supply drop added (type %u, batch %llu, %zu coupons)\n", b.type, b.id, b.coupons.size());
				notify_new_batch(b);
			});
		}
	};
}

REGISTER_COMPONENT(supply_drops::component)
