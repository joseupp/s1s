#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "game/game.hpp"

#include "command.hpp"
#include "console.hpp"
#include "scheduler.hpp"
#include "gsc/script_extension.hpp"
#include "lobby_bots.hpp"
#include "supply_drops.hpp"
#include "public_lobby.hpp"

#include <utils/hook.hpp>
#include <utils/io.hpp>
#include <utils/string.hpp>

#include <random>
#include <unordered_map>

namespace lobby_bots
{
	namespace
	{
		constexpr auto lobby_party = 0x1421CA8C0ull;
		constexpr auto max_members = 18;
		constexpr auto member_size = 0x358;

		namespace member
		{
			constexpr auto status = 0x0;
			constexpr auto party_id = 0x8;
			constexpr auto rank = 0x10;
			constexpr auto prestige = 0x14;
			constexpr auto rank_icons = 0x18;
			constexpr auto name = 0x30;
			constexpr auto name_size = 32;
			constexpr auto clan_tag = 0x5D;
			constexpr auto map_vote = 0x58;
			constexpr auto patch = 0x24;
			constexpr auto background = 0x28;
			constexpr auto costume = 0x20C;
			constexpr auto costume_size = 0x2C;
			constexpr auto kd_ratio = 0x74;
			constexpr auto win_streak = 0x78;
			constexpr auto games_played = 0x7C;
			constexpr auto win_loss = 0x80;
			constexpr auto time_played = 0x84;
			constexpr auto address = 0x2A0;
			constexpr auto last_packet = 0x2B0;
			constexpr auto team = 0x300;
			constexpr auto score = 0x306;
			constexpr auto deaths = 0x308;
			constexpr auto xuid = 0x318;
		}

		constexpr std::uint8_t status_in_lobby = 5;

		game::dvar_t* lobby_bots_enabled = nullptr;
		game::dvar_t* lobby_bots_max = nullptr;
		game::dvar_t* lobby_bots_join_min = nullptr;
		game::dvar_t* lobby_bots_join_max = nullptr;
		game::dvar_t* lobby_bots_leave_chance = nullptr;
		game::dvar_t* lobby_bots_difficulty = nullptr;

		std::recursive_mutex mutex;
		std::vector<std::uint64_t> fake_xuids{};
		int next_join_time = 0;
		bool was_in_match = false;
		bool first_wave_done = false;
		std::unordered_map<std::uint64_t, int> vote_times{};

		std::optional<std::string> pending_bot_name{};

		constexpr auto bot_costumes = 0x14B119B98ull;
		constexpr auto bot_costume_count = 0x14B119EB4ull;

		template <typename... Args>
		void write_log(const char* fmt, Args... args)
		{
			const std::string text = utils::string::va(fmt, args...);
			console::info("%s", text.data());
			utils::io::write_file("lobby_bots.log", text, true);
		}

		std::atomic_bool lobby_menu_entered{false};

		void on_lobby_menu_entered()
		{
			lobby_menu_entered = true;
			write_log("[lobby_bots] public lobby menu opened\n");
		}

		std::mt19937& rng()
		{
			static std::mt19937 engine{std::random_device{}()};
			return engine;
		}

		int random_int(const int min, const int max)
		{
			std::uniform_int_distribution<int> distribution(min, std::max(min, max));
			return distribution(rng());
		}

		std::uint8_t* get_member(const int index)
		{
			return reinterpret_cast<std::uint8_t*>(lobby_party + 0x1D0 + index * member_size);
		}

		template <typename T>
		T& field(std::uint8_t* m, const std::size_t offset)
		{
			return *reinterpret_cast<T*>(m + offset);
		}

		int get_party_int(const std::size_t offset)
		{
			return *reinterpret_cast<int*>(lobby_party + offset);
		}

		bool is_lobby_host()
		{
			return get_party_int(0x4178) != 0 && get_party_int(0x413C) == 0;
		}

		bool get_dvar_bool(const char* name)
		{
			const auto* dvar = game::Dvar_FindVar(name);
			return dvar && dvar->current.enabled;
		}

		int get_dvar_int(const char* name, const int default_value)
		{
			const auto* dvar = game::Dvar_FindVar(name);
			return dvar ? dvar->current.integer : default_value;
		}

