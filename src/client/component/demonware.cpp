#include <std_include.hpp>
#include "loader/component_loader.hpp"

#include <utils/hook.hpp>
#include <utils/string.hpp>
#include <utils/io.hpp>
#include <utils/thread.hpp>

#include "game/game.hpp"
#include "startup_log.hpp"
#include "game/demonware/servers/lobby_server.hpp"
#include "game/demonware/servers/auth3_server.hpp"
#include "game/demonware/servers/stun_server.hpp"
#include "game/demonware/servers/umbrella_server.hpp"
#include "game/demonware/server_registry.hpp"

#define TCP_BLOCKING true
#define UDP_BLOCKING false

namespace demonware
{
	namespace
	{
		volatile bool exit_server;
		std::thread server_thread;
		utils::concurrency::container<std::unordered_map<SOCKET, bool>> blocking_sockets;
		utils::concurrency::container<std::unordered_map<SOCKET, tcp_server*>> socket_map;
		server_registry<tcp_server> tcp_servers;
		server_registry<udp_server> udp_servers;

		tcp_server* find_server(const SOCKET socket)
		{
			return socket_map.access<tcp_server*>([&](const std::unordered_map<SOCKET, tcp_server*>& map) -> tcp_server*
			{
				const auto entry = map.find(socket);
				if (entry == map.end())
				{
					return nullptr;
				}

				return entry->second;
			});
		}

		bool socket_link(const SOCKET socket, const uint32_t address)
		{
			auto* server = tcp_servers.find(address);
			if (!server)
			{
				return false;
			}

			socket_map.access([&](std::unordered_map<SOCKET, tcp_server*>& map)
			{
				map[socket] = server;
			});

			return true;
		}

		void socket_unlink(const SOCKET socket)
		{
			socket_map.access([&](std::unordered_map<SOCKET, tcp_server*>& map)
			{
				const auto entry = map.find(socket);
				if (entry != map.end())
				{
					map.erase(entry);
				}
			});
		}

		bool is_socket_blocking(const SOCKET socket, const bool def)
		{
			return blocking_sockets.access<bool>([&](std::unordered_map<SOCKET, bool>& map)
			{
				const auto entry = map.find(socket);
				if (entry == map.end())
				{
					return def;
				}

				return entry->second;
			});
		}

		void remove_blocking_socket(const SOCKET socket)
		{
			blocking_sockets.access([&](std::unordered_map<SOCKET, bool>& map)
			{
				const auto entry = map.find(socket);
				if (entry != map.end())
				{
					map.erase(entry);
				}
			});
		}

		void add_blocking_socket(const SOCKET socket, const bool block)
		{
			blocking_sockets.access([&](std::unordered_map<SOCKET, bool>& map)
			{
				map[socket] = block;
			});
		}

		void server_main()
		{
			exit_server = false;

			while (!exit_server)
			{
				tcp_servers.frame();
				udp_servers.frame();
				std::this_thread::sleep_for(50ms);
			}
		}

		namespace io
		{
			hostent* WINAPI gethostbyname_stub(const char* name)
			{
#ifdef DEBUG
				printf("[ network ]: [gethostbyname]: \"%s\"\n", name);
#endif

				base_server* server = tcp_servers.find(name);
				if (!server)
				{
					server = udp_servers.find(name);
				}

				if (!server)
				{
#pragma warning(push)
#pragma warning(disable: 4996)
					return gethostbyname(name);
#pragma warning(pop)
				}

				static thread_local in_addr address{};
				address.s_addr = server->get_address();

				static thread_local in_addr* addr_list[2]{};
				addr_list[0] = &address;
				addr_list[1] = nullptr;

				static thread_local hostent host{};
				host.h_name = const_cast<char*>(name);
				host.h_aliases = nullptr;
				host.h_addrtype = AF_INET;
				host.h_length = sizeof(in_addr);
				host.h_addr_list = reinterpret_cast<char**>(addr_list);

				return &host;
			}

			struct fake_addrinfo
			{
				addrinfo info;
				sockaddr_in address;
			};

