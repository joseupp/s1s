#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "game/game.hpp"
#include "game/dvars.hpp"

#include "command.hpp"
#include "console.hpp"
#include "dvars.hpp"
#include "fastfiles.hpp"
#include "filesystem.hpp"
#include "scheduler.hpp"

#include <utils/hook.hpp>
#include <utils/io.hpp>
#include <utils/string.hpp>

#include <version.hpp>

namespace patches
{
	namespace
	{
		constexpr auto overhead_scale = 1.5f;

		float overhead_rank_text_scale_stub(void* font, const float scale)
		{
			static const auto* rank_size = game::Dvar_FindVar("cg_overheadRankSize");

			auto factor = overhead_scale;
			if (rank_size && rank_size->reset.value > 0.0f)
			{
				factor = overhead_scale * rank_size->current.value / rank_size->reset.value;
			}

			return utils::hook::invoke<float>(0x140606270, font, scale * factor);
		}

		constexpr auto overhead_rank_number_scale = 1.35f;

		float overhead_rank_number_scale_stub(void* font, const float scale)
		{
			return overhead_rank_text_scale_stub(font, scale * overhead_rank_number_scale);
		}

		constexpr auto overhead_icon_scale = 0.72f;

		void overhead_icon_draw_stub(const float x, const float y, const float w, const float h, const float s0,
		                             const float t0, const float s1, const float t1, const float* color, void* material)
		{
			const auto new_w = w * overhead_icon_scale;
			const auto new_h = h * overhead_icon_scale;
			utils::hook::invoke<void>(0x140258130, x + (w - new_w) * 0.5f, y + (h - new_h) * 0.5f, new_w, new_h, s0, t0,
				s1, t1, color, material);
		}

		utils::hook::detour live_get_local_client_name_hook;

		const char* live_get_local_client_name()
		{
			return game::Dvar_FindVar("name")->current.string;
		}

		utils::hook::detour sv_kick_client_num_hook;

		void sv_kick_client_num(const int client_num, const char* reason)
		{
			// Don't kick bot to equalize team balance.
			if (reason == "EXE_PLAYERKICKED_BOT_BALANCE"s)
			{
				return;
			}
			return sv_kick_client_num_hook.invoke<void>(client_num, reason);
		}

		std::string get_login_username()
		{
			char username[UNLEN + 1];
			DWORD username_len = UNLEN + 1;
			if (!GetUserNameA(username, &username_len))
			{
				return "HoverSlam";
			}

			return std::string{username, username_len - 1};
		}

		utils::hook::detour com_register_dvars_hook;

		std::string get_saved_name()
		{
			std::string data{};
			if (!utils::io::read_file("players2/config_mp.cfg", &data))
			{
				return {};
			}

			const std::string key = "seta name \"";
			const auto start = data.find(key);
			if (start == std::string::npos)
			{
				return {};
			}

			const auto value_start = start + key.size();
			const auto value_end = data.find('"', value_start);
			if (value_end == std::string::npos || value_end == value_start)
			{
				return {};
			}

			return data.substr(value_start, value_end - value_start);
		}

		void com_register_dvars_stub()
		{
			if (game::environment::is_mp())
			{
				// Make name save
				auto player_name = get_saved_name();
				if (player_name.empty())
				{
					player_name = get_login_username();
				}

				game::Dvar_RegisterString("name", player_name.data(), game::DVAR_FLAG_SAVED, "Player name.");

				// Disable data validation error popup
				game::Dvar_RegisterInt("data_validation_allow_drop", 0, 0, 0, game::DVAR_FLAG_NONE, "");
			}

			return com_register_dvars_hook.invoke<void>();
		}

		game::dvar_t* register_com_maxfps_stub(const char* name, int /*value*/, int /*min*/, int /*max*/, const unsigned int /*flags*/, const char* description)
		{
			return game::Dvar_RegisterInt(name, 0, 0, 1000, game::DVAR_FLAG_SAVED, description);
		}

		game::dvar_t* register_cg_fov_stub(const char* name, float value, float min, float /*max*/, const unsigned int /*flags*/, const char* description)
		{
			return game::Dvar_RegisterFloat(name, value, min, 160, game::DVAR_FLAG_SAVED, description);
		}

		game::dvar_t* register_fovscale_stub(const char* name, float /*value*/, float /*min*/, float /*max*/, unsigned int /*flags*/, const char* desc)
		{
			return game::Dvar_RegisterFloat(name, 1.0f, 0.2f, 5.0f, game::DVAR_FLAG_SAVED, desc);
		}