		bool in_public_lobby()
		{
			return lobby_menu_entered && !get_dvar_bool("xblive_privatematch") && !get_dvar_bool("systemlink") &&
				is_lobby_host();
		}

		bool is_in_match()
		{
			return game::CL_IsCgameInitialized() && !game::VirtualLobby_Loaded();
		}

		bool is_fake(const std::uint64_t xuid)
		{
			return std::find(fake_xuids.begin(), fake_xuids.end(), xuid) != fake_xuids.end();
		}

		bool is_active(std::uint8_t* m)
		{
			return field<std::uint8_t>(m, member::status) >= status_in_lobby;
		}

		bool is_fake_member(std::uint8_t* m)
		{
			return is_active(m) && is_fake(field<std::uint64_t>(m, member::xuid));
		}

		void mark_party_dirty()
		{
			++*reinterpret_cast<std::uint16_t*>(0x1421E4998);
			*reinterpret_cast<std::uint8_t*>(0x1421E4988) = 1;
		}

		std::string get_member_name(std::uint8_t* m)
		{
			const auto* name = reinterpret_cast<const char*>(m + member::name);
			return {name, strnlen(name, member::name_size)};
		}

		std::uint8_t* find_fake_member(const std::string& name)
		{
			for (auto i = 0; i < max_members; ++i)
			{
				auto* m = get_member(i);
				if (is_fake_member(m) && get_member_name(m) == name)
				{
					return m;
				}
			}

			return nullptr;
		}

		bool is_name_taken(const std::string& name)
		{
			for (auto i = 0; i < max_members; ++i)
			{
				auto* m = get_member(i);
				if (is_active(m) && utils::string::to_lower(get_member_name(m)) == utils::string::to_lower(name))
				{
					return true;
				}
			}

			return false;
		}

		std::string pick_name()
		{
			for (auto attempt = 0; attempt < 20; ++attempt)
			{
				const auto* name = game::SV_BotGetRandomName();
				const auto short_name = name ? std::string(name).substr(0, 15) : std::string{};
				if (!short_name.empty() && !is_name_taken(short_name))
				{
					return short_name;
				}
			}

			for (auto i = 1;; ++i)
			{
				auto name = utils::string::va("Player%d", random_int(100, 9999) + i);
				if (!is_name_taken(name))
				{
					return name;
				}
			}
		}

		void roll_rank(int& rank, int& prestige)
		{
			const auto roll = random_int(0, 99);
			if (roll < 35)
			{
				prestige = 0;
				rank = random_int(0, 49);
			}
			else if (roll < 70)
			{
				prestige = random_int(1, 4);
				rank = random_int(0, 49);
			}
			else if (roll < 92)
			{
				prestige = random_int(5, 8);
				rank = random_int(0, 49);
			}
			else
			{
				prestige = random_int(9, 10);
				rank = random_int(20, 49);
			}
		}

		std::uint8_t* find_host_member()
		{
			for (auto i = 0; i < max_members; ++i)
			{
				auto* m = get_member(i);
				if (is_active(m) && !is_fake(field<std::uint64_t>(m, member::xuid)))
				{
					return m;
				}
			}

			return nullptr;
		}

		int count_members()
		{
			auto count = 0;
			for (auto i = 0; i < max_members; ++i)
			{
				if (is_active(get_member(i)))
				{
					++count;
				}
			}

			return count;
		}

		std::atomic_bool setup_team_based{true};

		int get_setup_bots()
		{
			const auto* rules = public_lobby::get_match_rules();
			if (!rules)
			{
				return -1;
			}

			const int friendly = rules[0x2F];
			const int enemy = rules[0x30];
			return setup_team_based ? friendly + enemy : friendly;
		}

		int get_target_members()
		{
			auto target = lobby_bots_max ? lobby_bots_max->current.integer : 0;
			if (target <= 0)
			{
				const auto bots = get_setup_bots();
				target = bots >= 0 ? bots + 1 : get_dvar_int("party_maxplayers", 12);
			}

			return std::clamp(target, 1, max_members);
		}

