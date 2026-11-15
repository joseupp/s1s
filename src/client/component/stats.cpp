#include <std_include.hpp>
#include "loader/component_loader.hpp"

#include "game/game.hpp"

#include "command.hpp"
#include "console.hpp"

#include <utils/hook.hpp>
#include <utils/io.hpp>

namespace stats
{
	namespace
	{
		game::dvar_t* cg_unlock_all_items;

		utils::hook::detour is_item_unlocked_hook;

		int is_item_unlocked_stub(void* a1, void* a2, void* a3)
		{
			if (cg_unlock_all_items->current.enabled)
			{
				return 0;
			}

			return is_item_unlocked_hook.invoke<int>(a1, a2, a3);
		}

		int is_item_unlocked_stub2(void* a1, void* a2, void* a3, void* a4, void* a5)
		{
			if (cg_unlock_all_items->current.enabled)
			{
				return 0;
			}

			return utils::hook::invoke<int>(0x14041AF40, a1, a2, a3, a4, a5);
		}

		game::dvar_t* cg_unlock_all_items2;

		utils::hook::detour is_item_unlocked_hook3;

		bool is_paid_item_type(const int type)
		{
			return type >= 1 && type <= 3;
		}

		bool is_always_unlocked_type(const int type)
		{
			return type == 3;
		}

		const char* get_item_column(void* item, const int column)
		{
			return utils::hook::invoke<const char*>(0x14041BCA0, item, column);
		}

		bool is_entitlement_item(void* item)
		{
			const auto* type = get_item_column(item, 52);
			return type && (std::strcmp(type, "entitlement") == 0 || std::strcmp(type, "atvi_entitlement") == 0);
		}

		bool is_forced_unlocked(void* item, const int type)
		{
			return is_always_unlocked_type(type) || (type == 2 && is_entitlement_item(item));
		}

		int is_item_unlocked_stub3(void* a1)
		{
			const auto type = is_item_unlocked_hook3.invoke<int>(a1);
			if (is_forced_unlocked(a1, type) || (cg_unlock_all_items2->current.enabled && is_paid_item_type(type)))
			{
				return 0;
			}

			return type;
		}

		utils::hook::detour is_default_item_hook;

		bool is_default_item_stub(void* a1)
		{
			const auto type = is_item_unlocked_hook3.invoke<int>(a1);
			if (is_forced_unlocked(a1, type) || (cg_unlock_all_items2->current.enabled && is_paid_item_type(type)))
			{
				return true;
			}

			return is_default_item_hook.invoke<bool>(a1);
		}

		int is_item_unlocked()
		{
			return 0; // 0 == yes
		}
	}

	class component final : public component_interface
	{
	public:
		void post_unpack() override
		{
			if (game::environment::is_sp())
			{
				return;
			}

			if (game::environment::is_dedi())
			{
				// unlock all items
				utils::hook::jump(0x14041B6B0, is_item_unlocked); // LiveStorage_IsItemUnlockedFromTable_LocalClient
				utils::hook::jump(0x14041AF40, is_item_unlocked); // LiveStorage_IsItemUnlockedFromTable
				utils::hook::jump(0x140418B30, is_item_unlocked); // idk ( unlocks loot etc )
			}
			else
			{
				// unlock all items
				cg_unlock_all_items = game::Dvar_RegisterBool("cg_unlockall_items", false, game::DVAR_FLAG_SAVED, "Whether items should be locked based on the player's stats or always unlocked.");
				cg_unlock_all_items2 = game::Dvar_RegisterBool("cg_unlockall_items2", false, game::DVAR_FLAG_SAVED, "Loot unlock.");
				game::Dvar_RegisterBool("cg_unlockall_classes", false, game::DVAR_FLAG_SAVED, "Whether classes should be locked based on the player's stats or always unlocked.");
			
				is_item_unlocked_hook.create(0x14041B6B0, is_item_unlocked_stub); // LiveStorage_IsItemUnlockedFromTable_LocalClient
				for (const auto address : {0x1403719C2ull, 0x140418950ull, 0x140418B0Bull, 0x140418D5Eull, 0x14041B76Bull})
				{
					utils::hook::call(address, is_item_unlocked_stub2); // LiveStorage_IsItemUnlockedFromTable
				}
				is_item_unlocked_hook3.create(0x140418B30, is_item_unlocked_stub3); // idk (unlocks loot etc)
				is_default_item_hook.create(0x14041AE10, is_default_item_stub);
			}

			command::add("setPlayerDataInt", [](const command::params& params)
			{
				if (params.size() < 2)
				{
					console::info("usage: setPlayerDataInt <data>, <value>\n");
					return;
				}

				// SL_FindString
				const auto lookup_string = game::SL_FindString(params.get(1));
				const auto value = atoi(params.get(2));

				// SetPlayerDataInt
				reinterpret_cast<void(*)(signed int, unsigned int, unsigned int, unsigned int)>(0x14041D550)(
					0, lookup_string, value, 0);
			});

			command::add("getPlayerDataInt", [](const command::params& params)
			{
				if (params.size() < 2)
				{
					console::info("usage: getPlayerDataInt <data>\n");
					return;
				}

				// SL_FindString
				const auto lookup_string = game::SL_FindString(params.get(1));

				// GetPlayerDataInt
				const auto result = reinterpret_cast<int(*)(signed int, unsigned int, unsigned int)>(0x14041C820)(
					0, lookup_string, 0);
				console::info("%d\n", result);
			});

			command::add("dumptable", [](const command::params& params)
			{
				if (params.size() < 2)
				{
					console::info("usage: dumptable <mp/statstable.csv>\n");
					return;
				}

				const std::string name = params.get(1);
				const auto* table = game::DB_FindXAssetHeader(game::ASSET_TYPE_STRINGTABLE, name.data(), 0).stringTable;
				if (!table || !table->values)
				{
					console::info("dumptable: %s not found\n", name.data());
					return;
				}

				std::string csv{};
				for (auto row = 0; row < table->rowCount; ++row)
				{
					for (auto column = 0; column < table->columnCount; ++column)
					{
						const auto* value = table->values[row * table->columnCount + column].string;
						if (column > 0)
						{
							csv += ',';
						}
						csv += value ? value : "";
					}
					csv += '\n';
				}

				auto file = name;
				std::replace(file.begin(), file.end(), '/', '_');
				utils::io::write_file("dumps/" + file, csv);
				console::info("dumptable: %s -> dumps/%s (%d x %d)\n", name.data(), file.data(), table->rowCount,
				              table->columnCount);
			});

			command::add("unlockstats", []()
			{
				command::execute("setPlayerDataInt prestige 30");
				command::execute("setPlayerDataInt experience 1002100");
			});
		}
	};
}

REGISTER_COMPONENT(stats::component)
