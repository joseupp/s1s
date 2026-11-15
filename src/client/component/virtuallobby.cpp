#include <std_include.hpp>
#include "loader/component_loader.hpp"

#include "game/game.hpp"

#include <utils/hook.hpp>

namespace virtuallobby
{
	namespace
	{
		game::dvar_t* virtualLobby_fov;
		game::dvar_t* virtualLobby_fovscale;

		const auto get_fov_stub = utils::hook::assemble([](utils::hook::assembler& a)
		{
			const auto ret = a.newLabel();
			const auto original = a.newLabel();

			a.pushad64();
			a.mov(rax, qword_ptr(0x147FBD8A0)); // virtualLobbyInFiringRange
			a.cmp(byte_ptr(rax, 0x10), 1);
			a.je(original);
			a.call_aligned(game::VirtualLobby_Loaded);
			a.cmp(al, 0);
			a.je(original);

			// virtuallobby
			a.popad64();
			a.mov(rax, ptr(reinterpret_cast<int64_t>(&virtualLobby_fov)));
			a.jmp(ret);

			// original
			a.bind(original);
			a.popad64();
			a.mov(rax, qword_ptr(0x140F28298));
			a.jmp(ret);

			a.bind(ret);
			a.mov(rdx, 0x1418BAAC8);
			a.mov(edx, dword_ptr(rdx));
			a.mov(ecx, dword_ptr(rsp, 0xA8));
			a.shr(edx, 0xE);
			a.movss(xmm6, dword_ptr(rax, 0x10));
			a.jmp(0x140232841);
		});

		const auto get_fovscale_stub = utils::hook::assemble([](utils::hook::assembler& a)
		{
			const auto ret = a.newLabel();
			const auto original = a.newLabel();

			a.pushad64();
			a.mov(rax, qword_ptr(0x147FBD8A0)); // virtualLobbyInFiringRange
			a.cmp(byte_ptr(rax, 0x10), 1);
			a.je(original);
			a.call_aligned(game::VirtualLobby_Loaded);
			a.cmp(al, 0);
			a.je(original);

			// virtuallobby
			a.popad64();
			a.mov(rax, ptr(reinterpret_cast<int64_t>(&virtualLobby_fovscale)));
			a.jmp(ret);

			// original
			a.bind(original);
			a.popad64();
			a.mov(rax, qword_ptr(0x140F282A0));
			a.jmp(ret);

			a.bind(ret);
			a.mov(rcx, 0x1418BA500);
			a.jmp(0x140232AB2);
		});
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

			virtualLobby_fov = game::Dvar_RegisterFloat("virtualLobby_fov", 40.0f, 1.0f, 170.0f, game::DVAR_FLAG_SAVED, "Field of view for the virtual lobby");
			virtualLobby_fovscale = game::Dvar_RegisterFloat("virtualLobby_fovScale", 1.0f, 0.0f, 2.0f, game::DVAR_FLAG_SAVED, "Field of view scaled for the virtual lobby");

			utils::hook::nop(0x140232825, 28);
			utils::hook::jump(0x140232825, get_fov_stub, true);

			utils::hook::nop(0x140232AA4, 14);
			utils::hook::jump(0x140232AA4, get_fovscale_stub, true);
		}
	};
}

REGISTER_COMPONENT(virtuallobby::component)