		std::vector<std::uint8_t> generated_costumes{};
		int generated_costume_count = 0;
		bool costumes_generated = false;

		std::unordered_map<std::uint64_t, std::vector<std::uint8_t>> assigned_costumes{};
		std::vector<int> costume_order{};

		void generate_costumes()
		{
			costumes_generated = true;
			generated_costumes.assign(18 * member::costume_size, 0);
			generated_costume_count = std::clamp(utils::hook::invoke<int>(0x140498790, generated_costumes.data(), 18), 0, 18);
			costume_order.clear();
			write_log("[lobby_bots] generated %d bot costumes\n", generated_costume_count);
		}

		void copy_random_costume(std::uint8_t* target)
		{
			if (!costumes_generated)
			{
				generate_costumes();
			}

			if (generated_costume_count <= 0)
			{
				return;
			}

			if (costume_order.empty())
			{
				for (auto i = 0; i < generated_costume_count; ++i)
				{
					costume_order.push_back(i);
				}

				for (auto i = static_cast<int>(costume_order.size()) - 1; i > 0; --i)
				{
					std::swap(costume_order[i], costume_order[random_int(0, i)]);
				}
			}

			const auto index = costume_order.back();
			costume_order.pop_back();
			std::memcpy(target, generated_costumes.data() + index * member::costume_size, member::costume_size);
		}

		void apply_loot_clothes(std::uint8_t* costume)
		{
			static const std::pair<int, std::uint32_t> categories[] =
			{
				{1, 0x62},
				{3, 0x64},
				{4, 0x69},
				{5, 0x6A},
				{6, 0x68},
				{7, 0x67},
				{8, 0x66},
				{9, 0x6C},
				{10, 0x6B},
			};

			std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> owned{};
			for (const auto guid : supply_drops::get_inventory())
			{
				owned[guid >> 20].push_back(guid);
			}

			static std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> all{};
			if (all.empty())
			{
				for (const auto guid : supply_drops::get_loot_items())
				{
					all[guid >> 20].push_back(guid);
				}
			}

			auto applied = 0;
			for (const auto& [slot, category] : categories)
			{
				if (random_int(0, 99) >= 70)
				{
					continue;
				}

				const auto max_index = *reinterpret_cast<std::uint16_t*>(0x140EB3700ull + slot * 4 + 2);

				std::vector<std::uint32_t> candidates{};
				for (const auto* list : {&owned, &all})
				{
					const auto entry = list->find(category);
					if (entry == list->end())
					{
						continue;
					}

					for (const auto guid : entry->second)
					{
						const auto index = guid & 0x1FFF;
						if (index >= 1 && index <= max_index)
						{
							candidates.push_back(guid);
						}
					}

					if (!candidates.empty())
					{
						break;
					}
				}

				if (candidates.empty())
				{
					continue;
				}

				const auto guid = candidates[random_int(0, static_cast<int>(candidates.size()) - 1)];
				const auto value = ((guid >> 13) & 0x7F) << 16 | (guid & 0x1FFF);
				std::memcpy(costume + slot * 4, &value, 4);
				++applied;
			}

			write_log("[lobby_bots] loot clothes: %d pieces\n", applied);
		}

		void keep_costumes()
		{
			auto changed = false;
			for (auto i = 0; i < max_members; ++i)
			{
				auto* m = get_member(i);
				if (!is_fake_member(m))
				{
					continue;
				}

				const auto entry = assigned_costumes.find(field<std::uint64_t>(m, member::xuid));
				if (entry == assigned_costumes.end() || entry->second.size() != member::costume_size)
				{
					continue;
				}

				if (std::memcmp(m + member::costume, entry->second.data(), member::costume_size) != 0)
				{
					std::memcpy(m + member::costume, entry->second.data(), member::costume_size);
					changed = true;
				}
			}

			if (changed)
			{
				mark_party_dirty();
			}
		}