		int dvar_command_patch() // game makes this return an int and compares with eax instead of al -_-
		{
			const command::params args{};

			if (args.size() <= 0)
				return 0;

			auto* dvar = game::Dvar_FindVar(args.get(0));
			if (dvar)
			{
				if (args.size() == 1)
				{
					const auto* const current = game::Dvar_ValueToString(dvar, dvar->current);
					const auto* const reset = game::Dvar_ValueToString(dvar, dvar->reset);
					console::info("\"%s\" is: \"%s^7\" default: \"%s^7\"\n", dvar->name, current, reset);
					console::info("   %s\n", dvars::dvar_get_domain(dvar->type, dvar->domain).data());
				}
				else
				{
					char command[0x1000] = {0};
					game::Dvar_GetCombinedString(command, 1);
					game::Dvar_SetCommand(args.get(0), command);
				}

				return 1;
			}

			return 0;
		}

		char* db_read_raw_file_stub(const char* filename, char* buf, const int size)
		{
			std::string file_name = filename;
			if (file_name.find(".cfg") == std::string::npos)
			{
				file_name.append(".cfg");
			}

			const auto file = filesystem::file(file_name);
			if (file.exists())
			{
				snprintf(buf, size, "%s\n", file.get_buffer().data());
				return buf;
			}

			return game::DB_ReadRawFile(filename, buf, size);
		}

		void aim_assist_add_to_target_list(void* a1, void* a2)
		{
			if (!dvars::aimassist_enabled->current.enabled)
				return;

			game::AimAssist_AddToTargetList(a1, a2);
		}

		void missing_content_error_stub(int /*mode*/, const char* /*message*/)
		{
			game::Com_Error(game::ERR_DROP, utils::string::va("MISSING FILE\n%s.ff", fastfiles::get_current_fastfile().data()));
		}

		void bsp_sys_error_stub(const char* error, const char* arg1)
		{
			if (game::environment::is_dedi())
			{
				game::Sys_Error(error, arg1);
			}
			else
			{
				scheduler::once([]()
				{
					command::execute("reconnect");
				}, scheduler::pipeline::main, 1s);
				game::Com_Error(game::ERR_DROP, error, arg1);
			}
		}

		void set_client_dvar_from_server_stub(void* a1, void* a2, const char* dvar, const char* value)
		{
			if (utils::string::to_lower(dvar) == "cg_fov")
			{
				return;
			}

			// CG_SetClientDvarFromServer
			reinterpret_cast<void(*)(void*, void*, const char*, const char*)>(0x14021C2B0)(a1, a2, dvar, value);
		}

		utils::hook::detour cmd_lui_notify_server_hook;
		void cmd_lui_notify_server_stub(game::mp::gentity_s* ent)
		{
			command::params_sv params{};
			const auto menu_id = atoi(params.get(1));
			const auto client = &game::mp::svs_clients[ent->s.number];

			// 22 => "end_game"
			if (menu_id == 22 && client->header.remoteAddress.type != game::NA_LOOPBACK)
			{
				return;
			}

			cmd_lui_notify_server_hook.invoke<void>(ent);
		}
	}

	class component final : public component_interface
	{
	public:
		void post_unpack() override
		{
			// Increment ref-count on these
			LoadLibraryA("PhysXDevice64.dll");
			LoadLibraryA("PhysXUpdateLoader64.dll");

			// Register dvars
			com_register_dvars_hook.create(SELECT_VALUE(0x1402F86F0, 0x14042D200), &com_register_dvars_stub);

			// Unlock fps in main menu
			utils::hook::set<BYTE>(SELECT_VALUE(0x140144F5B, 0x14026EFBF), 0xEB);

			// Unlock com_maxfps
			utils::hook::call(SELECT_VALUE(0x1402F8726, 0x14042D2DA), register_com_maxfps_stub);

			// Unlock cg_fov
			utils::hook::call(SELECT_VALUE(0x1400EF830, 0x1400666B6), register_cg_fov_stub);

			// Unlock cg_fovscale
			utils::hook::call(SELECT_VALUE(0x140227599, 0x1400666EB), register_fovscale_stub);

			// Patch Dvar_Command to print out values how CoD4 does it
			utils::hook::jump(SELECT_VALUE(0x1402FB4C0, 0x140430890), dvar_command_patch);

			// Show missing fastfiles
			utils::hook::call(SELECT_VALUE(0x1401817AF, 0x1402CF975), missing_content_error_stub);

			// Allow executing custom cfg files with the "exec" command
			utils::hook::call(SELECT_VALUE(0x1402EE225, 0x14040C6C4), db_read_raw_file_stub);

			// Fix mouse lag
			utils::hook::nop(SELECT_VALUE(0x14038FAFF, 0x14053E68B), 6);
			scheduler::loop([]()
			{
				SetThreadExecutionState(ES_DISPLAY_REQUIRED);
			}, scheduler::pipeline::main);

			// Allow kbam input when gamepad is enabled
			utils::hook::nop(SELECT_VALUE(0x14013EF83, 0x140262503), 2);
			utils::hook::nop(SELECT_VALUE(0x14013CBAC, 0x14025FC74), 6);

			if (game::environment::is_sp())
			{
				patch_sp();
			}
			else
			{
				patch_mp();
			}
		}

