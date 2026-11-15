#include <std_include.hpp>
#include "loader/component_loader.hpp"

#include "game/game.hpp"

#include "command.hpp"
#include "console.hpp"
#include "fastfiles.hpp"
#include "filesystem.hpp"
#include "scheduler.hpp"

#include <utils/hook.hpp>
#include <utils/io.hpp>
#include <utils/string.hpp>

#include <d3d11_4.h>
#include <wincodec.h>

#pragma comment(lib, "windowscodecs.lib")

namespace images
{
	namespace
	{
		constexpr auto texture_replacement_enabled = false;

		std::mutex mutex;
		ID3D11Device* device{};
		std::unordered_map<std::string, std::string> available;
		bool folder_scanned = false;

		std::mutex replaced_mutex;
		std::unordered_set<game::GfxImage*> replaced;

		utils::hook::detour load_texture_hook;
		utils::hook::detour load_texture_db_hook;
		utils::hook::detour setup_texture_hook;

		DXGI_FORMAT& image_format(game::GfxImage* image)
		{
			return *reinterpret_cast<DXGI_FORMAT*>(reinterpret_cast<char*>(image) + 0x18);
		}

		uint8_t* image_bytes(game::GfxImage* image)
		{
			return reinterpret_cast<uint8_t*>(image);
		}

		int& image_picmip(game::GfxImage* image)
		{
			return *reinterpret_cast<int*>(reinterpret_cast<char*>(image) + 0x20);
		}

		struct srgb_pair
		{
			DXGI_FORMAT typeless, linear, srgb;
		};

		constexpr srgb_pair srgb_pairs[] =
		{
			{DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB},
			{DXGI_FORMAT_B8G8R8A8_TYPELESS, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB},
			{DXGI_FORMAT_B8G8R8X8_TYPELESS, DXGI_FORMAT_B8G8R8X8_UNORM, DXGI_FORMAT_B8G8R8X8_UNORM_SRGB},
			{DXGI_FORMAT_BC1_TYPELESS, DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_BC1_UNORM_SRGB},
			{DXGI_FORMAT_BC2_TYPELESS, DXGI_FORMAT_BC2_UNORM, DXGI_FORMAT_BC2_UNORM_SRGB},
			{DXGI_FORMAT_BC3_TYPELESS, DXGI_FORMAT_BC3_UNORM, DXGI_FORMAT_BC3_UNORM_SRGB},
			{DXGI_FORMAT_BC7_TYPELESS, DXGI_FORMAT_BC7_UNORM, DXGI_FORMAT_BC7_UNORM_SRGB},
		};

		const srgb_pair* find_srgb_pair(const DXGI_FORMAT format)
		{
			for (const auto& pair : srgb_pairs)
			{
				if (pair.typeless == format || pair.linear == format || pair.srgb == format)
				{
					return &pair;
				}
			}

			return nullptr;
		}

		bool is_srgb(const DXGI_FORMAT format)
		{
			const auto* pair = find_srgb_pair(format);
			return pair && pair->srgb == format;
		}

		uint32_t block_bytes(const DXGI_FORMAT format)
		{
			switch (format)
			{
			case DXGI_FORMAT_BC1_TYPELESS:
			case DXGI_FORMAT_BC1_UNORM:
			case DXGI_FORMAT_BC1_UNORM_SRGB:
			case DXGI_FORMAT_BC4_TYPELESS:
			case DXGI_FORMAT_BC4_UNORM:
			case DXGI_FORMAT_BC4_SNORM:
				return 8;
			case DXGI_FORMAT_BC2_TYPELESS:
			case DXGI_FORMAT_BC2_UNORM:
			case DXGI_FORMAT_BC2_UNORM_SRGB:
			case DXGI_FORMAT_BC3_TYPELESS:
			case DXGI_FORMAT_BC3_UNORM:
			case DXGI_FORMAT_BC3_UNORM_SRGB:
			case DXGI_FORMAT_BC5_TYPELESS:
			case DXGI_FORMAT_BC5_UNORM:
			case DXGI_FORMAT_BC5_SNORM:
			case DXGI_FORMAT_BC6H_TYPELESS:
			case DXGI_FORMAT_BC6H_UF16:
			case DXGI_FORMAT_BC6H_SF16:
			case DXGI_FORMAT_BC7_TYPELESS:
			case DXGI_FORMAT_BC7_UNORM:
			case DXGI_FORMAT_BC7_UNORM_SRGB:
				return 16;
			default:
				return 0;
			}
		}

