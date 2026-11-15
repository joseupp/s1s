#include <std_include.hpp>
#include "loader/component_loader.hpp"

#include "game/game.hpp"

#include "console.hpp"
#include "scheduler.hpp"
#include "startup_log.hpp"

#include <utils/hook.hpp>

#include <intrin.h>

namespace startup_log
{
	namespace
	{
		std::mutex mutex;
		FILE* file{};
		double process_start{};
		std::atomic<DWORD> main_thread_id{};

		utils::hook::detour db_load_x_assets_hook;
		utils::hook::detour wait_async_hook;

		std::atomic<DWORD> game_thread_id{};
		std::atomic<DWORD> loader_thread_id{};

		double now_seconds()
		{
			FILETIME ft{};
			GetSystemTimePreciseAsFileTime(&ft);
			ULARGE_INTEGER value{};
			value.LowPart = ft.dwLowDateTime;
			value.HighPart = ft.dwHighDateTime;
			return static_cast<double>(value.QuadPart) / 10'000'000.0;
		}

		double elapsed()
		{
			return now_seconds() - process_start;
		}

		void open()
		{
			FILETIME creation{}, exit{}, kernel{}, user{};
			if (GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user))
			{
				ULARGE_INTEGER value{};
				value.LowPart = creation.dwLowDateTime;
				value.HighPart = creation.dwHighDateTime;
				process_start = static_cast<double>(value.QuadPart) / 10'000'000.0;
			}
			else
			{
				process_start = now_seconds();
			}

			CreateDirectoryA("logs", nullptr);
			fopen_s(&file, "logs\\startup.log", "wb");
		}

		void write_line(const std::string& text)
		{
			std::lock_guard _(mutex);
			if (!file)
			{
				return;
			}

			const auto thread = GetCurrentThreadId();
			const auto* thread_name = thread == main_thread_id ? "main" : "";

			if (!loader_thread_id && text.starts_with("Loading fastfile"))
			{
				loader_thread_id = thread;
			}

			auto line = text;
			while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
			{
				line.pop_back();
			}

			if (line.empty())
			{
				return;
			}

			if (*thread_name)
			{
				fprintf(file, "[%8.3f] [%s] %s\n", elapsed(), thread_name, line.data());
			}
			else
			{
				fprintf(file, "[%8.3f] [%5lu] %s\n", elapsed(), thread, line.data());
			}

			fflush(file);
		}


		struct file_info
		{
			std::string path{};
			double opened{};
			uint64_t bytes{};
			uint32_t reads{};
		};

		std::mutex files_mutex;
		std::unordered_map<HANDLE, file_info> files;
		std::atomic<uint64_t> total_read{};
		constexpr auto file_log_seconds = 120.0;

		void file_opened(const HANDLE handle, std::string path)
		{
			if (handle == INVALID_HANDLE_VALUE || !handle || elapsed() > file_log_seconds)
			{
				return;
			}

			write_line("abre " + path);

			std::lock_guard _(files_mutex);
			files[handle] = {std::move(path), elapsed()};
		}

		void file_read(const HANDLE handle, const DWORD bytes)
		{
			total_read += bytes;

			std::lock_guard _(files_mutex);
			if (const auto it = files.find(handle); it != files.end())
			{
				it->second.bytes += bytes;
				++it->second.reads;
			}
		}

		std::string file_summary(const file_info& info)
		{
			return std::format("{} ({:.2f} MB en {} lecturas, {:.3f} s abierto)", info.path,
				static_cast<double>(info.bytes) / (1024.0 * 1024.0), info.reads, elapsed() - info.opened);
		}

		HANDLE WINAPI create_file_a(const LPCSTR name, const DWORD access, const DWORD share,
			const LPSECURITY_ATTRIBUTES security, const DWORD disposition, const DWORD flags, const HANDLE templ)
		{
			const auto handle = CreateFileA(name, access, share, security, disposition, flags, templ);
			file_opened(handle, name ? name : "");
			return handle;
		}

		std::string narrow(const LPCWSTR name)
		{
			if (!name)
			{
				return {};
			}

			const auto size = WideCharToMultiByte(CP_UTF8, 0, name, -1, nullptr, 0, nullptr, nullptr);
			std::string result(size > 0 ? size - 1 : 0, '\0');
			if (size > 1)
			{
				WideCharToMultiByte(CP_UTF8, 0, name, -1, result.data(), size, nullptr, nullptr);
			}

			return result;
		}

