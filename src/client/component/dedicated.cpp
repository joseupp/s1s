#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "game/game.hpp"

#include "console.hpp"
#include "scheduler.hpp"
#include "server_list.hpp"
#include "network.hpp"
#include "command.hpp"
#include "dvars.hpp"

#include <utils/hook.hpp>
#include <utils/string.hpp>

namespace dedicated
{
	namespace
	{
		utils::hook::detour gscr_set_dynamic_dvar_hook;
		utils::hook::detour com_quit_f_hook;

		const game::dvar_t* sv_lanOnly;

		void init_dedicated_server()
		{
			static bool initialized = false;
			if (initialized) return;
			initialized = true;

			// R_LoadGraphicsAssets
			reinterpret_cast<void(*)()>(0x1405A54F0)();
		}

		void send_heartbeat()
		{
			if (sv_lanOnly->current.enabled)
			{
				return;
			}

			game::netadr_s target{};
			if (server_list::get_master_server(target))
			{
				network::send(target, "heartbeat", "S1");
			}
		}

		std::vector<std::string>& get_startup_command_queue()
		{
			static std::vector<std::string> startup_command_queue;
			return startup_command_queue;
		}

		void execute_startup_command(int client, int /*controllerIndex*/, const char* command)
		{
			if (game::Live_SyncOnlineDataFlags(0) == 0)
			{
				game::Cbuf_ExecuteBufferInternal(0, 0, command, game::Cmd_ExecuteSingleCommand);
			}
			else
			{
				get_startup_command_queue().emplace_back(command);
			}
		}

		void execute_startup_command_queue()
		{
			const auto queue = get_startup_command_queue();
			get_startup_command_queue().clear();

			for (const auto& command : queue)
			{
				game::Cbuf_ExecuteBufferInternal(0, 0, command.data(), game::Cmd_ExecuteSingleCommand);
			}
		}

		std::vector<std::string>& get_console_command_queue()
		{
			static std::vector<std::string> console_command_queue;
			return console_command_queue;
		}

		void execute_console_command([[maybe_unused]] const int local_client_num, const char* command)
		{
			if (game::Live_SyncOnlineDataFlags(0) == 0)
			{
				command::execute(command);
			}
			else
			{
				get_console_command_queue().emplace_back(command);
			}
		}

		void execute_console_command_queue()
		{
			const auto queue = get_console_command_queue();
			get_console_command_queue().clear();

			for (const auto& command : queue)
			{
				game::Cbuf_AddText(0, command.data());
				game::Cbuf_AddText(0, "\n");
			}
		}

		void sync_gpu_stub()
		{
			std::this_thread::sleep_for(1ms);
		}

		void gscr_set_dynamic_dvar()
		{
			auto s = game::Scr_GetString(0);
			auto* dvar = game::Dvar_FindVar(s);
			if (dvar && !std::strncmp("scr_", dvar->name, 4))
			{
				return;
			}

			gscr_set_dynamic_dvar_hook.invoke<void>();
		}

		void kill_server()
		{
			for (auto i = 0; i < *game::mp::svs_numclients; ++i)
			{
				if (game::mp::svs_clients[i].header.state >= 3)
				{
					game::SV_GameSendServerCommand(i, game::SV_CMD_CAN_IGNORE,
					                               utils::string::va("r \"%s\"", "EXE_ENDOFGAME"));
				}
			}

			com_quit_f_hook.invoke<void>();
		}

		void sys_error_stub(const char* msg, ...)
		{
			char buffer[2048]{};

			va_list ap;
			va_start(ap, msg);

			vsnprintf_s(buffer, _TRUNCATE, msg, ap);

			va_end(ap);

			scheduler::once([]
			{
				command::execute("map_rotate");
			}, scheduler::main, 3s);

			game::Com_Error(game::ERR_DROP, "%s", buffer);
		}
	}

	void initialize()
	{
		command::execute("exec default_xboxlive.cfg", true);
		command::execute("onlinegame 1", true);
		command::execute("xblive_privatematch 1", true);
	}

	class component final : public component_interface
	{
	public:
		void* load_import(const std::string& library, const std::string& function) override
		{
			return nullptr;
		}

