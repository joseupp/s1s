#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "game/game.hpp"

#include <utils/hook.hpp>

namespace arxan_xor_fix
{
	namespace
	{
		alignas(16) uint8_t near_thunk[16]{};

		void* create_near_thunk(void* target)
		{
			DWORD old_protect{};
			VirtualProtect(near_thunk, sizeof(near_thunk), PAGE_EXECUTE_READWRITE, &old_protect);

			near_thunk[0] = 0x48;
			near_thunk[1] = 0xB8;
			std::memcpy(&near_thunk[2], &target, sizeof(target));
			near_thunk[10] = 0xFF;
			near_thunk[11] = 0xE0;

			FlushInstructionCache(GetCurrentProcess(), near_thunk, sizeof(near_thunk));
			return near_thunk;
		}

		void* create_rbx_guard(const size_t original)
		{
			return utils::hook::assemble([original](utils::hook::assembler& a)
			{
				a.push(rbx);
				a.sub(rsp, 0x40);

				a.mov(rax, qword_ptr(rsp, 0x70));
				a.mov(qword_ptr(rsp, 0x20), rax);
				a.mov(rax, qword_ptr(rsp, 0x78));
				a.mov(qword_ptr(rsp, 0x28), rax);
				a.mov(rax, qword_ptr(rsp, 0x80));
				a.mov(qword_ptr(rsp, 0x30), rax);

				a.call(reinterpret_cast<void*>(original));

				a.add(rsp, 0x40);
				a.pop(rbx);
				a.ret();
			});
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

			const auto guard = create_near_thunk(create_rbx_guard(0x140434F70));

			utils::hook::call(0x14026E38D, guard);
			utils::hook::call(0x1404AC7D8, guard);
			utils::hook::call(0x1404ACD3F, guard);
			utils::hook::call(0x1404ACE8B, guard);
		}
	};
}

REGISTER_COMPONENT(arxan_xor_fix::component)