			std::mutex fake_addrinfo_mutex;
			std::unordered_set<addrinfo*> fake_addrinfos;

			INT WSAAPI getaddrinfo_stub(const PCSTR name, const PCSTR service, const ADDRINFOA* hints, PADDRINFOA* result)
			{
				base_server* server = name ? tcp_servers.find(name) : nullptr;
				if (!server && name)
				{
					server = udp_servers.find(name);
				}

				startup_log::write(std::format("[DW] getaddrinfo {}:{} -> {}", name ? name : "(null)",
					service ? service : "(null)", server ? "interno" : "real"));

				if (!server || (hints && hints->ai_family != AF_UNSPEC && hints->ai_family != AF_INET))
				{
					return getaddrinfo(name, service, hints, result);
				}

				auto* entry = new fake_addrinfo{};
				entry->address.sin_family = AF_INET;
				entry->address.sin_addr.s_addr = server->get_address();
				entry->address.sin_port = htons(static_cast<u_short>(service ? atoi(service) : 0));

				entry->info.ai_family = AF_INET;
				entry->info.ai_socktype = hints ? hints->ai_socktype : 0;
				entry->info.ai_protocol = hints ? hints->ai_protocol : 0;
				entry->info.ai_addrlen = sizeof(sockaddr_in);
				entry->info.ai_addr = reinterpret_cast<sockaddr*>(&entry->address);

				{
					std::lock_guard _(fake_addrinfo_mutex);
					fake_addrinfos.insert(&entry->info);
				}

				*result = &entry->info;
				return 0;
			}

			VOID WSAAPI freeaddrinfo_stub(const PADDRINFOA info)
			{
				{
					std::lock_guard _(fake_addrinfo_mutex);
					if (fake_addrinfos.erase(info))
					{
						delete reinterpret_cast<fake_addrinfo*>(info);
						return;
					}
				}

				freeaddrinfo(info);
			}

			int WINAPI connect_stub(const SOCKET s, const sockaddr* addr, const int len)
			{
				if (len == sizeof(sockaddr_in))
				{
					const auto* in_addr = reinterpret_cast<const sockaddr_in*>(addr);
					if (socket_link(s, in_addr->sin_addr.s_addr)) return 0;
				}

				return connect(s, addr, len);
			}

			int WINAPI closesocket_stub(const SOCKET s)
			{
				remove_blocking_socket(s);
				socket_unlink(s);

				return closesocket(s);
			}

			int WINAPI send_stub(const SOCKET s, const char* buf, const int len, const int flags)
			{
				auto* server = find_server(s);

				if (server)
				{
					server->handle_input(buf, len);
					return len;
				}

				return send(s, buf, len, flags);
			}

			int WINAPI recv_stub(const SOCKET s, char* buf, const int len, const int flags)
			{
				auto* server = find_server(s);

				if (server)
				{
					if (server->pending_data())
					{
						return static_cast<int>(server->handle_output(buf, len));
					}
					else
					{
						WSASetLastError(WSAEWOULDBLOCK);
						return -1;
					}
				}

				return recv(s, buf, len, flags);
			}

			int WINAPI sendto_stub(const SOCKET s, const char* buf, const int len, const int flags, const sockaddr* to,
			                      const int tolen)
			{
				const auto* in_addr = reinterpret_cast<const sockaddr_in*>(to);
				auto* server = udp_servers.find(in_addr->sin_addr.s_addr);

				if (server)
				{
					server->handle_input(buf, len, {s, to, tolen});
					return len;
				}

				return sendto(s, buf, len, flags, to, tolen);
			}

			int WINAPI recvfrom_stub(const SOCKET s, char* buf, const int len, const int flags, sockaddr* from,
			                        int* fromlen)
			{
				// Not supported yet
				if (is_socket_blocking(s, UDP_BLOCKING))
				{
					return recvfrom(s, buf, len, flags, from, fromlen);
				}

				size_t result = 0;
				udp_servers.for_each([&](udp_server& server)
				{
					if (server.pending_data(s))
					{
						result = server.handle_output(
							s, buf, static_cast<size_t>(len), from, fromlen);
					}
				});

				if (result)
				{
					return static_cast<int>(result);
				}

				return recvfrom(s, buf, len, flags, from, fromlen);
			}

