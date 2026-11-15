#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "game/game.hpp"

#include "party.hpp"
#include "console.hpp"

#include <utils/hook.hpp>

namespace logger
{
	namespace
	{
		utils::hook::detour com_error_hook;

		game::dvar_t* logger_dev = nullptr;

		void print_error(const char* msg, ...)
		{
			char buffer[2048]{};
			va_list ap;

			va_start(ap, msg);
			vsnprintf_s(buffer, _TRUNCATE, msg, ap);
			va_end(ap);

			console::error("%s", buffer);
		}

		void print_com_error(int, const char* msg, ...)
		{
			char buffer[2048]{};
			va_list ap;

			va_start(ap, msg);
			vsnprintf_s(buffer, _TRUNCATE, msg, ap);
			va_end(ap);

			console::error("%s", buffer);
		}

		void com_error_stub(const int error, const char* msg, ...)
		{
			char buffer[2048]{};
			va_list ap;

			va_start(ap, msg);
			vsnprintf_s(buffer, _TRUNCATE, msg, ap);
			va_end(ap);

			console::error("Error: %s\n", buffer);

			party::clear_sv_motd(); // clear sv_motd on error if it exists

			com_error_hook.invoke<void>(error, "%s", buffer);
		}

		void print_warning(const char* msg, ...)
		{
			char buffer[2048]{};
			va_list ap;

			va_start(ap, msg);
			vsnprintf_s(buffer, _TRUNCATE, msg, ap);
			va_end(ap);

			console::warn("%s", buffer);
		}

		void print(const char* msg, ...)
		{
			char buffer[2048]{};
			va_list ap;

			va_start(ap, msg);
			vsnprintf_s(buffer, _TRUNCATE, msg, ap);
			va_end(ap);

			console::info("%s", buffer);
		}

		void print_dev(const char* msg, ...)
		{
			if (!logger_dev->current.enabled)
			{
				return;
			}

			char buffer[2048]{};
			va_list ap;

			va_start(ap, msg);
			vsnprintf_s(buffer, _TRUNCATE, msg, ap);
			va_end(ap);

			console::info("%s", buffer);
		}

		// nullsub_56
		void nullsub_56()
		{
			utils::hook::call(0x14012AD02, print_warning);
			utils::hook::call(0x14012AD0E, print_warning);
			utils::hook::call(0x14012AD16, print_warning);
			utils::hook::call(0x14012AD22, print_warning);

			utils::hook::call(0x14012B17D, print_warning);
			utils::hook::call(0x14012B189, print_warning);
			utils::hook::call(0x14012B191, print_warning);
			utils::hook::call(0x14012B19D, print_warning);

			utils::hook::call(0x1401310AF, print_warning);
			utils::hook::call(0x1401310BB, print_warning);
			utils::hook::call(0x1401310C3, print_warning);
			utils::hook::call(0x1401310CF, print_warning);

			utils::hook::call(0x140131D23, print_warning);
			utils::hook::call(0x140131D37, print_warning);

			utils::hook::call(0x140132438, print_warning);
			utils::hook::call(0x140132458, print_warning);
			utils::hook::call(0x140132464, print_warning);

			utils::hook::call(0x1401324EE, print_warning);
			utils::hook::call(0x140132528, print_warning);
			utils::hook::call(0x140132534, print_warning);
			utils::hook::call(0x140132548, print_warning);

			utils::hook::call(0x140134A88, print_warning);
			utils::hook::call(0x140134CA1, print_warning);
		}

		// sub_1400E7420
		void sub_1400E7420()
		{
			utils::hook::call(0x140126861, print_warning);
			utils::hook::call(0x14012A83E, print_warning);
			utils::hook::call(0x14012AACE, print_warning);
			utils::hook::call(0x14012F9F8, print_dev);
			utils::hook::call(0x140131474, print_dev);
			utils::hook::call(0x14013471A, print_dev);
			utils::hook::call(0x14013571C, print_dev);
		}
	}

	class component final : public component_interface
	{
	public:
		void post_unpack() override
		{
			if (game::environment::is_mp())
			{
				nullsub_56();
				sub_1400E7420();

				// Make havok script's print function actually print
				utils::hook::jump(0x140122D80, print);
			}

			if (!game::environment::is_sp())
			{
				utils::hook::call(0x14053B8F3, print_com_error);
			}

			com_error_hook.create(game::Com_Error, com_error_stub);

			logger_dev = game::Dvar_RegisterBool("logger_dev", false, game::DVAR_FLAG_SAVED, "Print dev stuff");
		}
	};
}

REGISTER_COMPONENT(logger::component)
