#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "game/game.hpp"

#include "command.hpp"
#include "console.hpp"

namespace memory_dump
{
	namespace
	{
		constexpr uintptr_t image_base = 0x140000000;

		void dump_image()
		{
			const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image_base);
			const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image_base + dos->e_lfanew);
			const size_t size = nt->OptionalHeader.SizeOfImage;

			const auto* name = game::environment::is_sp() ? "s1_sp_dump.bin" : "s1_mp_dump.bin";

			FILE* file = nullptr;
			if (fopen_s(&file, name, "wb") != 0 || !file)
			{
				console::error("dumpexe: no se pudo crear %s\n", name);
				return;
			}

			std::vector<uint8_t> zeros(0x10000, 0);
			size_t offset = 0;
			size_t readable = 0;

			while (offset < size)
			{
				MEMORY_BASIC_INFORMATION mbi{};
				const auto address = image_base + offset;
				if (!VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi)))
				{
					break;
				}

				const auto region_end = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
				const auto chunk = std::min(region_end - address, size - offset);

				const auto protect = mbi.Protect & 0xFF;
				const auto can_read = mbi.State == MEM_COMMIT && !(mbi.Protect & PAGE_GUARD) &&
					protect != PAGE_NOACCESS && protect != 0;

				if (can_read)
				{
					fwrite(reinterpret_cast<void*>(address), 1, chunk, file);
					readable += chunk;
				}
				else
				{
					for (size_t written = 0; written < chunk;)
					{
						const auto n = std::min(zeros.size(), chunk - written);
						fwrite(zeros.data(), 1, n, file);
						written += n;
					}
				}

				offset += chunk;
			}

			fclose(file);
			console::info("dumpexe: %s escrito (%zu bytes, %zu legibles)\n", name, offset, readable);
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

			command::add("dumpexe", []()
			{
				dump_image();
			});
		}
	};
}

REGISTER_COMPONENT(memory_dump::component)