		uint32_t bits_per_pixel(const DXGI_FORMAT format)
		{
			switch (format)
			{
			case DXGI_FORMAT_R32G32B32A32_TYPELESS:
			case DXGI_FORMAT_R32G32B32A32_FLOAT:
			case DXGI_FORMAT_R32G32B32A32_UINT:
			case DXGI_FORMAT_R32G32B32A32_SINT:
				return 128;
			case DXGI_FORMAT_R16G16B16A16_TYPELESS:
			case DXGI_FORMAT_R16G16B16A16_FLOAT:
			case DXGI_FORMAT_R16G16B16A16_UNORM:
			case DXGI_FORMAT_R16G16B16A16_UINT:
			case DXGI_FORMAT_R16G16B16A16_SNORM:
			case DXGI_FORMAT_R16G16B16A16_SINT:
			case DXGI_FORMAT_R32G32_TYPELESS:
			case DXGI_FORMAT_R32G32_FLOAT:
				return 64;
			case DXGI_FORMAT_R10G10B10A2_TYPELESS:
			case DXGI_FORMAT_R10G10B10A2_UNORM:
			case DXGI_FORMAT_R11G11B10_FLOAT:
			case DXGI_FORMAT_R8G8B8A8_TYPELESS:
			case DXGI_FORMAT_R8G8B8A8_UNORM:
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
			case DXGI_FORMAT_R8G8B8A8_SNORM:
			case DXGI_FORMAT_B8G8R8A8_TYPELESS:
			case DXGI_FORMAT_B8G8R8A8_UNORM:
			case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			case DXGI_FORMAT_B8G8R8X8_TYPELESS:
			case DXGI_FORMAT_B8G8R8X8_UNORM:
			case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
			case DXGI_FORMAT_R16G16_TYPELESS:
			case DXGI_FORMAT_R16G16_FLOAT:
			case DXGI_FORMAT_R16G16_UNORM:
			case DXGI_FORMAT_R16G16_SNORM:
			case DXGI_FORMAT_R32_TYPELESS:
			case DXGI_FORMAT_R32_FLOAT:
			case DXGI_FORMAT_R32_UINT:
				return 32;
			case DXGI_FORMAT_R8G8_TYPELESS:
			case DXGI_FORMAT_R8G8_UNORM:
			case DXGI_FORMAT_R8G8_SNORM:
			case DXGI_FORMAT_R16_TYPELESS:
			case DXGI_FORMAT_R16_FLOAT:
			case DXGI_FORMAT_R16_UNORM:
			case DXGI_FORMAT_B5G6R5_UNORM:
			case DXGI_FORMAT_B5G5R5A1_UNORM:
			case DXGI_FORMAT_B4G4R4A4_UNORM:
				return 16;
			case DXGI_FORMAT_R8_TYPELESS:
			case DXGI_FORMAT_R8_UNORM:
			case DXGI_FORMAT_R8_SNORM:
			case DXGI_FORMAT_A8_UNORM:
				return 8;
			default:
				return 0;
			}
		}

		bool level_layout(const DXGI_FORMAT format, const uint32_t width, const uint32_t height, uint32_t& row_bytes,
			uint32_t& rows)
		{
			if (const auto bb = block_bytes(format))
			{
				row_bytes = std::max(1u, (width + 3) / 4) * bb;
				rows = std::max(1u, (height + 3) / 4);
				return true;
			}

			if (const auto bpp = bits_per_pixel(format))
			{
				row_bytes = (width * bpp + 7) / 8;
				rows = height;
				return true;
			}

			return false;
		}

#pragma pack(push, 1)
		struct dds_pixel_format
		{
			uint32_t size, flags, four_cc, rgb_bit_count, r_mask, g_mask, b_mask, a_mask;
		};