		static void patch_mp()
		{
			// Use name dvar
			live_get_local_client_name_hook.create(0x140537A20, &live_get_local_client_name);

			// Patch SV_KickClientNum
			sv_kick_client_num_hook.create(0x140494C60, &sv_kick_client_num);

			// block changing name in-game
			utils::hook::set<uint8_t>(0x140495D70, 0xC3);

			// patch "Couldn't find the bsp for this map." error to not be fatal in mp
			utils::hook::call(0x1402C9FF6, bsp_sys_error_stub);

			// client side aim assist dvar
			dvars::aimassist_enabled = game::Dvar_RegisterBool("aimassist_enabled", true, game::DVAR_FLAG_SAVED, "Enables aim assist for controllers");
			utils::hook::call(0x140054579, aim_assist_add_to_target_list);

			// isProfanity
			utils::hook::set(0x1402989C0, 0xC3C033);

			// disable emblems
			dvars::override::register_int("emblems_active", 0, 0, 0, game::DVAR_FLAG_NONE);
			utils::hook::set<uint8_t>(0x1404D7EF0, 0xC3); // don't register commands

			// disable elite_clan
			dvars::override::register_int("elite_clan_active", 0, 0, 0, game::DVAR_FLAG_NONE);
			utils::hook::set<uint8_t>(0x1405AF570, 0xC3); // don't register commands

			// disable codPointStore
			dvars::override::register_int("codPointStore_enabled", 0, 0, 0, game::DVAR_FLAG_NONE);

			// don't register every replicated dvar as a network dvar
			utils::hook::nop(0x1403AE8CD, 5); // dvar_foreach

			// Patch "Server is on a different version"
			utils::hook::inject(0x14049700F, VERSION);

			// prevent servers overriding our fov
			utils::hook::call(0x14021842C, set_client_dvar_from_server_stub);
			utils::hook::nop(0x14042F06E, 5);
			utils::hook::nop(0x140155276, 5);
			utils::hook::set<uint8_t>(0x1401F7949, 0xEB);

			// some anti tamper thing that kills performance
			dvars::override::register_int("dvl", 0, 0, 0, game::DVAR_FLAG_READ);

			// unlock safeArea_*
			utils::hook::jump(0x140275D22, 0x140275D2B);
			utils::hook::jump(0x140275D45, 0x140275D4E);
			dvars::override::register_float("safeArea_adjusted_horizontal", 1, 0, 1, game::DVAR_FLAG_SAVED);
			dvars::override::register_float("safeArea_adjusted_vertical", 1, 0, 1, game::DVAR_FLAG_SAVED);
			dvars::override::register_float("safeArea_horizontal", 1, 0, 1, game::DVAR_FLAG_SAVED);
			dvars::override::register_float("safeArea_vertical", 1, 0, 1, game::DVAR_FLAG_SAVED);

			dvars::override::scale_float("cg_overheadNamesSize", overhead_scale * 1.35f);
			dvars::override::scale_float("cg_overheadRankSize", overhead_scale);
			dvars::override::scale_float("cg_overheadIconSize", overhead_scale);
			utils::hook::call(0x1401E5262, overhead_rank_text_scale_stub);
			utils::hook::call(0x1401E527C, overhead_rank_number_scale_stub);
			utils::hook::call(0x1401E5701, overhead_icon_draw_stub);

			// move chat position on the screen above menu splashes
			dvars::override::register_vector2("cg_hudChatPosition", 5, 170, 0, 640, game::DVAR_FLAG_SAVED);

			// allow servers to check for new packages more often
			dvars::override::register_int("sv_network_fps", 1000, 20, 1000, game::DVAR_FLAG_SAVED);

			// Massively increate timeouts
			dvars::override::register_int("cl_timeout", 90, 90, 1800, game::DVAR_FLAG_NONE); // Seems unused
			dvars::override::register_int("sv_timeout", 90, 90, 1800, game::DVAR_FLAG_NONE); // 30 - 0 - 1800
			dvars::override::register_int("cl_connectTimeout", 120, 120, 1800, game::DVAR_FLAG_NONE); // Seems unused
			dvars::override::register_int("sv_connectTimeout", 120, 120, 1800, game::DVAR_FLAG_NONE); // 60 - 0 - 1800

			game::Dvar_RegisterInt("scr_game_spectatetype", 1, 0, 99, game::DVAR_FLAG_REPLICATED, "");

			// Prevent clients from ending the game as non host by sending 'end_game' lui notification
			cmd_lui_notify_server_hook.create(0x1403436C0, cmd_lui_notify_server_stub);
		}

		static void patch_sp()
		{
		}
	};
}

REGISTER_COMPONENT(patches::component)
