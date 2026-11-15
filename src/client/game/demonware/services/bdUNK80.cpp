#include <std_include.hpp>
#include "../services.hpp"

#include <component/supply_drops.hpp>

#include <algorithm>
#include <component/auth.hpp>

namespace demonware
{
	namespace
	{
		constexpr uint32_t entitlement_items[] =
		{
			0x10001AC, 0x10001A7, 0x10001AA, 0x10001AB, 0x10001AE, 0x10001A9, 0x1000209, 0x10002B2,
			0x1000347, 0x1000348, 0x10001A8, 0x10001C6, 0x10001D4, 0x10001D5, 0x10001EE, 0x10001EF,
			0x10001F0, 0x10001F1, 0x10001F2, 0x10001F3, 0x10001F4, 0x10001F5, 0x10001F6, 0x10001F7,
			0x10001F8, 0x10001F9, 0x10001FA, 0x10001FB, 0x10001FC, 0x10001FD, 0x10001FE, 0x10001FF,
			0x1000200, 0x1000201, 0x1000202, 0x1000203, 0x1000204, 0x1000205, 0x1000207, 0x100020F,
			0x1000210, 0x1000211, 0x1000212, 0x1000213, 0x1000214, 0x1000254, 0x1000256, 0x1000255,
			0x1000257, 0x1000258, 0x100025A, 0x1000259, 0x100025B, 0x1000245, 0x1000246, 0x1000247,
			0x100022D, 0x100022E, 0x1000230, 0x1000231, 0x1000232, 0x1000233, 0x1000234, 0x1000235,
			0x1000275, 0x1000276, 0x1000277, 0x1000278, 0x1000279, 0x100027A, 0x100027B, 0x100027C,
			0x100027D, 0x100027E, 0x100027F, 0x1000280, 0x1000281, 0x1000282, 0x1000283, 0x1000284,
			0x1000285, 0x1000286, 0x1000287, 0x1000288, 0x1000289, 0x100028A, 0x100028B, 0x100028C,
			0x100028D, 0x100028E, 0x100028F, 0x1000290, 0x1000291, 0x1000292, 0x1000293, 0x1000294,
			0x1000295, 0x1000296, 0x1000297, 0x1000298, 0x1000299, 0x100029A, 0x100029B, 0x100029C,
			0x10002AE, 0x10002AF, 0x10002B0, 0x10002B1, 0x10002B5, 0x10002B6, 0x10002B7, 0x10002B8,
			0x10002B9, 0x10002BA, 0x10002BB, 0x10002BC, 0x10002BD, 0x10002BE, 0x10002C5, 0x10002C6,
			0x10002C7, 0x10002C8, 0x10002C9, 0x10002CA, 0x10002CB, 0x10002CC, 0x10002CD, 0x10002CE,
			0x10002CF, 0x10002D0, 0x10002D1, 0x10002D2, 0x10002D3, 0x10002D4, 0x10002D5, 0x10002D6,
			0x10002D7, 0x10002D8, 0x10002C0, 0x10002C1, 0x10002C2, 0x10002C3, 0x10002C4, 0x100030B,
			0x10002BF, 0x1000306, 0x1000307, 0x1000308, 0x1000309, 0x100030A, 0x1000314, 0x1000315,
			0x1000349, 0x1000393, 0x80000AD,
		};

		constexpr uint32_t no_expiration_u32 = 0xFFFFFFFF;
		constexpr int64_t no_expiration = 0x7FFFFFFFFFFFFFFF;

		class bdCouponBatchCount final : public bdTaskResult
		{
		public:
			uint32_t coupon_type{};
			uint64_t coupon_batch_id{};
			uint32_t claimed_count{};
			uint32_t pending_count{};

			void serialize(byte_buffer* buffer) override
			{
				buffer->write_uint32(this->coupon_type);
				buffer->write_uint64(this->coupon_batch_id);
				buffer->write_uint32(this->claimed_count);
				buffer->write_uint32(this->pending_count);
			}
		};