		bool add_fake_member()
		{
			auto* host = find_host_member();
			if (!host)
			{
				return false;
			}

			auto host_index = 0;
			for (auto i = 0; i < max_members; ++i)
			{
				if (get_member(i) == host)
				{
					host_index = i;
					break;
				}
			}

			std::uint8_t* slot = nullptr;
			for (auto pass = 0; pass < 2 && !slot; ++pass)
			{
				for (auto i = pass == 0 ? host_index + 1 : 0; i < max_members; ++i)
				{
					auto* m = get_member(i);
					if (field<std::uint8_t>(m, member::status) == 0)
					{
						slot = m;
						break;
					}
				}
			}

			if (!slot)
			{
				return false;
			}

			std::memcpy(slot, host, member_size);

			std::uint64_t xuid{};
			do
			{
				xuid = 0x0110000100000000ull | static_cast<std::uint32_t>(rng()());
			}
			while (!xuid || is_fake(xuid));

			const auto name = pick_name();
			int rank{}, prestige{};
			roll_rank(rank, prestige);

			field<std::uint64_t>(slot, member::xuid) = xuid;
			field<std::uint64_t>(slot, member::party_id) = (static_cast<std::uint64_t>(rng()()) << 32) | rng()();
			field<int>(slot, member::rank) = rank;
			field<int>(slot, member::prestige) = prestige;
			field<int>(slot, member::rank_icons) = 0;

			copy_random_costume(slot + member::costume);
			apply_loot_clothes(slot + member::costume);
			assigned_costumes[xuid].assign(slot + member::costume, slot + member::costume + member::costume_size);

			field<int>(slot, member::background) = random_int(0, 190);
			field<int>(slot, member::patch) = 0;
			std::memset(slot + member::clan_tag, 0, 5);
			field<int>(slot, member::map_vote) = 0;

			std::memset(slot + member::name, 0, member::name_size);
			std::memcpy(slot + member::name, name.data(), std::min<std::size_t>(name.size(), member::name_size - 1));

			field<int>(slot, member::kd_ratio) = random_int(450, 2200);
			field<int>(slot, member::win_streak) = random_int(0, 6);
			field<int>(slot, member::games_played) = random_int(20, 3000);
			field<int>(slot, member::win_loss) = random_int(500, 1800);
			field<int>(slot, member::time_played) = random_int(3600, 900000);
			field<std::uint16_t>(slot, member::score) = 0;
			field<int>(slot, member::deaths) = 0;
			field<int>(slot, member::team) = 0;

			std::memset(slot + member::address, 0, 0x10);
			field<int>(slot, member::last_packet) = game::Sys_Milliseconds();

			fake_xuids.push_back(xuid);
			vote_times[xuid] = game::Sys_Milliseconds() + random_int(3000, 25000);
			field<std::uint8_t>(slot, member::status) = status_in_lobby;

			mark_party_dirty();
			write_log("[lobby_bots] %s joined the lobby (rank %d, prestige %d, costumes %u)\n", name.data(), rank + 1,
			              prestige, *reinterpret_cast<std::uint32_t*>(bot_costume_count));
			return true;
		}

		void remove_fake_member(std::uint8_t* m)
		{
			const auto xuid = field<std::uint64_t>(m, member::xuid);
			write_log("[lobby_bots] %s left the lobby\n", get_member_name(m).data());

			std::memset(m, 0, member_size);
			fake_xuids.erase(std::remove(fake_xuids.begin(), fake_xuids.end(), xuid), fake_xuids.end());
			vote_times.erase(xuid);
			assigned_costumes.erase(xuid);
			mark_party_dirty();
		}

		void remove_all_fake_members()
		{
			for (auto i = 0; i < max_members; ++i)
			{
				auto* m = get_member(i);
				if (is_fake_member(m))
				{
					remove_fake_member(m);
				}
			}

			fake_xuids.clear();
			vote_times.clear();
		}

		void schedule_next_join(const bool first)
		{
			const auto min = lobby_bots_join_min ? lobby_bots_join_min->current.integer : 1000;
			const auto max = lobby_bots_join_max ? lobby_bots_join_max->current.integer : 3000;
			next_join_time = game::Sys_Milliseconds() + (first ? random_int(300, 1200) : random_int(min, max));
		}

		void apply_leaves()
		{
			const auto chance = lobby_bots_leave_chance ? lobby_bots_leave_chance->current.integer : 0;
			for (auto i = 0; i < max_members; ++i)
			{
				auto* m = get_member(i);
				if (is_fake_member(m) && random_int(0, 99) < chance)
				{
					remove_fake_member(m);
				}
			}
		}