			int WINAPI select_stub(const int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds,
			                      timeval* timeout)
			{
				if (exit_server)
				{
					return select(nfds, readfds, writefds, exceptfds, timeout);
				}

				auto result = 0;
				std::vector<SOCKET> read_sockets;
				std::vector<SOCKET> write_sockets;

				socket_map.access([&](std::unordered_map<SOCKET, tcp_server*>& sockets)
				{
					for (auto& s : sockets)
					{
						if (readfds)
						{
							if (FD_ISSET(s.first, readfds))
							{
								if (s.second->pending_data())
								{
									read_sockets.push_back(s.first);
									FD_CLR(s.first, readfds);
								}
							}
						}

						if (writefds)
						{
							if (FD_ISSET(s.first, writefds))
							{
								write_sockets.push_back(s.first);
								FD_CLR(s.first, writefds);
							}
						}

						if (exceptfds)
						{
							if (FD_ISSET(s.first, exceptfds))
							{
								FD_CLR(s.first, exceptfds);
							}
						}
					}
				});

				if ((!readfds || readfds->fd_count == 0) && (!writefds || writefds->fd_count == 0))
				{
					timeout->tv_sec = 0;
					timeout->tv_usec = 0;
				}

				result = select(nfds, readfds, writefds, exceptfds, timeout);
				if (result < 0) result = 0;

				for (const auto& socket : read_sockets)
				{
					if (readfds)
					{
						FD_SET(socket, readfds);
						result++;
					}
				}

				for (const auto& socket : write_sockets)
				{
					if (writefds)
					{
						FD_SET(socket, writefds);
						result++;
					}
				}

				return result;
			}

			int WINAPI ioctlsocket_stub(const SOCKET s, const long cmd, u_long* argp)
			{
				if (static_cast<unsigned long>(cmd) == (FIONBIO))
				{
					add_blocking_socket(s, *argp == 0);
				}

				return ioctlsocket(s, cmd, argp);
			}

			BOOL internet_get_connected_state_stub(LPDWORD, DWORD)
			{
				// Allow offline play
				return TRUE;
			}
		}

		void bd_logger_stub(const char* const function, const char* const msg, ...)
		{
			static const auto* bd_logger_enabled = game::Dvar_RegisterBool("bd_logger_enabled", false, game::DVAR_FLAG_NONE, "Enable bdLogger");
			if (!bd_logger_enabled->current.enabled)
			{
				return;
			}

			char buffer[2048];

			va_list ap;
			va_start(ap, msg);

			vsnprintf_s(buffer, _TRUNCATE, msg, ap);
			printf("%s: %s\n", function, buffer);

			va_end(ap);
		}

		void startup_dw()
		{
			udp_servers.create<stun_server>("s1-stun.us.demonware.net");
			udp_servers.create<stun_server>("s1-stun.eu.demonware.net");
			udp_servers.create<stun_server>("s1-stun.jp.demonware.net");
			udp_servers.create<stun_server>("s1-stun.au.demonware.net");

			udp_servers.create<stun_server>("stun.us.demonware.net");
			udp_servers.create<stun_server>("stun.eu.demonware.net");
			udp_servers.create<stun_server>("stun.jp.demonware.net");
			udp_servers.create<stun_server>("stun.au.demonware.net");

			tcp_servers.create<auth3_server>("aw-pc-auth3.prod.demonware.net");
			tcp_servers.create<lobby_server>("aw-pc-lobby.prod.demonware.net");
			tcp_servers.create<umbrella_server>("prod.umbrella.demonware.net");
		}

		utils::hook::detour bd_task_status_hook;

