#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "game/game.hpp"

#include "scheduler.hpp"
#include "startup_log.hpp"

#include <utils/concurrency.hpp>
#include <utils/hook.hpp>
#include <utils/thread.hpp>
#include <utils/string.hpp>

#include <DbgHelp.h>
#include <version.hpp>

namespace scheduler
{
	namespace
	{
		struct task
		{
			std::function<bool()> handler{};
			std::chrono::milliseconds interval{};
			std::chrono::high_resolution_clock::time_point last_call{};
		};

		using task_list = std::vector<task>;

		class task_pipeline
		{
		public:
			void add(task&& task)
			{
				new_callbacks_.access([&task](task_list& tasks)
				{
					tasks.emplace_back(std::move(task));
				});
			}

			void execute()
			{
				callbacks_.access([&](task_list& tasks)
				{
					this->merge_callbacks();

					for (auto i = tasks.begin(); i != tasks.end();)
					{
						const auto now = std::chrono::high_resolution_clock::now();
						const auto diff = now - i->last_call;

						if (diff < i->interval)
						{
							++i;
							continue;
						}

						i->last_call = now;

						const auto res = i->handler();
						if (res == cond_end)
						{
							i = tasks.erase(i);
						}
						else
						{
							++i;
						}
					}
				});
			}

		private:
			utils::concurrency::container<task_list> new_callbacks_;
			utils::concurrency::container<task_list, std::recursive_mutex> callbacks_;

			void merge_callbacks()
			{
				callbacks_.access([&](task_list& tasks)
				{
					new_callbacks_.access([&](task_list& new_tasks)
					{
						tasks.insert(tasks.end(), std::move_iterator<task_list::iterator>(new_tasks.begin()), std::move_iterator<task_list::iterator>(new_tasks.end()));
						new_tasks = {};
					});
				});
			}
		};

		volatile bool kill = false;
		std::thread thread;
		task_pipeline pipelines[pipeline::count];
		utils::hook::detour r_end_frame_hook;

		void execute(const pipeline type)
		{
			assert(type >= 0 && type < pipeline::count);
			pipelines[type].execute();
		}

		void r_end_frame_stub()
		{
			execute(pipeline::renderer);
			r_end_frame_hook.invoke<void>();
		}

		void server_frame_stub()
		{
			game::G_Glass_Update();
			execute(pipeline::server);
		}

		constexpr auto hang_timeout = 20s;
		std::atomic<std::int64_t> last_main_frame{0};
		bool hang_dumped = false;

		std::int64_t now_ms()
		{
			return std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now().time_since_epoch()).count();
		}

		void write_hang_dump()
		{
			const auto type = MiniDumpIgnoreInaccessibleMemory
				| MiniDumpWithHandleData
				| MiniDumpScanMemory
				| MiniDumpWithProcessThreadData
				| MiniDumpWithFullMemoryInfo
				| MiniDumpWithThreadInfo;

			CreateDirectoryA("minidumps", nullptr);
			const auto* file_name = utils::string::va("minidumps\\s1-mod_hang_%s_%u.dmp", SHORTVERSION,
				static_cast<std::uint32_t>(std::time(nullptr)));

			const auto file_handle = CreateFileA(file_name, GENERIC_WRITE | GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, NULL, nullptr);
			if (file_handle == INVALID_HANDLE_VALUE)
			{
				return;
			}

			MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file_handle,
				static_cast<MINIDUMP_TYPE>(type), nullptr, nullptr, nullptr);
			CloseHandle(file_handle);

			startup_log::write(utils::string::va("Cuelgue: el hilo principal lleva más de %lld s sin avanzar, volcado %s",
				static_cast<long long>(hang_timeout.count()), file_name));
		}

		void check_hang()
		{
			const auto last = last_main_frame.load();
			if (!last)
			{
				return;
			}

			const auto stalled = now_ms() - last > std::chrono::duration_cast<std::chrono::milliseconds>(hang_timeout).count();
			if (!stalled)
			{
				hang_dumped = false;
			}
			else if (!hang_dumped)
			{
				hang_dumped = true;
				write_hang_dump();
			}
		}

		void main_frame_stub()
		{
			last_main_frame = now_ms();
			execute(pipeline::main);
			game::Com_Frame_Try_Block_Function();
		}
	}

	void schedule(const std::function<bool()>& callback, const pipeline type,
	              const std::chrono::milliseconds delay)
	{
		assert(type >= 0 && type < pipeline::count);

		task task;
		task.handler = callback;
		task.interval = delay;
		task.last_call = std::chrono::high_resolution_clock::now();

		pipelines[type].add(std::move(task));
	}

	void loop(const std::function<void()>& callback, const pipeline type,
	          const std::chrono::milliseconds delay)
	{
		schedule([callback]()
		{
			callback();
			return cond_continue;
		}, type, delay);
	}

	void once(const std::function<void()>& callback, const pipeline type,
	          const std::chrono::milliseconds delay)
	{
		schedule([callback]()
		{
			callback();
			return cond_end;
		}, type, delay);
	}

	void on_game_initialized(const std::function<void()>& callback, const pipeline type,
	                         const std::chrono::milliseconds delay)
	{
		schedule([=]()
		{
			const auto dw_init = game::environment::is_sp() ? true : game::Live_SyncOnlineDataFlags(0) == 0;
			if (dw_init && game::Sys_IsDatabaseReady2())
			{
				once(callback, type, delay);
				return cond_end;
			}

			return cond_continue;
		}, pipeline::main);
	}

	class component final : public component_interface
	{
	public:
		void post_start() override
		{
			thread = utils::thread::create_named_thread("Async Scheduler", []()
			{
				while (!kill)
				{
					execute(pipeline::async);
					check_hang();
					std::this_thread::sleep_for(10ms);
				}
			});
		}

		void post_unpack() override
		{
			r_end_frame_hook.create(SELECT_VALUE(0x1404A3E20, 0x140629580), r_end_frame_stub);

			utils::hook::call(SELECT_VALUE(0x1402F7DC2, 0x14042C5C2), main_frame_stub);
			utils::hook::call(SELECT_VALUE(0x140228647, 0x14035375A), server_frame_stub);
		}

		void pre_destroy() override
		{
			kill = true;
			if (thread.joinable())
			{
				thread.join();
			}
		}
	};
}

REGISTER_COMPONENT(scheduler::component)