		struct dds_header
		{
			uint32_t size, flags, height, width, pitch_or_linear_size, depth, mip_map_count, reserved1[11];
			dds_pixel_format ddspf;
			uint32_t caps, caps2, caps3, caps4, reserved2;
		};

		struct dds_header_dx10
		{
			uint32_t dxgi_format, resource_dimension, misc_flag, array_size, misc_flags2;
		};
#pragma pack(pop)

		static_assert(sizeof(dds_header) == 124);

		constexpr uint32_t dds_magic = 0x20534444;
		constexpr uint32_t make_four_cc(const char a, const char b, const char c, const char d)
		{
			return static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8) | (static_cast<uint32_t>(c) << 16) |
				(static_cast<uint32_t>(d) << 24);
		}

		DXGI_FORMAT legacy_dds_format(const dds_pixel_format& pf)
		{
			if (pf.flags & 0x4)
			{
				switch (pf.four_cc)
				{
				case make_four_cc('D', 'X', 'T', '1'): return DXGI_FORMAT_BC1_UNORM;
				case make_four_cc('D', 'X', 'T', '2'):
				case make_four_cc('D', 'X', 'T', '3'): return DXGI_FORMAT_BC2_UNORM;
				case make_four_cc('D', 'X', 'T', '4'):
				case make_four_cc('D', 'X', 'T', '5'): return DXGI_FORMAT_BC3_UNORM;
				case make_four_cc('A', 'T', 'I', '1'):
				case make_four_cc('B', 'C', '4', 'U'): return DXGI_FORMAT_BC4_UNORM;
				case make_four_cc('B', 'C', '4', 'S'): return DXGI_FORMAT_BC4_SNORM;
				case make_four_cc('A', 'T', 'I', '2'):
				case make_four_cc('B', 'C', '5', 'U'): return DXGI_FORMAT_BC5_UNORM;
				case make_four_cc('B', 'C', '5', 'S'): return DXGI_FORMAT_BC5_SNORM;
				case 36: return DXGI_FORMAT_R16G16B16A16_UNORM;
				case 113: return DXGI_FORMAT_R16G16B16A16_FLOAT;
				case 116: return DXGI_FORMAT_R32G32B32A32_FLOAT;
				default: return DXGI_FORMAT_UNKNOWN;
				}
			}

			if (pf.flags & 0x40)
			{
				if (pf.rgb_bit_count == 32)
				{
					if (pf.r_mask == 0x000000FF && pf.g_mask == 0x0000FF00 && pf.b_mask == 0x00FF0000)
					{
						return DXGI_FORMAT_R8G8B8A8_UNORM;
					}

					if (pf.r_mask == 0x00FF0000 && pf.g_mask == 0x0000FF00 && pf.b_mask == 0x000000FF)
					{
						return (pf.flags & 0x1) && pf.a_mask ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_B8G8R8X8_UNORM;
					}
				}

				if (pf.rgb_bit_count == 16 && pf.r_mask == 0xF800 && pf.g_mask == 0x07E0 && pf.b_mask == 0x001F)
				{
					return DXGI_FORMAT_B5G6R5_UNORM;
				}

				return DXGI_FORMAT_UNKNOWN;
			}

			if ((pf.flags & 0x20000) && pf.rgb_bit_count == 8)
			{
				return DXGI_FORMAT_R8_UNORM;
			}

			if ((pf.flags & 0x2) && pf.rgb_bit_count == 8)
			{
				return DXGI_FORMAT_A8_UNORM;
			}

			return DXGI_FORMAT_UNKNOWN;
		}

		struct texture_data
		{
			DXGI_FORMAT format{};
			uint32_t width{}, height{};
			std::vector<std::string> levels{};
			std::vector<uint32_t> pitches{};
		};

