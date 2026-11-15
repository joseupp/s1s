#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "system_check.hpp"

#include "game/game.hpp"

#include <utils/io.hpp>
#include <utils/cryptography.hpp>

namespace system_check
{
	namespace
	{
		bool is_system_valid()
		{
			return true;
		}

		void verify_binary_version()
		{
			const auto value = *reinterpret_cast<DWORD*>(0x140001337);
			if (value != 0x0FE788A4)
			{
				throw std::runtime_error("Unsupported Call of Duty: Advanced Warfare version");
			}
		}
	}

	bool is_valid()
	{
		static auto valid = is_system_valid();
		return valid;
	}

	class component final : public component_interface
	{
	public:
		void post_load() override
		{
			verify_binary_version();

			if (!is_valid())
			{
				game::show_error("Your game files are outdated or unsupported.\n"
				                 "Please get the latest officially supported Call of Duty: Advanced Warfare files, or you will get random crashes and issues.",
				                 "Invalid game files!");
			}
		}
	};
}

REGISTER_COMPONENT(system_check::component)
