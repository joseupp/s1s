#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "game/game.hpp"

#include <utils/hook.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <vector>
#include <cmath>

namespace viewmodel_fov
{
	namespace
	{
		constexpr auto deg_to_rad = 0.01745329251994329577f;

		game::dvar_t* cg_fov_viewmodel = nullptr;
		game::dvar_t* cg_fov_viewmodel_scale = nullptr;
		game::dvar_t* cg_fov_ads_affected = nullptr;

		constexpr float base_fov = 65.0f;

		std::atomic<float> effective_fov_scale{1.0f};

		float captured_zoom = 0.0f;
		float last_zoom = 0.0f;

		float laser_scale_x = 1.0f;
		float laser_scale_y = 1.0f;

		bool in_viewmodel_laser = false;

		constexpr uintptr_t refdef_tan_x = 0x1419503D0;
		constexpr uintptr_t refdef_tan_y = 0x1419503D4;
		constexpr uintptr_t refdef_view_origin = 0x1419503D8;
		constexpr uintptr_t refdef_view_axis = 0x1419503E4;

		struct fov_entry
		{
			float world_x;
			float world_y;
			float weapon_x;
			float weapon_y;
		};

		constexpr size_t history_size = 16;
		std::mutex history_mutex;
		std::array<fov_entry, history_size> history{};
		size_t history_index = 0;

		utils::hook::detour r_derive_projection_matrix_hook;

		template <typename T>
		T& field(void* base, const size_t offset)
		{
			return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
		}

		float get_viewmodel_fov()
		{
			return cg_fov_viewmodel ? cg_fov_viewmodel->current.value : 0.0f;
		}

		float get_viewmodel_fov_scale()
		{
			return cg_fov_viewmodel_scale ? cg_fov_viewmodel_scale->current.value : 1.0f;
		}

		float get_world_fov_scale()
		{
			const auto value = effective_fov_scale.load();
			return value > 0.0f ? value : 1.0f;
		}

		float get_view_fov(const int local_client_num)
		{
			static auto* cg_fov = game::Dvar_FindVar("cg_fov");
			static auto* cg_fov_scale = game::Dvar_FindVar("cg_fovScale");

			if (!cg_fov || !cg_fov_scale)
			{
				return utils::hook::invoke<float>(0x140232700, local_client_num);
			}

			const auto user_fov = cg_fov->current.value;
			const auto user_scale = cg_fov_scale->current.value;

			if (!cg_fov_ads_affected || !cg_fov_ads_affected->current.enabled || user_fov <= base_fov)
			{
				effective_fov_scale = user_scale;
				return utils::hook::invoke<float>(0x140232700, local_client_num);
			}

			const auto scale = user_scale * user_fov / base_fov;
			cg_fov->current.value = base_fov;
			cg_fov_scale->current.value = scale;

			const auto result = utils::hook::invoke<float>(0x140232700, local_client_num);

			cg_fov->current.value = user_fov;
			cg_fov_scale->current.value = user_scale;
			effective_fov_scale = scale;
			return result;
		}

		bool calc_weapon_tan(const float world_x, const float world_y, const float zoom, float& weapon_x, float& weapon_y)
		{
			const auto viewmodel_fov = get_viewmodel_fov();
			if (viewmodel_fov <= 0.0f || world_x <= 0.0f || world_y <= 0.0f)
			{
				return false;
			}

			const auto viewmodel_tan_y = std::tan(viewmodel_fov * 0.5f * deg_to_rad) * 0.75f * get_viewmodel_fov_scale();
			if (viewmodel_tan_y <= 0.0f)
			{
				return false;
			}

			auto ads_tan_y = world_y;
			const auto world_fov_scale = get_world_fov_scale();
			if (world_fov_scale != 1.0f)
			{
				const auto half_fov = std::atan(world_y / 0.75f) / world_fov_scale;
				ads_tan_y = std::tan(std::clamp(half_fov, 0.001f, 1.56f)) * 0.75f;
			}

			const auto z = std::clamp(zoom, 0.0f, 1.0f);
			const auto inv_weapon_y = (1.0f - z) / viewmodel_tan_y + z / ads_tan_y;
			if (inv_weapon_y <= 0.0f)
			{
				return false;
			}

			weapon_y = 1.0f / inv_weapon_y;
			weapon_x = weapon_y * (world_x / world_y);
			return true;
		}