		void update_votes()
		{
			const auto now = game::Sys_Milliseconds();
			for (auto i = 0; i < max_members; ++i)
			{
				auto* m = get_member(i);
				if (!is_fake_member(m))
				{
					continue;
				}

				const auto xuid = field<std::uint64_t>(m, member::xuid);
				const auto itr = vote_times.find(xuid);
				if (itr == vote_times.end() || !itr->second || now < itr->second)
				{
					continue;
				}

				itr->second = 0;
				const auto roll = random_int(0, 99);
				field<int>(m, member::map_vote) = roll < 42 ? 1 : roll < 84 ? 2 : roll < 95 ? 3 : 0;
				mark_party_dirty();
			}
		}

		void reset_votes()
		{
			const auto now = game::Sys_Milliseconds();
			for (auto i = 0; i < max_members; ++i)
			{
				auto* m = get_member(i);
				if (is_fake_member(m))
				{
					field<int>(m, member::map_vote) = 0;
					vote_times[field<std::uint64_t>(m, member::xuid)] = now + random_int(3000, 25000);
				}
			}

			mark_party_dirty();
		}

		void assign_teams()
		{
			int counts[3]{};
			auto team_based = false;

			const auto preferred_team = get_dvar_int("lui_priv_lobby_team", 0);
			if (preferred_team == 1 || preferred_team == 2)
			{
				for (auto i = 0; i < max_members; ++i)
				{
					auto* m = get_member(i);
					if (!is_active(m) || is_fake(field<std::uint64_t>(m, member::xuid)))
					{
						continue;
					}

					auto& team = field<int>(m, member::team);
					if ((team == 1 || team == 2) && team != preferred_team)
					{
						team = preferred_team;
						mark_party_dirty();
					}
				}
			}

			for (auto i = 0; i < max_members; ++i)
			{
				auto* m = get_member(i);
				const auto team = is_active(m) ? field<int>(m, member::team) : 0;
				if (team == 1 || team == 2)
				{
					++counts[team];
					team_based = team_based || !is_fake(field<std::uint64_t>(m, member::xuid));
				}
			}

			if (!team_based)
			{
				return;
			}

			for (auto i = 0; i < max_members; ++i)
			{
				auto* m = get_member(i);
				auto& team = field<int>(m, member::team);
				if (!is_fake_member(m) || team == 1 || team == 2)
				{
					continue;
				}

				team = counts[1] < counts[2] ? 1 : counts[2] < counts[1] ? 2 : random_int(1, 2);
				++counts[team];
				mark_party_dirty();
			}

			for (auto i = max_members - 1; i >= 0 && std::abs(counts[1] - counts[2]) > 1; --i)
			{
				auto* m = get_member(i);
				const auto bigger = counts[1] > counts[2] ? 1 : 2;
				auto& team = field<int>(m, member::team);
				if (!is_fake_member(m) || team != bigger)
				{
					continue;
				}

				team = 3 - bigger;
				--counts[bigger];
				++counts[team];
				mark_party_dirty();
			}
		}

