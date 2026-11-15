#include <std_include.hpp>
#include "loader/component_loader.hpp"

#include "game/game.hpp"

#include "console.hpp"
#include "scheduler.hpp"

#include <utils/hook.hpp>

namespace fast_loadlibrary
{
	namespace
	{
		utils::hook::detour load_library_a_hook;
		std::atomic<uint64_t> shortcuts{};
		std::atomic<uint64_t> loads{};

		HMODULE WINAPI load_library_a_stub(const LPCSTR name)
		{
			if (name && *name)
			{
				HMODULE module{};
				if (GetModuleHandleExA(0, name, &module) && module)
				{
					++shortcuts;
					return module;
				}
			}

			++loads;
			return load_library_a_hook.invoke<HMODULE>(name);
		}
	}

	class component final : public component_interface
	{
	public:
		void post_start() override
		{
			const auto kernelbase = GetModuleHandleA("kernelbase.dll");
			const auto function = kernelbase ? GetProcAddress(kernelbase, "LoadLibraryA") : nullptr;
			if (function)
			{
				load_library_a_hook.create(function, load_library_a_stub);
			}
		}

		void post_unpack() override
		{
			scheduler::once([]
			{
				console::info("[fast_loadlibrary] LoadLibraryA: %llu con la DLL ya cargada, %llu cargas reales\n",
					shortcuts.load(), loads.load());
			}, scheduler::main, 20s);
		}
	};
}

REGISTER_COMPONENT(fast_loadlibrary::component)