		void post_unpack() override
		{
			if (!game::environment::is_dedi())
			{
				return;
			}

			game::Dvar_RegisterBool("dedicated", true, game::DVAR_FLAG_READ, "Dedicated server");

			sv_lanOnly = game::Dvar_RegisterBool("sv_lanOnly", false, game::DVAR_FLAG_NONE, "Don't send heartbeat");

			// Disable VirtualLobby
			dvars::override::register_bool("virtualLobbyEnabled", false, game::DVAR_FLAG_NONE | game::DVAR_FLAG_READ);

			// Disable r_preloadShaders
			dvars::override::register_bool("r_preloadShaders", false, game::DVAR_FLAG_NONE | game::DVAR_FLAG_READ);

			// Don't allow sv_hostname to be changed by the game
			dvars::disable::set_string("sv_hostname");

			// Stop crashing from sys_errors
			utils::hook::jump(0x140539670, sys_error_stub);

			// Hook R_SyncGpu
			utils::hook::jump(0x140659C10, sync_gpu_stub);

			utils::hook::jump(0x140267F30, init_dedicated_server);

			// delay startup commands until the initialization is done
			utils::hook::call(0x14042B75B, execute_startup_command);

			// delay console commands until the initialization is done
			utils::hook::call(0x14042C2A6, execute_console_command);
			utils::hook::nop(0x14042C2BD, 5);

			// patch GScr_SetDynamicDvar to behave better
			gscr_set_dynamic_dvar_hook.create(0x140366230, &gscr_set_dynamic_dvar);

			utils::hook::nop(0x1405117FC, 5); // don't load config file
			utils::hook::nop(0x14040C625, 5); // ^
			if (game::port_pending) utils::hook::set<uint8_t>(0x1403D2490, 0xC3); // don't save config file
			utils::hook::set<uint8_t>(0x140287120, 0xC3); // disable self-registration
			utils::hook::set<uint8_t>(0x14053DBB0, 0xC3); // init sound system (1)
			utils::hook::set<uint8_t>(0x140693EC0, 0xC3); // init sound system (2)
			utils::hook::set<uint8_t>(0x140657C90, 0xC3); // render thread
			utils::hook::set<uint8_t>(0x14026EFA0, 0xC3); // called from Com_Frame, seems to do renderer stuff
			utils::hook::set<uint8_t>(0x140263D90, 0xC3);
			// CL_CheckForResend, which tries to connect to the local server constantly
			utils::hook::set<uint8_t>(0x1406026F9, 0); // r_loadForRenderer default to 0
			utils::hook::set<uint8_t>(0x140539CC4, 0xC3); // recommended settings check - TODO: Check hook
			utils::hook::set<uint8_t>(0x14053CC80, 0xC3); // some mixer-related function called on shutdown
			utils::hook::set<uint8_t>(0x14040F8D0, 0xC3); // dont load ui gametype stuff
			utils::hook::nop(0x1404980E4, 6); // unknown check in SV_ExecuteClientMessage
			utils::hook::nop(0x140497678, 4); // allow first slot to be occupied
			utils::hook::nop(0x14026998C, 2); // properly shut down dedicated servers
			utils::hook::nop(0x140269959, 2); // ^
			if (game::port_pending) utils::hook::nop(0x14020E047, 5); // don't shutdown renderer

			utils::hook::set<uint8_t>(0x1400AA430, 0xC3); // something to do with blendShapeVertsView
			utils::hook::nop(0x140697C59, 8); // sound thing

			utils::hook::set<uint8_t>(0x140438670, 0xC3); // cpu detection stuff?
			utils::hook::set<uint8_t>(0x140615DA0, 0xC3); // gfx stuff during fastfile loading
			utils::hook::set<uint8_t>(0x140615CB0, 0xC3); // ^
			utils::hook::set<uint8_t>(0x140615D40, 0xC3); // ^
			utils::hook::set<uint8_t>(0x1402D0CA0, 0xC3); // ^
			utils::hook::set<uint8_t>(0x140615D00, 0xC3); // ^
			utils::hook::set<uint8_t>(0x1405D86D0, 0xC3); // directx stuff
			utils::hook::set<uint8_t>(0x140607BE0, 0xC3); // ^
			utils::hook::set<uint8_t>(0x14065D790, 0xC3); // ^ - mutex
			utils::hook::set<uint8_t>(0x140607F70, 0xC3); // ^

			utils::hook::set<uint8_t>(0x1400A5C00, 0xC3); // rendering stuff
			if (game::port_pending) utils::hook::set<uint8_t>(0x1405A1AB0, 0xC3); // ^
			if (game::port_pending) utils::hook::set<uint8_t>(0x1405A1BB0, 0xC3); // ^
			utils::hook::set<uint8_t>(0x140608B10, 0xC3); // ^
			utils::hook::set<uint8_t>(0x1406095D0, 0xC3); // ^
			utils::hook::set<uint8_t>(0x140609CF0, 0xC3); // ^

			// shaders
			utils::hook::set<uint8_t>(0x1400AA2E0, 0xC3); // ^
			utils::hook::set<uint8_t>(0x1400AA270, 0xC3); // ^

			utils::hook::set<uint8_t>(0x140651590, 0xC3); // ^ - mutex

			utils::hook::set<uint8_t>(0x14053E400, 0xC3); // idk
			utils::hook::set<uint8_t>(0x1405D9A10, 0xC3); // ^

			utils::hook::set<uint8_t>(0x14060C685, 0xC3); // R_Shutdown
			utils::hook::set<uint8_t>(0x1405D9640, 0xC3); // shutdown stuff
			if (game::port_pending) utils::hook::set<uint8_t>(0x1405A6F40, 0xC3); // ^
			utils::hook::set<uint8_t>(0x14060CA20, 0xC3); // ^

			if (game::port_pending) utils::hook::set<uint8_t>(0x14062C550, 0xC3); // sound crashes

			utils::hook::set<uint8_t>(0x1404A2CE0, 0xC3); // disable host migration

			utils::hook::set<uint8_t>(0x14043F0E0, 0xC3); // render synchronization lock
			utils::hook::set<uint8_t>(0x14043F020, 0xC3); // render synchronization unlock

			if (game::port_pending) utils::hook::set<uint8_t>(0x1400E517B, 0xEB);
			// LUI: Unable to start the LUI system due to errors in main.lua

			if (game::port_pending) utils::hook::nop(0x1404CC482, 5); // Disable sound pak file loading
			utils::hook::nop(0x14052F099, 2); // ^
			if (game::port_pending) utils::hook::set<uint8_t>(0x140279B80, 0xC3); // Disable image pak file loading

			// Reduce min required memory
			utils::hook::set<uint64_t>(0x140533FED, 0x80000000);
			utils::hook::set<uint64_t>(0x1405340B5, 0x80000000);

			// initialize the game after onlinedataflags is 32 (workaround)
			scheduler::schedule([=]()
			{
				if (game::Live_SyncOnlineDataFlags(0) == 32 && game::Sys_IsDatabaseReady2())
				{
					scheduler::once([]
					{
						command::execute("xstartprivateparty", true);
						command::execute("disconnect", true); // 32 -> 0
					}, scheduler::pipeline::main, 1s);
					return scheduler::cond_end;
				}

				return scheduler::cond_continue;
			}, scheduler::pipeline::main, 1s);

			scheduler::on_game_initialized([]
			{
				initialize();

				console::info("==================================\n");
				console::info("Server started!\n");
				console::info("==================================\n");

				// remove disconnect command
				game::Cmd_RemoveCommand(reinterpret_cast<const char*>(751));

				execute_startup_command_queue();
				execute_console_command_queue();

				// Send heartbeat to dpmaster
				scheduler::once(send_heartbeat, scheduler::pipeline::server);
				scheduler::loop(send_heartbeat, scheduler::pipeline::server, 10min);
				command::add("heartbeat", send_heartbeat);
			}, scheduler::pipeline::main, 1s);

			command::add("killserver", kill_server);
			com_quit_f_hook.create(0x14042E470, &kill_server);
		}
	};
}

REGISTER_COMPONENT(dedicated::component)