		void update()
		{
			std::lock_guard _(mutex);

			if (!game::environment::is_mp() || !lobby_bots_enabled)
			{
				return;
			}

			const auto in_match = is_in_match();
			const auto host = is_lobby_host();

			if (!host)
			{
				if (get_party_int(0x4178) == 0)
				{
					fake_xuids.clear();
					vote_times.clear();
					assigned_costumes.clear();
					costumes_generated = false;

					if (first_wave_done)
					{
						write_log("[lobby_bots] lobby closed\n");
					}

					first_wave_done = false;
				}

				was_in_match = in_match;
				return;
			}

			const auto now = game::Sys_Milliseconds();
			for (auto i = 0; i < max_members; ++i)
			{
				auto* m = get_member(i);
				if (is_fake_member(m))
				{
					field<int>(m, member::last_packet) = now;
				}
			}

			keep_costumes();

			if (game::VirtualLobby_Loaded() && *reinterpret_cast<std::uint32_t*>(bot_costume_count) != 0)
			{
				*reinterpret_cast<std::uint32_t*>(bot_costume_count) = 0;
				mark_party_dirty();
				write_log("[lobby_bots] bot costume table reset for the lobby\n");
			}

			if (!lobby_bots_enabled->current.enabled)
			{
				if (!fake_xuids.empty())
				{
					remove_all_fake_members();
				}

				was_in_match = in_match;
				return;
			}

			if (in_match)
			{
				was_in_match = true;
				return;
			}

			if (was_in_match)
			{
				was_in_match = false;
				apply_leaves();
				reset_votes();
				schedule_next_join(false);
			}

			if (!in_public_lobby())
			{
				return;
			}

			assign_teams();
			update_votes();

			const auto target = get_target_members();
			const auto members = count_members();

			if (members > target)
			{
				for (auto i = max_members - 1; i >= 0 && count_members() > target; --i)
				{
					auto* m = get_member(i);
					if (is_fake_member(m))
					{
						remove_fake_member(m);
					}
				}

				return;
			}

			if (members >= target)
			{
				next_join_time = 0;
				return;
			}

			if (!first_wave_done)
			{
				if (!find_host_member())
				{
					return;
				}

				first_wave_done = true;
				auto added = 0;
				while (added < 6 && count_members() < target)
				{
					if (!add_fake_member())
					{
						break;
					}

					++added;
				}

				write_log("[lobby_bots] first wave: %d bots (members %d, target %d)\n", added, count_members(), target);

				schedule_next_join(false);
				return;
			}

			if (!next_join_time)
			{
				schedule_next_join(false);
				return;
			}

			if (now < next_join_time)
			{
				return;
			}

			const auto batch = target - count_members() > 2 ? random_int(1, 2) : 1;
			for (auto i = 0; i < batch && count_members() < target; ++i)
			{
				add_fake_member();
			}

			schedule_next_join(false);
		}

		[[maybe_unused]] int connect_string_stub(char* buffer, const char* fmt, const int bot_num, const char* userinfo,
		                        const char* name, const int qport, const char* ctz)
		{
			std::string info = userinfo ? userinfo : "";

			{
				std::lock_guard _(mutex);
				if (auto* m = name ? find_fake_member(name) : nullptr)
				{
					const auto xuid = field<std::uint64_t>(m, member::xuid);
					const std::string empty_xuid = "\\xuid\\\\";
					const auto pos = info.find(empty_xuid);
					if (pos != std::string::npos)
					{
						info.replace(pos, empty_xuid.size(), utils::string::va("\\xuid\\%016llx\\", xuid));
					}
				}
			}

			return utils::hook::invoke<int>(0x14006B240, buffer, fmt, bot_num, info.data(), name, qport, ctz);
		}

		std::vector<std::uint8_t*> get_fake_members()
		{
			std::vector<std::uint8_t*> result{};
			for (auto i = 0; i < max_members; ++i)
			{
				auto* m = get_member(i);
				if (is_fake_member(m))
				{
					result.push_back(m);
				}
			}

			return result;
		}

		std::string get_difficulty()
		{
			static const char* difficulties[] = {"", "recruit", "regular", "hardened", "veteran"};

			auto value = lobby_bots_difficulty ? lobby_bots_difficulty->current.integer : 0;

			if (value == 0)
			{
				if (const auto* rules = public_lobby::get_match_rules())
				{
					const int setup = rules[0x2D];
					value = setup >= 4 ? 5 : setup + 1;
				}
			}

			if (value == 5)
			{
				static const int mixed[] = {4, 4, 3, 3, 2, 2};
				static size_t index = 0;
				static auto last = std::chrono::steady_clock::time_point{};

				const auto now = std::chrono::steady_clock::now();
				if (now - last > std::chrono::seconds(30))
				{
					index = 0;
				}
				last = now;

				value = mixed[index++ % std::size(mixed)];
			}

			return difficulties[std::clamp(value, 0, 4)];
		}

		constexpr auto svs_clients = 0x149B0CF90ull;
		constexpr auto svs_client_count = 0x149B0CF8Cull;
		constexpr auto client_size = 0xA1738;
		constexpr auto client_name = 0x41AB8;
		constexpr auto level_clients = 0x144DA48E0ull;
		constexpr auto gclient_size = 0x5758;
		constexpr auto gclient_ping = 0x51BC;

