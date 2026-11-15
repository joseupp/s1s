#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "game/game.hpp"

#include "localized_strings.hpp"
#include "scheduler.hpp"
#include "dvars.hpp"

#include <utils/hook.hpp>
#include <utils/string.hpp>

#include "version.hpp"

namespace branding
{
	namespace
	{
		utils::hook::detour ui_get_formatted_build_number_hook;

		const char* ui_get_formatted_build_number_stub()
		{
			const auto* const build_num = ui_get_formatted_build_number_hook.invoke<const char*>();

			return utils::string::va("%s (%s)", SHORTVERSION, build_num);
		}
	}

	class component final : public component_interface
	{
	public:
		void post_unpack() override
		{
			if (game::environment::is_dedi())
			{
				return;
			}

			dvars::override::set_string("version", "S1S " SHORTVERSION);

			ui_get_formatted_build_number_hook.create(
				SELECT_VALUE(0x14035B3F0, 0x14050B2B0), ui_get_formatted_build_number_stub);


		}
	};
}

REGISTER_COMPONENT(branding::component)
