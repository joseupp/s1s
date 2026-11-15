#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "game/game.hpp"

#include "command.hpp"
#include "console.hpp"
#include "lobby_bots.hpp"
#include "public_lobby.hpp"
#include "scheduler.hpp"

#include <utils/hook.hpp>

namespace public_lobby
{
	namespace
	{
		constexpr auto max_party_players = 18;

		std::atomic_bool game_start_requested{false};

		std::mutex map_mutex;
		std::string match_map{};
		std::string match_gametype{};
		std::string match_gametype_name{};

		void keep_dvar(const char* name, const std::string& value)
		{
			if (value.empty())
			{
				return;
			}

			auto* dvar = game::Dvar_FindVar(name);
			if (dvar && dvar->current.string && value != dvar->current.string)
			{
				game::Dvar_SetString(dvar, value.data());
			}
		}

		void keep_match_map()
		{
			if (!lobby_bots::is_public_lobby())
			{
				return;
			}

			std::string map{};
			std::string gametype{};
			std::string gametype_name{};
			{
				std::lock_guard _(map_mutex);
				map = match_map;
				gametype = match_gametype;
				gametype_name = match_gametype_name;
			}

			keep_dvar("ui_gametype", gametype);

			keep_dvar("ui_mapvote_entrya_gametype", gametype);
			keep_dvar("ui_mapvote_entryb_gametype", gametype);

			keep_dvar("playlist_name", gametype_name);
			if (game_start_requested)
			{
				keep_dvar("ui_mapname", map);
			}
		}

		int game_state_info_get_index_stub()
		{
			return 0;
		}

		int bots_allowed_stub()
		{
			return 1;
		}

		bool not_playlist_stub()
		{
			return false;
		}

		bool get_dvar_bool(const char* name)
		{
			const auto* dvar = game::Dvar_FindVar(name);
			return dvar && dvar->current.enabled;
		}

		void set_return_state(std::uint8_t* party)
		{
			auto& state = *reinterpret_cast<int*>(party + 0x41BC);
			if (!get_dvar_bool("xblive_privatematch") && !get_dvar_bool("systemlink"))
			{
				state = (state & ~0x78) | 0x4;
			}
			else
			{
				state = (state & ~0x3C) | 0x40;
			}
		}

		void* make_return_state_stub_eax(const std::size_t return_address)
		{
			return utils::hook::assemble([return_address](utils::hook::assembler& a)
			{
				a.mov(dword_ptr(rbx, 0x4158), r13d);
				a.mov(dword_ptr(rbx, 0x41BC), eax);
				a.pushad64();
				a.mov(rcx, rbx);
				a.call_aligned(set_return_state);
				a.popad64();
				a.mov(eax, dword_ptr(rbx, 0x41BC));
				a.jmp(return_address);
			});
		}

		void* make_return_state_stub(const std::size_t return_address)
		{
			return utils::hook::assemble([return_address](utils::hook::assembler& a)
			{
				a.pushad64();
				a.mov(rcx, rbx);
				a.call_aligned(set_return_state);
				a.popad64();
				a.jmp(return_address);
			});
		}

		void set_dvar_int(const char* name, const int value)
		{
			auto* dvar = game::Dvar_FindVar(name);
			if (dvar && dvar->current.integer != value)
			{
				dvar->current.integer = value;
			}
		}

		bool is_in_match()
		{
			return game::CL_IsCgameInitialized() && !game::VirtualLobby_Loaded();
		}

		void update()
		{
			if (game_start_requested && (!lobby_bots::is_public_lobby() || is_in_match()))
			{
				game_start_requested = false;
				std::lock_guard _(map_mutex);
				match_map.clear();
			}

			if (!lobby_bots::is_public_lobby())
			{
				return;
			}

			set_dvar_int("party_minplayers", game_start_requested ? 1 : max_party_players + 1);

			if (auto* rules = get_match_rules())
			{
				*reinterpret_cast<int*>(rules + 0x1C) = 0;
			}
		}
	}

	bool is_game_start_requested()
	{
		return game_start_requested;
	}

	std::uint8_t* get_match_rules()
	{
		return utils::hook::invoke<std::uint8_t*>(0x140688EC0);
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

			game::Dvar_RegisterBool("public_lobby_buttons", true, 0,
			                        "Start game and game setup buttons in the public lobby");

			utils::hook::set<std::uint8_t>(0x14050B55B, 6);

			utils::hook::jump(0x140688E70, 0x140688EC0, true);
			utils::hook::jump(0x140688ED0, game_state_info_get_index_stub, true);
			utils::hook::jump(0x140195030, bots_allowed_stub, true);

			utils::hook::call(0x1404E5540, not_playlist_stub);
			utils::hook::call(0x1404E5581, not_playlist_stub);
			utils::hook::call(0x1404E5610, not_playlist_stub);
			utils::hook::call(0x1404E5651, not_playlist_stub);
			utils::hook::call(0x1404F162F, not_playlist_stub);

			utils::hook::nop(0x140278F12, 19);
			utils::hook::jump(0x140278F12, make_return_state_stub_eax(0x140278F25), true);
			utils::hook::jump(0x14027D5F5, make_return_state_stub(0x14027D603), true);

			command::add("lobby_partygo", [](const command::params& params)
			{
				if (lobby_bots::is_public_lobby())
				{
					{
						std::lock_guard _(map_mutex);
						match_map = params.size() >= 2 ? params.get(1) : "";
						if (params.size() >= 3)
						{
							match_gametype = params.get(2);
						}
					}

					game_start_requested = true;
					keep_match_map();
					update();
					return;
				}

				console::warn("lobby_partygo: no estas en la sala publica\n");
			});

			command::add("lobby_gametype", [](const command::params& params)
			{
				if (params.size() >= 2)
				{
					std::lock_guard _(map_mutex);
					match_gametype = params.get(1);
					match_gametype_name = params.size() >= 3 ? params.join(2) : "";
				}

				keep_match_map();
			});

			command::add("lobby_cancelstartserver", []
			{
				if (!game_start_requested)
				{
					return;
				}

				game_start_requested = false;
				update();
			});

			scheduler::loop(update, scheduler::pipeline::main, 100ms);
			scheduler::loop(keep_match_map, scheduler::pipeline::main);
		}
	};
}

REGISTER_COMPONENT(public_lobby::component)