		struct bot_ping
		{
			int base{};
			int current{};
			int next_change{};
		};

		std::unordered_map<std::string, bot_ping> bot_pings{};

		int roll_ping()
		{
			const auto roll = random_int(0, 99);
			if (roll < 60) return random_int(15, 50);
			if (roll < 90) return random_int(50, 90);
			return random_int(90, 140);
		}

		void update_bot_pings()
		{
			if (!game::SV_Loaded() || game::VirtualLobby_Loaded())
			{
				return;
			}

			std::unique_lock lock(mutex, std::try_to_lock);
			if (!lock.owns_lock())
			{
				return;
			}

			const auto now = game::Sys_Milliseconds();
			const auto count = *reinterpret_cast<int*>(svs_client_count);
			auto* const clients = *reinterpret_cast<std::uint8_t**>(level_clients);
			if (!clients)
			{
				return;
			}

			for (auto i = 0; i < count && i < 32; ++i)
			{
				auto* client = reinterpret_cast<std::uint8_t*>(svs_clients + static_cast<std::size_t>(i) * client_size);
				if (*reinterpret_cast<int*>(client) < 4)
				{
					continue;
				}

				const auto* raw_name = reinterpret_cast<const char*>(client + client_name);
				const std::string name{raw_name, strnlen(raw_name, 32)};
				if (!find_fake_member(name))
				{
					continue;
				}

				auto& ping = bot_pings[name];
				if (!ping.base)
				{
					ping.base = roll_ping();
					ping.current = ping.base;
				}

				if (now >= ping.next_change)
				{
					const auto spread = std::max(3, ping.base / 8);
					ping.current = std::max(5, ping.base + random_int(-spread, spread) + (random_int(0, 99) < 2 ? random_int(20, 60) : 0));
					ping.next_change = now + random_int(800, 2500);
				}

				*reinterpret_cast<std::uint16_t*>(clients + static_cast<std::size_t>(i) * gclient_size + gclient_ping) =
					static_cast<std::uint16_t>(ping.current);
			}
		}

		void gsc_lobbybots()
		{
			std::lock_guard _(mutex);

			const std::string cmd = game::Scr_GetString(0);

			if (cmd == "active")
			{
				game::Scr_AddInt(lobby_bots_enabled && lobby_bots_enabled->current.enabled && !get_fake_members().empty());
			}
			else if (cmd == "count")
			{
				game::Scr_AddInt(static_cast<int>(get_fake_members().size()));
			}
			else if (cmd == "name")
			{
				const auto members = get_fake_members();
				const auto index = game::Scr_GetInt(1);
				game::Scr_AddString(index >= 0 && index < static_cast<int>(members.size())
					? get_member_name(members[index]).data()
					: "");
			}
			else if (cmd == "isbot")
			{
				game::Scr_AddInt(find_fake_member(game::Scr_GetString(1)) != nullptr);
			}
			else if (cmd == "team")
			{
				auto* m = find_fake_member(game::Scr_GetString(1));
				const auto team = m ? field<int>(m, member::team) : 0;
				game::Scr_AddString(team == 1 ? "axis" : team == 2 ? "allies" : "");
			}
			else if (cmd == "rank")
			{
				auto* m = find_fake_member(game::Scr_GetString(1));
				game::Scr_AddInt(m ? field<int>(m, member::rank) : 0);
			}
			else if (cmd == "prestige")
			{
				auto* m = find_fake_member(game::Scr_GetString(1));
				game::Scr_AddInt(m ? field<int>(m, member::prestige) : 0);
			}
			else if (cmd == "costume")
			{
				std::string result{};
				if (auto* m = find_fake_member(game::Scr_GetString(1)))
				{
					for (auto i = 0; i < member::costume_size / 4; ++i)
					{
						if (!result.empty())
						{
							result += ' ';
						}

						result += std::to_string(field<int>(m, member::costume + i * 4));
					}
				}

				write_log("[lobby_bots] costume for %s: %s\n", game::Scr_GetString(1), result.empty() ? "(not found)" : result.data());
				game::Scr_AddString(result.data());
			}
			else if (cmd == "precachecostumes")
			{
				auto* table = reinterpret_cast<std::uint8_t*>(bot_costumes);
				auto index = 0;
				for (auto* m : get_fake_members())
				{
					if (index >= 18)
					{
						break;
					}

					std::memcpy(table + index * member::costume_size, m + member::costume, member::costume_size);
					++index;
				}

				auto& count = *reinterpret_cast<std::uint32_t*>(bot_costume_count);
				count = std::max<std::uint32_t>(count, static_cast<std::uint32_t>(index));

				utils::hook::invoke<void>(0x1404B2F20, 0, 1);
				write_log("[lobby_bots] %d lobby costumes added to the bot costume table (%u)\n", index, count);
				game::Scr_AddInt(index);
			}
			else if (cmd == "log")
			{
				write_log("[lobby_bots] gsc: %s\n", game::Scr_GetString(1));
				game::Scr_AddInt(1);
			}
			else if (cmd == "setpending")
			{
				pending_bot_name = game::Scr_GetString(1);
				write_log("[lobby_bots] creating bot %s\n", pending_bot_name->data());
				game::Scr_AddInt(1);
			}
			else if (cmd == "difficulty")
			{
				game::Scr_AddString(get_difficulty().data());
			}
			else if (cmd == "setstats")
			{
				if (auto* m = find_fake_member(game::Scr_GetString(1)))
				{
					field<std::uint16_t>(m, member::score) = static_cast<std::uint16_t>(std::clamp(game::Scr_GetInt(2), 0, 0xFFFF));
					field<int>(m, member::deaths) = std::max(game::Scr_GetInt(3), 0);

					mark_party_dirty();
				}
			}
		}
	}