		class bdCoupon final : public bdTaskResult
		{
		public:
			std::string coupon_id{};
			bool claimed{};
			uint32_t sku_id{};
			std::string item_data{};

			void serialize(byte_buffer* buffer) override
			{
				const auto user_id = auth::get_guid();

				buffer->write_string(this->coupon_id);
				buffer->write_uint64(user_id);
				buffer->write_string("steam");
				buffer->write_bool(this->claimed);
				buffer->write_uint32(this->sku_id);
				buffer->write_uint64(user_id);
				buffer->write_string("steam");
				buffer->write_uint32(0);
				buffer->write_uint32(0);
				buffer->write_byte(0);
				buffer->write_uint32(0);
				buffer->write_int32(0);
				buffer->write_blob(this->item_data);
			}
		};

		class bdMarketplaceProduct final : public bdTaskResult
		{
		public:
			uint32_t sku_id{};
			std::vector<uint32_t> items{};

			void serialize(byte_buffer* buffer) override
			{
				buffer->write_uint32(this->sku_id);
				buffer->write_blob("");
				buffer->write_blob("");
				buffer->write_blob("");
				buffer->write_uint16(0);
				buffer->write_uint32(0);

				buffer->write_uint32(static_cast<uint32_t>(this->items.size()));
				for (const auto item : this->items)
				{
					buffer->write_uint32(item);
					buffer->write_uint32(1);
				}

				buffer->write_uint32(0);
			}
		};

		class bdMarketplaceInventory final : public bdTaskResult
		{
		public:
			uint32_t item_id{};
			uint32_t quantity{1};
			std::string item_data{};

			void serialize(byte_buffer* buffer) override
			{
				buffer->write_uint64(auth::get_guid());
				buffer->write_string("steam");
				buffer->write_uint32(this->item_id);
				buffer->write_uint32(this->quantity);
				buffer->write_uint32(0);
				buffer->write_blob(this->item_data);
				buffer->write_uint32(no_expiration_u32);
				buffer->write_int64(no_expiration);
				buffer->write_uint16(0);
				buffer->write_uint32(0);
			}
		};

		std::string get_item_data(const supply_drops::batch& batch)
		{
			auto item_data = (batch.type == 4 ? "MTXRewardRoll-" : "activityReward-") + std::to_string(batch.id);
			item_data.push_back('\0');
			return item_data;
		}

		std::vector<std::string> read_coupon_ids(byte_buffer* buffer)
		{
			std::string title{};
			uint32_t count{};
			buffer->read_string(&title);
			buffer->read_uint32(&count);

			std::vector<std::string> coupon_ids{};
			for (uint32_t i = 0; i < count && i < 64; ++i)
			{
				std::string coupon_id{};
				if (!buffer->read_string(&coupon_id))
				{
					break;
				}

				coupon_ids.push_back(coupon_id);
			}

			return coupon_ids;
		}
	}

	bdUNK80::bdUNK80() : service(80, "bdUNK80")
	{
		this->register_task(42, &bdUNK80::unk42);
		this->register_task(49, &bdUNK80::unk49);
		this->register_task(60, &bdUNK80::unk60);
		this->register_task(130, &bdUNK80::unk130);
		this->register_task(165, &bdUNK80::get_inventory);
		this->register_task(193, &bdUNK80::unk193);
		this->register_task(194, &bdUNK80::get_coupons);
		this->register_task(195, &bdUNK80::get_coupons);
		this->register_task(150, &bdUNK80::claim_coupons);
		this->register_task(155, &bdUNK80::redeem_coupons);
		this->register_task(197, &bdUNK80::consume_item);
		this->register_task(163, &bdUNK80::set_item_data);
		this->register_task(146, &bdUNK80::delete_items);
	}