		bool parse_dds(const std::string& name, const std::string& data, texture_data& out)
		{
			if (data.size() < 4 + sizeof(dds_header) || *reinterpret_cast<const uint32_t*>(data.data()) != dds_magic)
			{
				console::error("[images] %s: no es un DDS\n", name.data());
				return false;
			}

			const auto* header = reinterpret_cast<const dds_header*>(data.data() + 4);
			auto offset = 4 + sizeof(dds_header);

			auto format = DXGI_FORMAT_UNKNOWN;
			if ((header->ddspf.flags & 0x4) && header->ddspf.four_cc == make_four_cc('D', 'X', '1', '0'))
			{
				if (data.size() < offset + sizeof(dds_header_dx10))
				{
					console::error("[images] %s: DDS cortado\n", name.data());
					return false;
				}

				const auto* dx10 = reinterpret_cast<const dds_header_dx10*>(data.data() + offset);
				offset += sizeof(dds_header_dx10);

				if (dx10->resource_dimension != 3 || dx10->array_size > 1 || (dx10->misc_flag & 0x4))
				{
					console::error("[images] %s: solo DDS 2D (sin cubemap ni array)\n", name.data());
					return false;
				}

				format = static_cast<DXGI_FORMAT>(dx10->dxgi_format);
			}
			else
			{
				if ((header->caps2 & 0x200) || (header->flags & 0x800000))
				{
					console::error("[images] %s: solo DDS 2D (sin cubemap ni volumen)\n", name.data());
					return false;
				}

				format = legacy_dds_format(header->ddspf);
			}

			out.format = format;
			out.width = header->width;
			out.height = header->height;

			const auto mips = std::max(1u, header->mip_map_count);
			auto width = out.width;
			auto height = out.height;

			for (auto i = 0u; i < mips; ++i)
			{
				uint32_t row_bytes{}, rows{};
				if (!level_layout(format, width, height, row_bytes, rows))
				{
					console::error("[images] %s: formato DDS no soportado (%d)\n", name.data(), format);
					return false;
				}

				const size_t size = static_cast<size_t>(row_bytes) * rows;
				if (offset + size > data.size())
				{
					if (i == 0)
					{
						console::error("[images] %s: DDS cortado\n", name.data());
						return false;
					}

					break;
				}

				out.levels.emplace_back(data.data() + offset, size);
				out.pitches.push_back(row_bytes);
				offset += size;

				width = std::max(1u, width / 2);
				height = std::max(1u, height / 2);
			}

			return true;
		}

