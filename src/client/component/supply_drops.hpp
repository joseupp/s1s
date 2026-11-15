#pragma once

namespace supply_drops
{
	struct batch
	{
		uint32_t type{};
		uint64_t id{};
		uint64_t group{};
		std::vector<std::string> coupons{};
		std::vector<uint32_t> items{};
		uint32_t claimed{};
		bool opened{};
		bool bonus{};
	};

	struct group_info
	{
		uint32_t type{};
		uint64_t id{};
		uint32_t claimed{};
		uint32_t pending{};
	};

	std::vector<group_info> get_groups(uint32_t type, size_t max_results);

	size_t count_unopened_packages(uint32_t type);

	std::vector<batch> get_group_packages(uint32_t type, uint64_t group);

	std::vector<batch> claim_coupons(const std::vector<std::string>& coupon_ids);

	std::vector<uint32_t> redeem_coupons(const std::vector<std::string>& coupon_ids);

	bool is_pending_sku(uint32_t sku);

	std::vector<uint32_t> get_inventory();

	bool remove_item(uint32_t item);

	std::string get_item_data(uint32_t item);
	void set_item_data(uint32_t item, const std::string& data);

	std::vector<uint32_t> get_loot_items();

	int get_item_rarity(uint32_t item);

	uint32_t add_currency(uint8_t currency, uint32_t amount);

	batch add_batch(uint32_t type, size_t coupon_count = 3, bool bonus = false);

	constexpr uint32_t armory_slot_item = 0x1000273;

	uint32_t get_armory_slot_items();
}