	bool is_public_lobby()
	{
		return in_public_lobby();
	}

	std::optional<std::string> take_pending_bot_name()
	{
		std::lock_guard _(mutex);
		auto name = std::move(pending_bot_name);
		pending_bot_name.reset();
		return name;
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

			lobby_bots_enabled = game::Dvar_RegisterBool("lobby_bots", true, game::DVAR_FLAG_SAVED,
			                                             "Bots join the public lobby as players");
			lobby_bots_max = game::Dvar_RegisterInt("lobby_bots_max", 0, 0, max_members, game::DVAR_FLAG_SAVED,
			                                        "Players in the public lobby, bots included (0 = party_maxplayers)");
			lobby_bots_join_min = game::Dvar_RegisterInt("lobby_bots_join_min", 1000, 0, 60000, game::DVAR_FLAG_SAVED,
			                                             "Minimum time in ms between bots joining the lobby");
			lobby_bots_join_max = game::Dvar_RegisterInt("lobby_bots_join_max", 3000, 0, 60000, game::DVAR_FLAG_SAVED,
			                                             "Maximum time in ms between bots joining the lobby");
			lobby_bots_leave_chance = game::Dvar_RegisterInt("lobby_bots_leave_chance", 15, 0, 100, game::DVAR_FLAG_SAVED,
			                                                 "Chance (%) of each bot leaving the lobby after a match");
			lobby_bots_difficulty = game::Dvar_RegisterInt("lobby_bots_difficulty", 0, 0, 5, game::DVAR_FLAG_SAVED,
			                                               "Lobby bot difficulty: 0 game default, 1 recruit, 2 regular, 3 hardened, 4 veteran, 5 mixed");

			command::add("lobbybots_lobbyopened", []
			{
				on_lobby_menu_entered();
			});

			gsc::add_function("lobbybots", gsc_lobbybots);

			scheduler::loop(update, scheduler::pipeline::main, 100ms);
			scheduler::loop(update_bot_pings, scheduler::pipeline::server);

			command::add("lobbybots_teambased", [](const command::params& params)
			{
				if (params.size() >= 2)
				{
					setup_team_based = std::atoi(params.get(1)) != 0;
				}
			});

			command::add("lobbybots_clear", []
			{
				std::lock_guard _(mutex);
				remove_all_fake_members();
			});
		}
	};
}

REGISTER_COMPONENT(lobby_bots::component)