	void bdUNK80::delete_items(service_server* server, byte_buffer* buffer) const
	{
		std::string title{};
		uint32_t count{};
		buffer->read_string(&title);
		buffer->read_uint32(&count);

		for (uint32_t i = 0; i < count && i < 64; ++i)
		{
			uint32_t item{};
			uint32_t quantity{};
			uint32_t unknown{};
			if (!buffer->read_uint32(&item) || !buffer->read_uint32(&quantity) || !buffer->read_uint32(&unknown))
			{
				break;
			}

			const auto removed = supply_drops::remove_item(item);
			dw_log(utils::string::va("  -> delete item 0x%X x%u (%s)", item, quantity, removed ? "removed" : "not owned"));
		}

		auto reply = server->create_reply(this->task_id());
		reply->send();
	}

	void bdUNK80::set_item_data(service_server* server, byte_buffer* buffer) const
	{
		std::string title{};
		uint64_t user{};
		std::string platform{};
		uint32_t item{};
		std::string data{};
		buffer->read_string(&title);
		buffer->read_uint64(&user);
		buffer->read_string(&platform);
		buffer->read_uint32(&item);
		buffer->read_blob(&data);

		supply_drops::set_item_data(item, data);
		dw_log(utils::string::va("  -> item data 0x%X (%zu bytes)", item, data.size()));

		auto reply = server->create_reply(this->task_id());
		reply->send();
	}

	void bdUNK80::consume_item(service_server* server, byte_buffer* buffer) const
	{
		std::string title{};
		uint32_t item{};
		uint8_t unknown{};
		uint32_t currency{};
		buffer->read_string(&title);
		buffer->read_uint32(&item);
		buffer->read_byte(&unknown);
		buffer->read_uint32(&currency);

		constexpr uint32_t xp_values[] = {500, 1000, 2000, 3500, 5000};
		constexpr uint32_t codpoint_values[] = {100, 300, 1000, 2000, 0};

		const auto rarity = std::clamp(supply_drops::get_item_rarity(item), 0, 4);
		const auto amount = currency == 2 ? codpoint_values[rarity] : xp_values[rarity];

		const auto removed = supply_drops::remove_item(item);
		const auto balance = supply_drops::add_currency(static_cast<uint8_t>(currency), removed ? amount : 0);

		dw_log(utils::string::va("  -> consume item 0x%X (%s), currency %u +%u = %u", item,
		                         removed ? "removed" : "not owned", currency, removed ? amount : 0, balance));

		auto reply = server->create_reply(this->task_id());
		reply->send();

		{
			byte_buffer push{};
			push.write_uint32(52);
			push.write_uint64(auth::get_guid());
			push.write_string("steam");
			push.write_uint32(1);
			push.write_byte(static_cast<char>(currency));
			push.write_uint32(balance);
			remote_reply(server, 2).send(&push, true);
		}

		{
			byte_buffer push{};
			push.write_uint32(49);
			push.write_uint64(auth::get_guid());
			push.write_string("steam");
			push.write_string("aw_pc");
			push.write_uint32(1);
			push.write_uint32(item);
			push.write_uint32(0);
			push.write_uint32(0);
			push.write_blob("");
			push.write_uint32(no_expiration_u32);
			push.write_int64(no_expiration);
			push.write_uint16(0);
			push.write_uint32(0);
			remote_reply(server, 2).send(&push, true);
		}
	}

	bool bdUNK80::exec_missing_task(service_server* server, byte_buffer* buffer)
	{
		const auto data = buffer->get_remaining();

		std::vector<uint32_t> skus{};
		for (size_t i = 0; i + sizeof(uint32_t) <= data.size(); ++i)
		{
			uint32_t value{};
			std::memcpy(&value, data.data() + i, sizeof(value));

			if (std::find(skus.begin(), skus.end(), value) == skus.end() && supply_drops::is_pending_sku(value))
			{
				skus.push_back(value);
			}
		}

		if (skus.empty())
		{
			return false;
		}

		auto reply = server->create_reply(this->task_id());

		for (const auto sku : skus)
		{
			auto* product = new bdMarketplaceProduct;
			product->sku_id = sku;
			product->items = {sku};
			reply->add(product);

			dw_log(utils::string::va("  -> product sku 0x%X", sku));
		}

		reply->send();
		return true;
	}