		bool decode_png(const std::string& name, const std::string& data, uint32_t& width, uint32_t& height,
			std::string& rgba)
		{
			const auto com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
			const auto _ = gsl::finally([com]
			{
				if (SUCCEEDED(com))
				{
					CoUninitialize();
				}
			});

			auto ok = false;
			{
				CComPtr<IWICImagingFactory> factory{};
				CComPtr<IWICStream> stream{};
				CComPtr<IWICBitmapDecoder> decoder{};
				CComPtr<IWICBitmapFrameDecode> frame{};
				CComPtr<IWICFormatConverter> converter{};

				ok = SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
						IID_PPV_ARGS(&factory)))
					&& SUCCEEDED(factory->CreateStream(&stream))
					&& SUCCEEDED(stream->InitializeFromMemory(
						reinterpret_cast<BYTE*>(const_cast<char*>(data.data())), static_cast<DWORD>(data.size())))
					&& SUCCEEDED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand,
						&decoder))
					&& SUCCEEDED(decoder->GetFrame(0, &frame))
					&& SUCCEEDED(factory->CreateFormatConverter(&converter))
					&& SUCCEEDED(converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
						nullptr, 0.0, WICBitmapPaletteTypeCustom))
					&& SUCCEEDED(converter->GetSize(&width, &height));

				if (ok)
				{
					rgba.resize(static_cast<size_t>(width) * height * 4);
					ok = SUCCEEDED(converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(rgba.size()),
						reinterpret_cast<BYTE*>(rgba.data())));
				}
			}

			if (!ok)
			{
				console::error("[images] %s: PNG no válido\n", name.data());
			}

			return ok;
		}

		void build_mips(texture_data& tex)
		{
			auto width = tex.width;
			auto height = tex.height;

			while (width > 1 || height > 1)
			{
				const auto& src = tex.levels.back();
				const auto new_width = std::max(1u, width / 2);
				const auto new_height = std::max(1u, height / 2);

				std::string dst(static_cast<size_t>(new_width) * new_height * 4, '\0');

				for (auto y = 0u; y < new_height; ++y)
				{
					for (auto x = 0u; x < new_width; ++x)
					{
						for (auto c = 0u; c < 4; ++c)
						{
							auto sum = 0u;
							for (auto oy = 0u; oy < 2; ++oy)
							{
								for (auto ox = 0u; ox < 2; ++ox)
								{
									const auto sx = std::min(width - 1, x * 2 + ox);
									const auto sy = std::min(height - 1, y * 2 + oy);
									sum += static_cast<uint8_t>(src[(static_cast<size_t>(sy) * width + sx) * 4 + c]);
								}
							}

							dst[(static_cast<size_t>(y) * new_width + x) * 4 + c] = static_cast<char>((sum + 2) / 4);
						}
					}
				}

				tex.levels.push_back(std::move(dst));
				tex.pitches.push_back(new_width * 4);
				width = new_width;
				height = new_height;
			}
		}

		std::string file_name(const char* image_name)
		{
			auto name = utils::string::to_lower(image_name);
			for (auto& c : name)
			{
				if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' ||
					c == '*' || static_cast<unsigned char>(c) < 32)
				{
					c = '_';
				}
			}

			return name;
		}

		bool matches(const std::string& filter, const char* image_name)
		{
			const auto pattern = utils::string::to_lower(filter);
			const auto name = utils::string::to_lower(image_name);

			if (pattern.find('*') == std::string::npos)
			{
				return name.find(pattern) != std::string::npos;
			}

			size_t p = 0, n = 0, star = std::string::npos, mark = 0;
			while (n < name.size())
			{
				if (p < pattern.size() && pattern[p] == name[n])
				{
					++p;
					++n;
				}
				else if (p < pattern.size() && pattern[p] == '*')
				{
					star = p++;
					mark = n;
				}
				else if (star != std::string::npos)
				{
					p = star + 1;
					n = ++mark;
				}
				else
				{
					return false;
				}
			}

			while (p < pattern.size() && pattern[p] == '*')
			{
				++p;
			}

			return p == pattern.size();
		}


		void scan_folder()
		{
			available.clear();
			filesystem::exists("images");

			std::unordered_map<std::string, std::string> png{};
			for (const auto& search_path : filesystem::get_search_paths())
			{
				const auto dir = std::filesystem::path(search_path) / "images";
				std::error_code ec{};
				if (!std::filesystem::is_directory(dir, ec))
				{
					continue;
				}

				for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, ec))
				{
					if (!entry.is_regular_file(ec))
					{
						continue;
					}

					const auto& path = entry.path();
					const auto ext = utils::string::to_lower(path.extension().string());
					const auto name = utils::string::to_lower(path.stem().string());

					if (ext == ".dds")
					{
						available.emplace(name, path.string());
					}
					else if (ext == ".png")
					{
						png.emplace(name, path.string());
					}
				}
			}

			const auto dds = available.size();
			for (auto& [name, path] : png)
			{
				available.emplace(name, std::move(path));
			}

			console::info("[images] %zu DDS y %zu PNG en images\n", dds, available.size() - dds);
		}

		bool has_texture_2d(const game::GfxImage* image)
		{
			auto* view = image->textures.shaderView;
			if (!view || !image->textures.___u0.map)
			{
				return false;
			}

			D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
			view->GetDesc(&desc);
			return desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D;
		}

		bool find_device()
		{
			if (device)
			{
				return true;
			}

			fastfiles::enum_assets(game::ASSET_TYPE_IMAGE, [&](const game::XAssetHeader header)
			{
				if (device || !header.image || !has_texture_2d(header.image))
				{
					return;
				}

				header.image->textures.shaderView->GetDevice(&device);
			}, true);

			return device != nullptr;
		}


		void ensure_folder()
		{
			if (!folder_scanned)
			{
				folder_scanned = true;
				scan_folder();
			}
		}

		std::optional<std::string> find_file(const char* image_name)
		{
			std::lock_guard _(mutex);
			ensure_folder();

			if (available.empty())
			{
				return {};
			}

			const auto it = available.find(file_name(image_name));
			if (it == available.end())
			{
				return {};
			}

			return it->second;
		}

		bool load_texture_data(const std::string& name, const std::string& path, texture_data& td)
		{
			std::string data{};
			if (!utils::io::read_file(path, &data))
			{
				console::error("[images] No se puede leer %s\n", path.data());
				return false;
			}

			const auto is_dds = utils::string::to_lower(std::filesystem::path(path).extension().string()) == ".dds";
			if (is_dds)
			{
				return parse_dds(name, data, td);
			}

			std::string rgba{};
			if (!decode_png(name, data, td.width, td.height, rgba))
			{
				return false;
			}

			td.format = DXGI_FORMAT_R8G8B8A8_UNORM;
			td.levels.push_back(std::move(rgba));
			td.pitches.push_back(td.width * 4);
			build_mips(td);
			return true;
		}

		uint32_t full_mip_count(uint32_t width, uint32_t height)
		{
			auto count = 1u;
			while (width > 1 || height > 1)
			{
				width = std::max(1u, width / 2);
				height = std::max(1u, height / 2);
				++count;
			}

			return count;
		}

		bool load_custom_texture(game::GfxImage* image)
		{
			if (!image || !image->name || !*image->name)
			{
				return false;
			}

			const auto path = find_file(image->name);
			if (!path)
			{
				return false;
			}

			const auto name = file_name(image->name);

			texture_data td{};
			if (!load_texture_data(name, *path, td))
			{
				return false;
			}

			auto format = td.format;
			if (const auto* pair = find_srgb_pair(format))
			{
				format = is_srgb(image_format(image)) ? pair->srgb : pair->linear;
			}

			auto flags = 0u;
			auto levels = static_cast<uint32_t>(td.levels.size());
			if (levels != full_mip_count(td.width, td.height))
			{
				if (levels > 1)
				{
					console::warn("[images] %s: mips incompletos, se usa solo el más grande\n", name.data());
				}

				flags = 2;
				levels = 1;
			}

			std::vector<D3D11_SUBRESOURCE_DATA> init(levels);
			for (auto i = 0u; i < levels; ++i)
			{
				init[i].pSysMem = td.levels[i].data();
				init[i].SysMemPitch = td.pitches[i];
				init[i].SysMemSlicePitch = static_cast<UINT>(td.levels[i].size());
			}

			auto* bytes = image_bytes(image);
			bytes[0x1C] = 3;
			bytes[0x1D] = 2;
			bytes[0x1E] = 3;
			bytes[0x1F] = 0;

			game::Image_Setup(image, td.width, td.height, 1, 1, flags, format, image->name, init.data());

			if (!image->textures.shaderView)
			{
				console::error("[images] %s: Image_Setup no ha creado la textura (formato %d)\n", name.data(), format);
				return false;
			}

			image_picmip(image) = -1;
			{
				std::lock_guard _(replaced_mutex);
				replaced.insert(image);
			}

			console::info("[images] %s cambiada (%ux%u, formato %d, %u mips)\n", name.data(), td.width, td.height,
				format, levels);
			return true;
		}

		bool try_load_custom_texture(game::GfxImage* image)
		{
			try
			{
				return load_custom_texture(image);
			}
			catch (const std::exception& e)
			{
				console::error("[images] Error al cargar %s: %s\n", image->name, e.what());
			}

			return false;
		}

		void load_texture_stub(game::GfxImage* image, const uint64_t level, const uint64_t flag)
		{
			if (try_load_custom_texture(image))
			{
				return;
			}

			load_texture_hook.invoke<void>(image, level, flag);
		}

		void load_texture_db_stub(void* a1, game::GfxImage* image)
		{
			if (try_load_custom_texture(image))
			{
				return;
			}

			{
				std::lock_guard _(replaced_mutex);
				replaced.erase(image);
			}

			load_texture_db_hook.invoke<void>(a1, image);
		}

		bool setup_texture_stub(game::GfxImage* image, const uint64_t level, const uint64_t flag)
		{
			if (image_picmip(image) == -1)
			{
				std::lock_guard _(replaced_mutex);
				if (replaced.contains(image))
				{
					return true;
				}
			}

			return setup_texture_hook.invoke<bool>(image, level, flag);
		}

		bool dump_image(ID3D11DeviceContext* context, const game::GfxImage* image, const std::filesystem::path& dir)
		{
			const auto fail = [&](const char* reason, const HRESULT hr = S_OK)
			{
				console::warn("[images] %s: %s (hr 0x%08X)\n", image->name, reason, static_cast<unsigned>(hr));
				return false;
			};

			if (!has_texture_2d(image))
			{
				return fail("sin textura 2D cargada");
			}

			auto* source = image->textures.___u0.map;
			D3D11_TEXTURE2D_DESC desc{};
			source->GetDesc(&desc);

			if (desc.ArraySize != 1 || desc.SampleDesc.Count != 1)
			{
				return fail("array o multisample");
			}

			D3D11_SHADER_RESOURCE_VIEW_DESC view_desc{};
			image->textures.shaderView->GetDesc(&view_desc);
			const auto format = view_desc.Format != DXGI_FORMAT_UNKNOWN ? view_desc.Format : desc.Format;

			uint32_t row_bytes{}, rows{};
			if (!level_layout(format, desc.Width, desc.Height, row_bytes, rows))
			{
				return fail("formato no soportado");
			}

			auto staging_desc = desc;
			staging_desc.Usage = D3D11_USAGE_STAGING;
			staging_desc.BindFlags = 0;
			staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			staging_desc.MiscFlags = 0;

			CComPtr<ID3D11Texture2D> staging{};
			if (const auto hr = device->CreateTexture2D(&staging_desc, nullptr, &staging); FAILED(hr))
			{
				return fail("CreateTexture2D", hr);
			}

			context->CopyResource(staging, source);

			std::string pixels{};
			auto width = desc.Width;
			auto height = desc.Height;
			for (auto mip = 0u; mip < desc.MipLevels; ++mip)
			{
				level_layout(format, width, height, row_bytes, rows);

				D3D11_MAPPED_SUBRESOURCE mapped{};
				if (const auto hr = context->Map(staging, mip, D3D11_MAP_READ, 0, &mapped); FAILED(hr))
				{
					return fail("Map", hr);
				}

				for (auto r = 0u; r < rows; ++r)
				{
					pixels.append(static_cast<const char*>(mapped.pData) + static_cast<size_t>(r) * mapped.RowPitch,
						row_bytes);
				}

				context->Unmap(staging, mip);

				width = std::max(1u, width / 2);
				height = std::max(1u, height / 2);
			}

			dds_header header{};
			header.size = sizeof(dds_header);
			header.flags = 0x1 | 0x2 | 0x4 | 0x1000 | 0x20000;
			header.height = desc.Height;
			header.width = desc.Width;
			header.mip_map_count = desc.MipLevels;
			header.ddspf.size = sizeof(dds_pixel_format);
			header.ddspf.flags = 0x4;
			header.ddspf.four_cc = make_four_cc('D', 'X', '1', '0');
			header.caps = 0x1000 | (desc.MipLevels > 1 ? 0x400008 : 0);

			dds_header_dx10 dx10{};
			dx10.dxgi_format = format;
			dx10.resource_dimension = 3;
			dx10.array_size = 1;

			std::string file{};
			file.append(reinterpret_cast<const char*>(&dds_magic), sizeof(dds_magic));
			file.append(reinterpret_cast<const char*>(&header), sizeof(header));
			file.append(reinterpret_cast<const char*>(&dx10), sizeof(dx10));
			file.append(pixels);

			const auto path = dir / (file_name(image->name) + ".dds");
			return utils::io::write_file(path.string(), file) || fail("no se puede escribir el archivo");
		}

		void dump(const std::string& filter)
		{
			if (!find_device())
			{
				console::error("[images] Sin dispositivo D3D11\n");
				return;
			}

			CComPtr<ID3D11DeviceContext> context{};
			device->GetImmediateContext(&context);

			CComPtr<ID3D11Multithread> multithread{};
			if (FAILED(context->QueryInterface(__uuidof(ID3D11Multithread), reinterpret_cast<void**>(&multithread))))
			{
				console::error("[images] El contexto D3D11 no se puede proteger para varios hilos\n");
				return;
			}

			const std::filesystem::path dir = "s1/images_dump";
			std::error_code ec{};
			std::filesystem::create_directories(dir, ec);

			const auto was_protected = multithread->SetMultithreadProtected(TRUE);

			game::R_SyncRenderThread();
			std::this_thread::sleep_for(50ms);

			std::unordered_set<std::string> ours{};
			{
				std::lock_guard _(mutex);
				for (const auto& [name, path] : available)
				{
					ours.insert(name);
				}
			}

			std::vector<game::GfxImage*> list{};
			fastfiles::enum_assets(game::ASSET_TYPE_IMAGE, [&](const game::XAssetHeader header)
			{
				auto* image = header.image;
				if (image && image->name && (filter.empty() || matches(filter, image->name)))
				{
					list.push_back(image);
				}
			}, true);

			auto saved = 0, failed = 0;
			for (const auto* image : list)
			{
				if (ours.contains(file_name(image->name)))
				{
					continue;
				}

				multithread->Enter();
				const auto ok = dump_image(context, image, dir);
				multithread->Leave();

				ok ? ++saved : ++failed;
			}

			if (!was_protected)
			{
				multithread->Enter();
				multithread->Leave();
				multithread->SetMultithreadProtected(FALSE);
			}

			console::info("[images] %d imágenes guardadas en s1\\images_dump (%d sin poder guardar)\n", saved, failed);
		}

		void list_images(const std::string& filter)
		{
			std::vector<std::string> lines{};
			fastfiles::enum_assets(game::ASSET_TYPE_IMAGE, [&](const game::XAssetHeader header)
			{
				const auto* image = header.image;
				if (!image || !image->name || (!filter.empty() && !matches(filter, image->name)))
				{
					return;
				}

				auto format = -1;
				if (image->textures.shaderView)
				{
					D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
					image->textures.shaderView->GetDesc(&desc);
					format = desc.Format;
				}

				lines.push_back(utils::string::va("%s\t%dx%d\tformato %d", file_name(image->name).data(),
					image->width, image->height, format));
			}, true);

			std::ranges::sort(lines);

			std::string file{};
			for (const auto& line : lines)
			{
				console::info("%s\n", line.data());
				file.append(line).append("\n");
			}

			utils::io::write_file("s1/images_list.txt", file);
			console::info("[images] %zu imágenes (lista en s1\\images_list.txt)\n", lines.size());
		}


		void reload()
		{
			std::lock_guard _(mutex);
			folder_scanned = true;
			scan_folder();
		}
	}

	class component final : public component_interface
	{
	public:
		void post_unpack() override
		{
			if (game::environment::is_dedi() || game::environment::is_sp())
			{
				return;
			}

			if (texture_replacement_enabled)
			{
				setup_texture_hook.create(0x1400A6B40, setup_texture_stub);
				if (game::port_pending) load_texture_hook.create(0x140053040, load_texture_stub);
				load_texture_db_hook.create(0x140608B10, load_texture_db_stub);
			}

			command::add("images_reload", reload);

			command::add("images_list", [](const command::params& params)
			{
				const std::string filter = params.size() > 1 ? params.join(1) : "";
				scheduler::once([filter]
				{
					list_images(filter);
				}, scheduler::main);
			});

			command::add("images_dump", [](const command::params& params)
			{
				const std::string filter = params.size() > 1 ? params.join(1) : "";
				scheduler::once([filter]
				{
					dump(filter);
				}, scheduler::main);
			});
		}
	};
}

REGISTER_COMPONENT(images::component)