		void push_entry(const fov_entry& entry)
		{
			std::lock_guard _(history_mutex);

			const auto& last = history[(history_index + history_size - 1) % history_size];
			if (last.world_x == entry.world_x && last.world_y == entry.world_y &&
				last.weapon_x == entry.weapon_x && last.weapon_y == entry.weapon_y)
			{
				return;
			}

			history[history_index] = entry;
			history_index = (history_index + 1) % history_size;
		}

		bool find_scale(const float world_x, const float world_y, float& scale_x, float& scale_y)
		{
			if (get_viewmodel_fov() <= 0.0f || world_x <= 0.0f || world_y <= 0.0f)
			{
				return false;
			}

			{
				std::lock_guard _(history_mutex);

				for (size_t i = 1; i <= history_size; ++i)
				{
					const auto& entry = history[(history_index + history_size - i) % history_size];
					if (entry.weapon_x <= 0.0f || entry.weapon_y <= 0.0f)
					{
						continue;
					}

					if (std::abs(entry.world_x - world_x) <= world_x * 1e-4f &&
						std::abs(entry.world_y - world_y) <= world_y * 1e-4f)
					{
						scale_x = world_x / entry.weapon_x;
						scale_y = world_y / entry.weapon_y;
						return true;
					}
				}
			}

			float weapon_x, weapon_y;
			if (!calc_weapon_tan(world_x, world_y, last_zoom, weapon_x, weapon_y))
			{
				return false;
			}

			scale_x = world_x / weapon_x;
			scale_y = world_y / weapon_y;
			return true;
		}

		float get_zoom_frac_stub(void* ps, const uint64_t weapon)
		{
			const auto zoom = utils::hook::invoke<float>(0x140243EE0, ps, weapon);
			captured_zoom = zoom;
			return zoom;
		}

		float cg_get_view_fov_stub(const int local_client_num)
		{
			captured_zoom = 0.0f;
			return get_view_fov(local_client_num);
		}

		void cg_calc_fov_tangents_stub(const int local_client_num, const float fov)
		{
			utils::hook::invoke<void>(0x1402352D0, local_client_num, fov);

			auto* tan_x = reinterpret_cast<float*>(refdef_tan_x);
			auto* tan_y = reinterpret_cast<float*>(refdef_tan_y);

			last_zoom = captured_zoom;
			laser_scale_x = 1.0f;
			laser_scale_y = 1.0f;

			float weapon_x, weapon_y;
			if (calc_weapon_tan(*tan_x, *tan_y, captured_zoom, weapon_x, weapon_y))
			{
				push_entry({*tan_x, *tan_y, weapon_x, weapon_y});
				laser_scale_x = *tan_x / weapon_x;
				laser_scale_y = *tan_y / weapon_y;
			}
		}

		void viewmodel_laser_stub(const int a1, void* a2, void* a3, void* a4, void* a5, void* a6, const int a7, void* a8,
		                          const float a9)
		{
			in_viewmodel_laser = true;
			utils::hook::invoke<void>(0x1401FCDC0, a1, a2, a3, a4, a5, a6, a7, a8, a9);
			in_viewmodel_laser = false;
		}

