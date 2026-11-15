#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "scheduler.hpp"

#include "game/game.hpp"

#include "steam/interface.hpp"
#include "steam/steam.hpp"

#include <utils/nt.hpp>
#include <utils/string.hpp>
#include <utils/cryptography.hpp>
#include <utils/io.hpp>
#include <utils/smbios.hpp>

namespace steam_proxy
{
	namespace
	{
		utils::nt::library steam_client_module_{};

		steam::interface client_engine_{};
		steam::interface client_user_{};
		steam::interface client_utils_{};

		void* steam_pipe_ = nullptr;
		void* global_user_ = nullptr;

		enum class ownership_state
		{
			success,
			unowned,
			nosteam,
			error,
		};

		const char* license_path = "s1/players2/license.dat";

		size_t get_app_id()
		{
			return game::environment::is_sp() ? 209650 : 209660;
		}

		bool is_steam_running()
		{
			HKEY key{};
			if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam\\ActiveProcess", 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
			{
				return false;
			}

			DWORD pid{};
			DWORD size = sizeof(pid);
			const auto result = RegQueryValueExA(key, "pid", nullptr, nullptr, reinterpret_cast<BYTE*>(&pid), &size);
			RegCloseKey(key);
			if (result != ERROR_SUCCESS || !pid)
			{
				return false;
			}

			if (utils::nt::is_wine())
			{
				return true;
			}

			const auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
			if (!process)
			{
				return false;
			}

			DWORD exit_code{};
			const auto alive = GetExitCodeProcess(process, &exit_code) && exit_code == STILL_ACTIVE;
			CloseHandle(process);
			return alive;
		}

		void* load_client_engine()
		{
			if (!steam_client_module_) return nullptr;

			for (auto i = 1; i < 1000; ++i)
			{
				const auto* name = utils::string::va("CLIENTENGINE_INTERFACE_VERSION%03i", i);
				auto* const client_engine = steam_client_module_.invoke<void*>("CreateInterface", name, nullptr);
				if (client_engine) return client_engine;
			}

			return nullptr;
		}

		void load_client()
		{
			const auto app_id = get_app_id();
			SetEnvironmentVariableA("SteamAppId", utils::string::va("%zu", app_id));
			SetEnvironmentVariableA("SteamGameId", utils::string::va("%zu", app_id & 0xFFFFFF));

			const std::filesystem::path steam_path = steam::SteamAPI_GetSteamInstallPath();
			if (steam_path.empty()) return;

			utils::nt::library::load(steam_path / "tier0_s64.dll");
			utils::nt::library::load(steam_path / "vstdlib_s64.dll");
			steam_client_module_ = utils::nt::library::load(steam_path / "steamclient64.dll");
			if (!steam_client_module_) return;

			client_engine_ = load_client_engine();
			if (!client_engine_) return;

			steam_pipe_ = steam_client_module_.invoke<void*>("Steam_CreateSteamPipe");
			global_user_ = steam_client_module_.invoke<void*>("Steam_ConnectToGlobalUser", steam_pipe_);
			client_user_ = client_engine_.invoke<void*>(8, steam_pipe_, global_user_); // GetIClientUser
			client_utils_ = client_engine_.invoke<void*>(14, steam_pipe_); // GetIClientUtils
		}

		void do_cleanup()
		{
			client_engine_ = nullptr;
			client_user_ = nullptr;
			client_utils_ = nullptr;

			steam_pipe_ = nullptr;
			global_user_ = nullptr;
		}

		bool perform_cleanup_if_needed()
		{
			if (steam_client_module_
				&& steam_pipe_
				&& global_user_
				&& steam_client_module_.invoke<bool>("Steam_BConnected", global_user_, steam_pipe_)
				&& steam_client_module_.invoke<bool>("Steam_BLoggedOn", global_user_, steam_pipe_))
			{
				return false;
			}

			do_cleanup();
			return true;
		}

		void clean_up_on_error()
		{
			scheduler::schedule([]()
			{
				return perform_cleanup_if_needed() ? scheduler::cond_end : scheduler::cond_continue;
			}, scheduler::pipeline::main);
		}

		std::string make_license(const size_t app_id)
		{
			return utils::cryptography::sha256::compute(
				utils::string::va("s1-license|%zu|%s", app_id, utils::smbios::get_uuid().data()), true);
		}

		bool has_saved_license(const size_t app_id)
		{
			std::string data{};
			return utils::io::read_file(license_path, &data) && data == make_license(app_id);
		}

		void save_license(const size_t app_id)
		{
			utils::io::write_file(license_path, make_license(app_id), false);
		}

		ownership_state start_mod_unsafe(const size_t app_id)
		{
			if (!client_utils_ || !client_user_)
			{
				return has_saved_license(app_id) ? ownership_state::success : ownership_state::nosteam;
			}

			if (!client_user_.invoke<bool>("BIsSubscribedApp", app_id))
			{
				std::remove(license_path);
				return ownership_state::unowned;
			}

			save_license(app_id);
			client_utils_.invoke<void>("SetAppIDForCurrentPipe", app_id, false);

			return ownership_state::success;
		}

		ownership_state start_mod(const size_t app_id)
		{
			try
			{
				return start_mod_unsafe(app_id);
			}
			catch (const std::exception& e)
			{
				do_cleanup();
				printf("Steam: %s\n", e.what());
				return ownership_state::error;
			}
		}

		[[noreturn]] void fail(const char* message)
		{
			do_cleanup();
			printf("Steam: %s\n", message);

			if (auto* const splash_window = FindWindowA("S1 Splash Screen", nullptr))
			{
				DWORD_PTR result{};
				SendMessageTimeoutA(splash_window, WM_CLOSE, 0, 0, SMTO_ABORTIFHUNG, 1000, &result);
			}

			MessageBoxA(nullptr, message, "S1", MB_ICONERROR | MB_SYSTEMMODAL | MB_TOPMOST | MB_SETFOREGROUND);
			TerminateProcess(GetCurrentProcess(), 1234);
			std::abort();
		}
	}

	class component final : public component_interface
	{
	public:
		void post_load() override
		{
			if (game::environment::is_dedi() || !is_steam_running())
			{
				return;
			}

			load_client();
			perform_cleanup_if_needed();
		}

		void post_unpack() override
		{
			if (game::environment::is_dedi())
			{
				return;
			}

			switch (start_mod(get_app_id()))
			{
			case ownership_state::nosteam:
				fail("Steam must be running the first time to verify that you own the game!");
			case ownership_state::unowned:
				fail("You must own the game on Steam to play this mod!");
			case ownership_state::error:
				fail("Failed to verify ownership of the game, please try again!");
			case ownership_state::success:
				break;
			}

			clean_up_on_error();
		}

		void pre_destroy() override
		{
			if (steam_client_module_ && steam_pipe_)
			{
				if (global_user_)
				{
					steam_client_module_.invoke<void>("Steam_ReleaseUser", steam_pipe_, global_user_);
				}

				steam_client_module_.invoke<bool>("Steam_BReleaseSteamPipe", steam_pipe_);
			}
		}
	};
}

REGISTER_COMPONENT(steam_proxy::component)