	void bdUNK80::unk42(service_server* server, byte_buffer* /*buffer*/) const
	{
		// TODO:
		auto reply = server->create_reply(this->task_id());
		reply->send();
	}

	void bdUNK80::unk49(service_server* server, byte_buffer* /*buffer*/) const
	{
		// TODO:
		auto reply = server->create_reply(this->task_id());
		reply->send();
	}

	void bdUNK80::unk60(service_server* server, byte_buffer* /*buffer*/) const
	{
		// TODO:
		auto reply = server->create_reply(this->task_id());
		reply->send();
	}

	void bdUNK80::unk130(service_server* server, byte_buffer* /*buffer*/) const
	{
		// TODO:
		auto reply = server->create_reply(this->task_id());
		reply->send();
	}

	void bdUNK80::get_inventory(service_server* server, byte_buffer* buffer) const
	{
		std::string title{};
		uint32_t page{};
		uint32_t page_size{};
		buffer->read_string(&title);
		buffer->read_uint32(&page);
		buffer->read_uint32(&page_size);

		page = std::max(page, 1u);
		if (!page_size)
		{
			page_size = 100;
		}

		auto reply = server->create_reply(this->task_id());

		std::vector<std::pair<uint32_t, uint32_t>> inventory{};
		for (const auto guid : supply_drops::get_inventory())
		{
			inventory.emplace_back(guid, 1);
		}

		for (const auto guid : entitlement_items)
		{
			if (std::none_of(inventory.begin(), inventory.end(), [guid](const auto& entry) { return entry.first == guid; }))
			{
				inventory.emplace_back(guid, 1);
			}
		}

		const auto slot_items = supply_drops::get_armory_slot_items();
		if (slot_items)
		{
			inventory.emplace_back(supply_drops::armory_slot_item, slot_items);
		}

		const auto start = static_cast<size_t>(page - 1) * page_size;

		for (auto i = start; i < inventory.size() && i < start + page_size; ++i)
		{
			auto* item = new bdMarketplaceInventory;
			item->item_id = inventory[i].first;
			item->quantity = inventory[i].second;
			item->item_data = supply_drops::get_item_data(inventory[i].first);
			reply->add(item);
		}

		dw_log(utils::string::va("  -> inventory page %u: %zu items in total", page, inventory.size()));

		reply->send();
	}

	void bdUNK80::unk193(service_server* server, byte_buffer* buffer) const
	{
		std::string title{};
		uint16_t max_results{};
		uint32_t type{};
		buffer->read_string(&title);
		buffer->read_uint16(&max_results);
		buffer->read_uint32(&type);

		auto reply = server->create_reply(this->task_id());

		for (const auto& group : supply_drops::get_groups(type, std::min<size_t>(max_results ? max_results : 3, 3)))
		{
			auto* result = new bdCouponBatchCount;
			result->coupon_type = group.type;
			result->coupon_batch_id = group.id;
			result->claimed_count = group.claimed;
			result->pending_count = group.pending;
			reply->add(result);

			dw_log(utils::string::va("  -> coupon batch type %u id %llu pending %u", result->coupon_type,
			                         result->coupon_batch_id, result->pending_count));
		}

		if (type == 4)
		{
			const auto count = static_cast<uint32_t>(supply_drops::count_unopened_packages(type));
			reply->set_total(count);
			dw_log(utils::string::va("  -> advanced supply drops: %u", count));
		}

		reply->send();
	}