		void remap_to_world_projection(float* point, float* dir)
		{
			const auto* origin = reinterpret_cast<const float*>(refdef_view_origin);
			const auto* axis = reinterpret_cast<const float*>(refdef_view_axis);

			const float d[3] = {point[0] - origin[0], point[1] - origin[1], point[2] - origin[2]};
			const auto forward = d[0] * axis[0] + d[1] * axis[1] + d[2] * axis[2];
			const auto right = (d[0] * axis[3] + d[1] * axis[4] + d[2] * axis[5]) * laser_scale_x;
			const auto up = (d[0] * axis[6] + d[1] * axis[7] + d[2] * axis[8]) * laser_scale_y;

			for (auto i = 0; i < 3; ++i)
			{
				point[i] = origin[i] + axis[i] * forward + axis[3 + i] * right + axis[6 + i] * up;
			}

			if (!dir)
			{
				return;
			}

			const auto dir_forward = dir[0] * axis[0] + dir[1] * axis[1] + dir[2] * axis[2];
			const auto dir_right = (dir[0] * axis[3] + dir[1] * axis[4] + dir[2] * axis[5]) * laser_scale_x;
			const auto dir_up = (dir[0] * axis[6] + dir[1] * axis[7] + dir[2] * axis[8]) * laser_scale_y;

			float new_dir[3];
			for (auto i = 0; i < 3; ++i)
			{
				new_dir[i] = axis[i] * dir_forward + axis[3 + i] * dir_right + axis[6 + i] * dir_up;
			}

			const auto length = std::sqrt(new_dir[0] * new_dir[0] + new_dir[1] * new_dir[1] + new_dir[2] * new_dir[2]);
			if (length > 0.0001f)
			{
				for (auto i = 0; i < 3; ++i)
				{
					dir[i] = new_dir[i] / length;
				}
			}
		}

		void dolly_to_world_projection(float* point, float* dir = nullptr, const float back = 0.0f)
		{
			if (laser_scale_y <= 0.0f)
			{
				return;
			}

			const auto* origin = reinterpret_cast<const float*>(refdef_view_origin);
			const auto* axis = reinterpret_cast<const float*>(refdef_view_axis);

			const float d[3] = {point[0] - origin[0], point[1] - origin[1], point[2] - origin[2]};
			const auto forward = d[0] * axis[0] + d[1] * axis[1] + d[2] * axis[2];
			const auto right = d[0] * axis[3] + d[1] * axis[4] + d[2] * axis[5];
			const auto up = d[0] * axis[6] + d[1] * axis[7] + d[2] * axis[8];
			const auto new_forward = forward / laser_scale_y - back * (1.0f - 1.0f / laser_scale_y);

			for (auto i = 0; i < 3; ++i)
			{
				point[i] = origin[i] + axis[i] * new_forward + axis[3 + i] * right + axis[6 + i] * up;
			}

			if (!dir)
			{
				return;
			}

			const auto dir_forward = (dir[0] * axis[0] + dir[1] * axis[1] + dir[2] * axis[2]) / laser_scale_y;
			const auto dir_right = dir[0] * axis[3] + dir[1] * axis[4] + dir[2] * axis[5];
			const auto dir_up = dir[0] * axis[6] + dir[1] * axis[7] + dir[2] * axis[8];

			float new_dir[3];
			for (auto i = 0; i < 3; ++i)
			{
				new_dir[i] = axis[i] * dir_forward + axis[3 + i] * dir_right + axis[6 + i] * dir_up;
			}

			const auto length = std::sqrt(new_dir[0] * new_dir[0] + new_dir[1] * new_dir[1] + new_dir[2] * new_dir[2]);
			if (length > 0.0001f)
			{
				for (auto i = 0; i < 3; ++i)
				{
					dir[i] = new_dir[i] / length;
				}
			}
		}

		bool remap_active()
		{
			return get_viewmodel_fov() > 0.0f && (laser_scale_x != 1.0f || laser_scale_y != 1.0f);
		}

		void laser_tag_orientation_stub(void* a1, void* a2, const uint32_t tag, float* tag_axis, float* orientation)
		{
			utils::hook::invoke<void>(0x1401EC760, a1, a2, tag, tag_axis, orientation);
			if (!in_viewmodel_laser || !orientation || !tag_axis || !remap_active())
			{
				return;
			}

			remap_to_world_projection(orientation, tag_axis);
		}