		HANDLE WINAPI create_file_w(const LPCWSTR name, const DWORD access, const DWORD share,
			const LPSECURITY_ATTRIBUTES security, const DWORD disposition, const DWORD flags, const HANDLE templ)
		{
			const auto handle = CreateFileW(name, access, share, security, disposition, flags, templ);
			file_opened(handle, narrow(name));
			return handle;
		}

		HANDLE WINAPI create_file_2(const LPCWSTR name, const DWORD access, const DWORD share, const DWORD disposition,
			const LPCREATEFILE2_EXTENDED_PARAMETERS params)
		{
			const auto handle = CreateFile2(name, access, share, disposition, params);
			file_opened(handle, narrow(name));
			return handle;
		}

		BOOL WINAPI read_file(const HANDLE handle, const LPVOID buffer, const DWORD size, const LPDWORD read,
			const LPOVERLAPPED overlapped)
		{
			file_read(handle, size);
			return ReadFile(handle, buffer, size, read, overlapped);
		}

		BOOL WINAPI read_file_ex(const HANDLE handle, const LPVOID buffer, const DWORD size,
			const LPOVERLAPPED overlapped, const LPOVERLAPPED_COMPLETION_ROUTINE routine)
		{
			file_read(handle, size);
			return ReadFileEx(handle, buffer, size, overlapped, routine);
		}

		BOOL WINAPI close_handle(const HANDLE handle)
		{
			std::optional<file_info> info{};
			{
				std::lock_guard _(files_mutex);
				if (const auto it = files.find(handle); it != files.end())
				{
					info = std::move(it->second);
					files.erase(it);
				}
			}

			if (info)
			{
				write_line("cierra " + file_summary(*info));
			}

			return CloseHandle(handle);
		}


		std::mutex checks_mutex;
		std::unordered_map<std::string, uint32_t> checks;
		std::atomic<uint32_t> total_checks{};

		void file_checked(const char* kind, const std::string& path)
		{
			if (elapsed() > file_log_seconds)
			{
				return;
			}

			++total_checks;

			auto first = false;
			{
				std::lock_guard _(checks_mutex);
				first = ++checks[std::string(kind) + " " + path] == 1;
			}

			if (first)
			{
				write_line(std::string(kind) + " " + path);
			}
		}

		DWORD WINAPI get_file_attributes_a(const LPCSTR name)
		{
			file_checked("atributos", name ? name : "");
			return GetFileAttributesA(name);
		}

		BOOL WINAPI get_file_attributes_ex_w(const LPCWSTR name, const GET_FILEEX_INFO_LEVELS level, const LPVOID info)
		{
			file_checked("atributos", narrow(name));
			return GetFileAttributesExW(name, level, info);
		}

		HANDLE WINAPI find_first_file_ex_a(const LPCSTR name, const FINDEX_INFO_LEVELS level, const LPVOID data,
			const FINDEX_SEARCH_OPS op, const LPVOID filter, const DWORD flags)
		{
			file_checked("busca", name ? name : "");
			return FindFirstFileExA(name, level, data, op, filter, flags);
		}

		HANDLE WINAPI find_first_file_ex_w(const LPCWSTR name, const FINDEX_INFO_LEVELS level, const LPVOID data,
			const FINDEX_SEARCH_OPS op, const LPVOID filter, const DWORD flags)
		{
			file_checked("busca", narrow(name));
			return FindFirstFileExW(name, level, data, op, filter, flags);
		}

		std::atomic<uint32_t> total_find_next{};

		BOOL WINAPI find_next_file_a(const HANDLE handle, const LPWIN32_FIND_DATAA data)
		{
			++total_find_next;
			return FindNextFileA(handle, data);
		}

		BOOL WINAPI find_next_file_w(const HANDLE handle, const LPWIN32_FIND_DATAW data)
		{
			++total_find_next;
			return FindNextFileW(handle, data);
		}

		void log_checks()
		{
			std::vector<std::pair<uint32_t, std::string>> list{};
			{
				std::lock_guard _(checks_mutex);
				for (const auto& [path, count] : checks)
				{
					list.emplace_back(count, path);
				}
			}

			std::ranges::sort(list, std::greater{});
			write_line(std::format("Comprobaciones de archivos: {} ({} rutas distintas), FindNextFile: {}",
				total_checks.load(), list.size(), total_find_next.load()));

			for (size_t i = 0; i < list.size() && i < 60; ++i)
			{
				write_line(std::format("  {} x {}", list[i].first, list[i].second));
			}
		}

		void log_open_files()
		{
			std::vector<std::string> lines{};
			{
				std::lock_guard _(files_mutex);
				for (const auto& [handle, info] : files)
				{
					lines.push_back(file_summary(info));
				}
			}

			for (const auto& line : lines)
			{
				write_line("sigue abierto " + line);
			}
		}