	void bdUNK80::get_coupons(service_server* server, byte_buffer* buffer) const
	{
		std::string title{};
		uint16_t max_results{};
		uint32_t count{};
		buffer->read_string(&title);
		buffer->read_uint16(&max_results);
		buffer->read_uint32(&count);

		auto reply = server->create_reply(this->task_id());

		const size_t limit = max_results ? max_results : 36;
		size_t added = 0;

		for (uint32_t i = 0; i < count && i < 64; ++i)
		{
			uint32_t type{};
			uint64_t id{};
			if (!buffer->read_uint32(&type) || !buffer->read_uint64(&id))
			{
				break;
			}

			const auto packages = supply_drops::get_group_packages(type, id);
			size_t group_coupons = 0;
			for (const auto& batch : packages)
			{
				group_coupons += batch.coupons.size();
			}

			if (added + group_coupons > limit)
			{
				dw_log(utils::string::va("  -> batch type %u id %llu skipped (%zu coupons, limit %zu)", type, id,
				                         group_coupons, limit));
				continue;
			}

			for (const auto& batch : packages)
			{

				for (size_t c = 0; c < batch.coupons.size(); ++c)
				{
					auto* coupon = new bdCoupon;
					coupon->coupon_id = batch.coupons[c];
					coupon->claimed = c < batch.claimed;
					coupon->sku_id = c < batch.items.size() ? batch.items[c] : 0;
					coupon->item_data = get_item_data(batch);
					reply->add(coupon);
					++added;

					dw_log(utils::string::va("  -> coupon %s sku 0x%X (%s)", coupon->coupon_id.data(), coupon->sku_id,
					                         coupon->item_data.data()));
				}
			}
		}

		reply->send();
	}

	void bdUNK80::claim_coupons(service_server* server, byte_buffer* buffer) const
	{
		const auto coupon_ids = read_coupon_ids(buffer);

		auto reply = server->create_reply(this->task_id());

		for (const auto& batch : supply_drops::claim_coupons(coupon_ids))
		{
			for (size_t c = 0; c < batch.coupons.size(); ++c)
			{
				const auto& coupon_id = batch.coupons[c];
				if (std::find(coupon_ids.begin(), coupon_ids.end(), coupon_id) == coupon_ids.end())
				{
					continue;
				}

				auto* coupon = new bdCoupon;
				coupon->coupon_id = coupon_id;
				coupon->claimed = true;
				coupon->sku_id = c < batch.items.size() ? batch.items[c] : 0;
				coupon->item_data = get_item_data(batch);
				reply->add(coupon);

				dw_log(utils::string::va("  -> claimed %s sku 0x%X", coupon->coupon_id.data(), coupon->sku_id));
			}
		}

		reply->send();
	}

	void bdUNK80::redeem_coupons(service_server* server, byte_buffer* buffer) const
	{
		const auto coupon_ids = read_coupon_ids(buffer);
		const auto items = supply_drops::redeem_coupons(coupon_ids);

		for (const auto item : items)
		{
			dw_log(utils::string::va("  -> redeemed item 0x%X", item));
		}

		auto reply = server->create_reply(this->task_id());
		reply->send();

		for (size_t i = 0; i < items.size(); i += 6)
		{
			const auto count = std::min<size_t>(6, items.size() - i);

			byte_buffer push{};
			push.write_uint32(49);
			push.write_uint64(auth::get_guid());
			push.write_string("steam");
			push.write_string("aw_pc");
			push.write_uint32(static_cast<uint32_t>(count));

			for (size_t j = i; j < i + count; ++j)
			{
				push.write_uint32(items[j]);
				push.write_uint32(1);
				push.write_uint32(0);
				push.write_blob("");
				push.write_uint32(no_expiration_u32);
				push.write_int64(no_expiration);
				push.write_uint16(0);
				push.write_uint32(0);
			}

			remote_reply(server, 2).send(&push, true);
			dw_log(utils::string::va("  -> push 49 (%zu items)", count));
		}
	}
}