		utils::hook::detour fx_get_bone_orientation_hook;
		thread_local bool in_fx_bolt_update = false;

		thread_local bool fx_origin_only = false;

		constexpr auto body_fx_back = 5.0f;

		const char* get_effect_name(const void* effect)
		{
			if (!effect)
			{
				return nullptr;
			}

			const auto* def = *static_cast<const void* const*>(effect);
			return def ? *static_cast<const char* const*>(def) : nullptr;
		}

		std::string to_lower(const char* name)
		{
			std::string lower = name ? name : "";
			std::transform(lower.begin(), lower.end(), lower.begin(), [](const unsigned char c)
			{
				return static_cast<char>(std::tolower(c));
			});
			return lower;
		}

		bool is_uncorrected_effect(const char* name)
		{
			if (!name)
			{
				return false;
			}

			std::string lower = name;
			std::transform(lower.begin(), lower.end(), lower.begin(), [](const unsigned char c)
			{
				return static_cast<char>(std::tolower(c));
			});

			return lower.find("shell") != std::string::npos || lower.find("brass") != std::string::npos ||
				lower.find("eject") != std::string::npos;
		}

		void beam_draw_stub(const int local_client_num, const float* start, const float* dir, const int ent,
		                    const int a5, const std::uint8_t a6)
		{
			const auto* origin = reinterpret_cast<const float*>(refdef_view_origin);
			if (start && remap_active())
			{
				const float d[3] = {start[0] - origin[0], start[1] - origin[1], start[2] - origin[2]};
				if (d[0] * d[0] + d[1] * d[1] + d[2] * d[2] < 80.0f * 80.0f)
				{
					float corrected[3] = {start[0], start[1], start[2]};
					dolly_to_world_projection(corrected);
					utils::hook::invoke<void>(0x1401FCCB0, local_client_num, corrected, dir, ent, a5, a6);
					return;
				}
			}

			utils::hook::invoke<void>(0x1401FCCB0, local_client_num, start, dir, ent, a5, a6);
		}

		bool is_viewmodel_dobj(const int dobj)
		{
			return dobj == 0x800 || dobj == 0x801;
		}

		bool fx_get_bone_orientation_stub(const int local_client_num, const int dobj, const int bone, float* orientation)
		{
			const auto result = fx_get_bone_orientation_hook.invoke<bool>(local_client_num, dobj, bone, orientation);
			if (result && orientation && is_viewmodel_dobj(dobj) && in_fx_bolt_update && remap_active())
			{
				if (fx_origin_only)
				{
					dolly_to_world_projection(orientation, nullptr, body_fx_back);
				}
				else
				{
					dolly_to_world_projection(orientation, orientation + 3);
				}
			}

			return result;
		}

		bool fx_bolt_update_bone_orientation(const int local_client_num, const int dobj, const int bone,
		                                     float* orientation, const void* effect)
		{
			const auto* name = get_effect_name(effect);
			in_fx_bolt_update = !is_uncorrected_effect(name);
			fx_origin_only = to_lower(name).find("_body") != std::string::npos;
			const auto result = utils::hook::invoke<bool>(0x14030B090, local_client_num, dobj, bone, orientation);
			in_fx_bolt_update = false;
			return result;
		}

		void* make_fx_bolt_update_stub()
		{
			return utils::hook::assemble([](utils::hook::assembler& a)
			{
				a.lea(r9, qword_ptr(rbp, -9));
				a.and_(r8d, 0x7FF);
				a.sub(rsp, 0x30);
				a.mov(qword_ptr(rsp, 0x20), rbx);
				a.call(reinterpret_cast<void*>(fx_bolt_update_bone_orientation));
				a.add(rsp, 0x30);
				a.jmp(reinterpret_cast<void*>(0x14030E5BA));
			});
		}

