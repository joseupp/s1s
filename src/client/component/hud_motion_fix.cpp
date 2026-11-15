#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "game/game.hpp"

#include <utils/hook.hpp>

namespace hud_motion_fix
{
	namespace
	{
		game::dvar_t* cg_hud_motion_resolution_fix = nullptr;

		float get_scale()
		{
			if (cg_hud_motion_resolution_fix && !cg_hud_motion_resolution_fix->current.enabled)
			{
				return 1.0f;
			}

			const auto height = *reinterpret_cast<int*>(0x1420D0C58);
			if (height <= 0)
			{
				return 1.0f;
			}

			return static_cast<float>(height) / 1080.0f;
		}

		void matrix_perspective_stub(const float distance, float* out)
		{
			utils::hook::invoke<void>(0x1401803D0, distance * get_scale(), out);
		}

		void matrix_translation_stub(const float x, const float y, const float z, float* out)
		{
			const auto scale = get_scale();
			utils::hook::invoke<void>(0x1401806E0, x * scale, y * scale, z * scale, out);
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

			cg_hud_motion_resolution_fix = game::Dvar_RegisterBool("cg_hudMotionResolutionFix", true, game::DVAR_FLAG_SAVED,
			                                                       "Scale HUD motion perspective with resolution (same look as 1080p)");

			utils::hook::call(0x1401361E0, matrix_perspective_stub);
			utils::hook::call(0x140136380, matrix_perspective_stub);
			utils::hook::call(0x140136209, matrix_translation_stub);
		}
	};
}

REGISTER_COMPONENT(hud_motion_fix::component)
