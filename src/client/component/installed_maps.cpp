#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "game/game.hpp"

#include "game_module.hpp"
#include "scheduler.hpp"

#include <utils/hook.hpp>
#include <utils/io.hpp>

namespace installed_maps
{
	namespace
	{
		constexpr auto map_count_address = 0x147E732E0;
		constexpr auto map_pack_address = 0x147E73D7C;
		constexpr auto map_entry_size = 0x1228;
		constexpr auto map_name_offset = 0x828;
		constexpr auto missing_pack = 0x7FFF;

		utils::hook::detour party_map_allowed_hook;

		std::mutex cache_mutex;
		std::unordered_map<std::string, bool> installed_cache{};

		bool is_installed(const char* map)
		{
			if (!map || !*map)
			{
				return true;
			}

			std::lock_guard _(cache_mutex);

			const std::string name = map;
			if (const auto i = installed_cache.find(name); i != installed_cache.end())
			{
				return i->second;
			}

			const auto& folder = game_module::get_host_module().get_folder();
			const auto installed = utils::io::file_exists(folder + "/" + name + ".ff")
				|| utils::io::file_exists(folder + "/zone/" + name + ".ff")
				|| utils::io::file_exists(folder + "/s1/zone/" + name + ".ff");

			installed_cache[name] = installed;
			return installed;
		}

		bool party_map_allowed_stub(void* party, const char* map, const int playlist)
		{
			if (map && !is_installed(map))
			{
				return false;
			}

			return party_map_allowed_hook.invoke<bool>(party, map, playlist);
		}

		void hide_missing_maps()
		{
			const auto count = *reinterpret_cast<int*>(map_count_address);
			for (auto i = 0; i < count && i < 64; ++i)
			{
				auto* pack = reinterpret_cast<int*>(map_pack_address + i * map_entry_size);
				if (*pack == missing_pack || *pack >= 0x4000)
				{
					continue;
				}

				const auto* name = reinterpret_cast<const char*>(pack) - map_name_offset;
				if (strncmp(name, "mp_", 3) == 0 && !is_installed(name))
				{
					*pack = missing_pack;
				}
			}
		}
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

			party_map_allowed_hook.create(0x14028AAE0, party_map_allowed_stub);

			scheduler::loop(hide_missing_maps, scheduler::pipeline::main, 500ms);
		}
	};
}

REGISTER_COMPONENT(installed_maps::component)