		void r_derive_projection_matrix_stub(void* source)
		{
			r_derive_projection_matrix_hook.invoke<void>(source);

			if ((field<uint32_t>(source, 0x3210) & 1) == 0)
			{
				return;
			}

			const auto m00 = field<float>(source, 0x100);
			const auto m11 = field<float>(source, 0x114);
			if (m00 <= 0.0f || m11 <= 0.0f)
			{
				return;
			}

			float scale_x, scale_y;
			if (!find_scale(1.0f / m00, 1.0f / m11, scale_x, scale_y))
			{
				return;
			}

			field<float>(source, 0x100) = m00 * scale_x;
			field<float>(source, 0x114) = m11 * scale_y;
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

			cg_fov_viewmodel = game::Dvar_RegisterFloat("cg_fovViewmodel", 65.0f, 0.0f, 160.0f, game::DVAR_FLAG_SAVED,
			                                             "FOV of the weapon viewmodel (0 = use world FOV)");
			cg_fov_viewmodel_scale = game::Dvar_RegisterFloat("cg_fovViewmodelScale", 1.0f, 0.1f, 3.0f, game::DVAR_FLAG_SAVED,
			                                                  "Scale applied to the weapon viewmodel FOV");
			cg_fov_ads_affected = game::Dvar_RegisterBool("cg_fovAdsAffected", true, game::DVAR_FLAG_SAVED,
			                                              "ADS zoom scales with cg_fov (cg_fov 65 + cg_fovScale * cg_fov / 65)");

			utils::hook::call(0x1401E5AF2, get_view_fov);
			utils::hook::call(0x1401E5B0F, get_view_fov);
			utils::hook::call(0x1402E4DA9, get_view_fov);

			utils::hook::call(0x140232A37, get_zoom_frac_stub);
			utils::hook::call(0x140230A28, cg_get_view_fov_stub);
			utils::hook::call(0x140232F87, cg_get_view_fov_stub);
			utils::hook::call(0x140230A32, cg_calc_fov_tangents_stub);
			utils::hook::call(0x140232F91, cg_calc_fov_tangents_stub);
			utils::hook::call(0x1401FD195, viewmodel_laser_stub);
			utils::hook::call(0x1401FCF1A, laser_tag_orientation_stub);
			utils::hook::call(0x1401FD527, beam_draw_stub);
			fx_get_bone_orientation_hook.create(0x14030B090, fx_get_bone_orientation_stub);
			utils::hook::nop(0x14030E5AA, 16);
			utils::hook::jump(0x14030E5AA, make_fx_bolt_update_stub(), true);

			const auto wrapper = utils::hook::assemble([](utils::hook::assembler& a)
			{
				a.push(rcx);
				a.push(rdx);
				a.push(r8);
				a.push(r9);
				a.push(r10);
				a.push(r11);
				a.sub(rsp, 0x68);
				a.movups(xmmword_ptr(rsp, 0x20), xmm2);
				a.movups(xmmword_ptr(rsp, 0x30), xmm3);
				a.movups(xmmword_ptr(rsp, 0x40), xmm4);
				a.movups(xmmword_ptr(rsp, 0x50), xmm5);

				a.call(reinterpret_cast<void*>(r_derive_projection_matrix_stub));

				a.movups(xmm2, xmmword_ptr(rsp, 0x20));
				a.movups(xmm3, xmmword_ptr(rsp, 0x30));
				a.movups(xmm4, xmmword_ptr(rsp, 0x40));
				a.movups(xmm5, xmmword_ptr(rsp, 0x50));
				a.add(rsp, 0x68);
				a.pop(r11);
				a.pop(r10);
				a.pop(r9);
				a.pop(r8);
				a.pop(rdx);
				a.pop(rcx);
				a.ret();
			});

			r_derive_projection_matrix_hook.create(0x14063FB80, wrapper);
		}
	};
}

REGISTER_COMPONENT(viewmodel_fov::component)