		bool is_valid_task(const void* task)
		{
			const auto address = reinterpret_cast<std::uintptr_t>(task);
			if (address & 7)
			{
				return false;
			}

			MEMORY_BASIC_INFORMATION info{};
			if (!VirtualQuery(task, &info, sizeof(info)) || info.State != MEM_COMMIT ||
				(info.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
					PAGE_EXECUTE_READWRITE)) == 0 || (info.Protect & PAGE_GUARD))
			{
				return false;
			}

			const auto vtable = *reinterpret_cast<const std::uintptr_t*>(task);
			return (vtable & 7) == 0 && vtable >= 0x140792000 && vtable < 0x1408FE600;
		}

		int bd_task_status_stub(std::uint8_t* slot)
		{
			auto*& task = *reinterpret_cast<void**>(slot + 0x8);
			if (task && !is_valid_task(task))
			{
				printf("[ demonware ]: tarea de LiveStorage inválida (%p, tipo %d), se da por fallida\n", task,
					*reinterpret_cast<int*>(slot + 0x18));
				task = nullptr;
				return 2;
			}

			return bd_task_status_hook.invoke<int>(slot);
		}
	}

	class component final : public component_interface
	{
	public:
		void post_load() override
		{
			startup_dw();

			server_thread = utils::thread::create_named_thread("Demonware", server_main);
		}

		void* load_import(const std::string& library, const std::string& function) override
		{
			if (library == "WS2_32.dll")
			{
				if (function == "#3") return io::closesocket_stub;
				if (function == "#4") return io::connect_stub;
				if (function == "#10") return io::ioctlsocket_stub;
				if (function == "#16") return io::recv_stub;
				if (function == "#17") return io::recvfrom_stub;
				if (function == "#18") return io::select_stub;
				if (function == "#19") return io::send_stub;
				if (function == "#20") return io::sendto_stub;
				if (function == "#52") return io::gethostbyname_stub;
				if (function == "getaddrinfo") return io::getaddrinfo_stub;
				if (function == "freeaddrinfo") return io::freeaddrinfo_stub;
			}

			if (function == "InternetGetConnectedState")
			{
				return io::internet_get_connected_state_stub;
			}

			return nullptr;
		}

		void post_unpack() override
		{
			utils::hook::jump(SELECT_VALUE(0x140575880, 0x1406F3AA0), bd_logger_stub);

			if (game::environment::is_sp())
			{
				utils::hook::set<uint8_t>(0x1405C9350, 0xC3); // bdAuthSteam
				if (game::port_pending) utils::hook::set<uint8_t>(0x1402DF2C0, 0xC3); // dwNet
				return;
			}

			if (game::port_pending) utils::hook::set<uint8_t>(0x140698BB2, 0x0); // CURLOPT_SSL_VERIFYPEER
			if (game::port_pending) utils::hook::set<uint8_t>(0x140698B69, 0xAF); // CURLOPT_SSL_VERIFYHOST
			utils::hook::set<uint8_t>(0x14083D984, 0x0); // HTTPS -> HTTP

			const auto write_string = [](const size_t address, const char* text)
			{
				utils::hook::copy(address, text, std::strlen(text) + 1);
			};

			write_string(0x140826D90, "http://%s:%d/auth/");
			write_string(0x140796E80, "http://prod.umbrella.demonware.net/v1.0/");
			write_string(0x140796EB0, "http://prod.umbrella.demonware.net/v1.0/");
			write_string(0x140796EE0, "http://prod.umbrella.demonware.net/v1.0/");

			utils::hook::set<uint8_t>(0x140495180, 0xC3); // SV_SendMatchData
			utils::hook::set<uint8_t>(0x1405C6940, 0xC3); // Live_CheckForFullDisconnect

			bd_task_status_hook.create(0x1405A4BC0, bd_task_status_stub);

			utils::hook::set<std::uint8_t>(0x140423060, 0xC3);
		}

		void pre_destroy() override
		{
			exit_server = true;
			if (server_thread.joinable())
			{
				server_thread.join();
			}
		}
	};
}

REGISTER_COMPONENT(demonware::component)