		void* wait_async_stub(void* a1)
		{
			const auto caller = reinterpret_cast<uint64_t>(_ReturnAddress());
			const auto start = elapsed();
			auto* result = wait_async_hook.invoke<void*>(a1);
			const auto time = elapsed() - start;
			if (time > 0.05)
			{
				write_line(std::format("Espera asíncrona de {:.3f} s llamada desde 0x{:X}", time, caller));
			}

			return result;
		}

		void db_load_x_assets_stub(game::XZoneInfo* zone_info, const unsigned int zone_count,
			const game::DBSyncMode sync_mode)
		{
			std::string zones{};
			for (auto i = 0u; i < zone_count; ++i)
			{
				if (!zones.empty())
				{
					zones.append(", ");
				}

				zones.append(zone_info[i].name ? zone_info[i].name : "(sin nombre)");
				zones.append(std::format(" (alloc 0x{:X} free 0x{:X})", zone_info[i].allocFlags,
					zone_info[i].freeFlags));
			}

			write_line(std::format("DB_LoadXAssets sync {}: {}", static_cast<int>(sync_mode), zones));

			const auto start = elapsed();
			db_load_x_assets_hook.invoke<void>(zone_info, zone_count, sync_mode);
			write_line(std::format("DB_LoadXAssets terminado en {:.3f} s", elapsed() - start));
		}



		std::string nearest_export(const HMODULE module, const uint64_t address)
		{
			const auto base = reinterpret_cast<uint64_t>(module);
			const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
			const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
			const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
			if (!dir.VirtualAddress)
			{
				return {};
			}

			const auto* exports = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(base + dir.VirtualAddress);
			const auto* names = reinterpret_cast<const DWORD*>(base + exports->AddressOfNames);
			const auto* ordinals = reinterpret_cast<const WORD*>(base + exports->AddressOfNameOrdinals);
			const auto* functions = reinterpret_cast<const DWORD*>(base + exports->AddressOfFunctions);

			const char* best_name = nullptr;
			uint64_t best = 0;
			for (DWORD i = 0; i < exports->NumberOfNames; ++i)
			{
				const auto function = base + functions[ordinals[i]];
				if (function <= address && function > best)
				{
					best = function;
					best_name = reinterpret_cast<const char*>(base + names[i]);
				}
			}

			if (!best_name)
			{
				return {};
			}

			return std::format("{}+0x{:X}", best_name, address - best);
		}

