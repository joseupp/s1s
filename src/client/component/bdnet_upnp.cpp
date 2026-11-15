#include <std_include.hpp>
#include "loader/component_loader.hpp"

#include "game/game.hpp"

#include <utils/hook.hpp>

namespace bdnet_upnp
{
	namespace
	{
		bool upnp_start_stub()
		{
			return false;
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

			utils::hook::jump(0x1406F1C70, upnp_start_stub);
		}
	};
}

REGISTER_COMPONENT(bdnet_upnp::component)