		std::string describe_address(const uint64_t address)
		{
			if (address >= 0x140000000 && address < 0x160000000)
			{
				return std::format("0x{:X}", address);
			}

			HMODULE module{};
			if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCSTR>(address), &module) && module)
			{
				char path[MAX_PATH]{};
				GetModuleFileNameA(module, path, sizeof(path));
				const auto name = std::filesystem::path(path).filename().string();
				const auto function = nearest_export(module, address);
				return std::format("{}+0x{:X}{}", name, address - reinterpret_cast<uint64_t>(module),
					function.empty() ? "" : " (" + function + ")");
			}

			return std::format("0x{:X}", address);
		}

		bool is_game_code(const uint64_t value)
		{
			return value >= 0x140001000 && value < 0x140785000;
		}

		int unwind(CONTEXT& context, uint64_t* frames, const int max_frames)
		{
			auto count = 0;
			__try
			{
				while (count < max_frames && context.Rip)
				{
					frames[count++] = context.Rip;
					if (is_game_code(context.Rip))
					{
						break;
					}

					DWORD64 image_base{};
					const auto* function = RtlLookupFunctionEntry(context.Rip, &image_base, nullptr);
					if (!function)
					{
						context.Rip = *reinterpret_cast<const DWORD64*>(context.Rsp);
						context.Rsp += 8;
						continue;
					}

					void* handler_data{};
					DWORD64 establisher{};
					RtlVirtualUnwind(UNW_FLAG_NHANDLER, image_base, context.Rip,
						const_cast<PRUNTIME_FUNCTION>(function), &context, &handler_data,
						&establisher, nullptr);
				}
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
			}

			return count;
		}

		std::string sample_thread(const DWORD thread_id)
		{
			const auto thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE,
				thread_id);
			if (!thread)
			{
				return "(no se puede abrir el hilo)";
			}

			auto ok = false;
			uint64_t frames[24]{};
			auto frame_count = 0;
			uint64_t stack[1024]{};
			SIZE_T read{};

			if (SuspendThread(thread) != static_cast<DWORD>(-1))
			{
				CONTEXT context{};
				context.ContextFlags = CONTEXT_FULL;
				if (GetThreadContext(thread, &context))
				{
					ok = true;

					frame_count = unwind(context, frames, 24);

					ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(context.Rsp), stack, sizeof(stack),
						&read);
				}

				ResumeThread(thread);
			}

			CloseHandle(thread);

			if (!ok)
			{
				return "(no se puede leer el hilo)";
			}

			std::string result{};
			for (auto i = 0; i < frame_count; ++i)
			{
				result += (i ? " <- " : "") + describe_address(frames[i]);
			}

			result += " | pila";
			auto count = 0;
			for (size_t i = 0; i < read / sizeof(uint64_t) && count < 10; ++i)
			{
				if (is_game_code(stack[i]))
				{
					result += std::format(" 0x{:X}", stack[i]);
					++count;
				}
			}

			return result;
		}

		void start_sampler()
		{
			std::thread([]
			{

				uint64_t last_read = 0;
				uint32_t last_checks = 0;
				uint32_t last_next = 0;
				while (elapsed() < 90.0)
				{
					std::this_thread::sleep_for(250ms);

					const auto read = total_read.load();
					const auto checks_now = total_checks.load();
					const auto next_now = total_find_next.load();
					write_line(std::format("leído en 250 ms: {:.2f} MB, comprobaciones {}, FindNextFile {}",
						static_cast<double>(read - last_read) / (1024.0 * 1024.0), checks_now - last_checks,
						next_now - last_next));
					last_read = read;
					last_checks = checks_now;
					last_next = next_now;

					if (const auto id = game_thread_id.load())
					{
						write_line("muestra principal: " + sample_thread(id));
					}

					if (const auto id = main_thread_id.load(); id && id != game_thread_id.load())
					{
						write_line("muestra frames:    " + sample_thread(id));
					}

					if (const auto id = loader_thread_id.load())
					{
						write_line("muestra carga:     " + sample_thread(id));
					}
				}

				write_line("Fin de las muestras");
				log_open_files();
				log_checks();
			}).detach();
		}
	}

	void write(const std::string& text)
	{
		write_line(text);
	}

	class component final : public component_interface
	{
	public:
		void post_start() override
		{
			open();
			write_line("post_start (cliente cargado)");

			game_thread_id = GetCurrentThreadId();
			start_sampler();
		}

		void post_unpack() override
		{
			write_line("post_unpack (juego descifrado, ganchos del cliente puestos)");

			if (game_thread_id != GetCurrentThreadId())
			{
				write_line(std::format("post_unpack en otro hilo ({})", GetCurrentThreadId()));
			}

			if (game::environment::is_dedi())
			{
				return;
			}

			db_load_x_assets_hook.create(game::DB_LoadXAssets, db_load_x_assets_stub);

			if (game::environment::is_mp())
			{
				wait_async_hook.create(0x1405A9230, wait_async_stub);
			}

			scheduler::loop([]
			{
				static auto first = true;
				static auto database_ready = false;
				static auto last = 0.0;

				const auto now = elapsed();

				if (first)
				{
					first = false;
					main_thread_id = GetCurrentThreadId();
					write_line("Primer frame del hilo principal");
				}
				else if (now < 180.0 && now - last > 0.25)
				{
					write_line(std::format("Hilo principal bloqueado {:.3f} s", now - last));
				}

				if (!database_ready && game::Sys_IsDatabaseReady2())
				{
					database_ready = true;
					write_line("Base de datos lista (Sys_IsDatabaseReady2)");
				}

				last = elapsed();
			}, scheduler::main);
		}

		void* load_import(const std::string& library, const std::string& function) override
		{
			if (_stricmp(library.data(), "KERNEL32.dll") != 0)
			{
				return nullptr;
			}

			if (function == "CreateFileA") return create_file_a;
			if (function == "CreateFileW") return create_file_w;
			if (function == "CreateFile2") return create_file_2;
			if (function == "ReadFile") return read_file;
			if (function == "ReadFileEx") return read_file_ex;
			if (function == "CloseHandle") return close_handle;
			if (function == "GetFileAttributesA") return get_file_attributes_a;
			if (function == "GetFileAttributesExW") return get_file_attributes_ex_w;
			if (function == "FindFirstFileExA") return find_first_file_ex_a;
			if (function == "FindFirstFileExW") return find_first_file_ex_w;
			if (function == "FindNextFileA") return find_next_file_a;
			if (function == "FindNextFileW") return find_next_file_w;

			return nullptr;
		}

		void pre_destroy() override
		{
			std::lock_guard _(mutex);
			if (file)
			{
				fclose(file);
				file = nullptr;
			}
		}
	};
}

REGISTER_COMPONENT(startup_log::component)
