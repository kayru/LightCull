#include "Utils.h"

#include <Rush/UtilLog.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize.h>

#include <gli/dx.hpp>
#include <gli/load_dds.hpp>

std::string directoryFromFilename(const std::string& filename)
{
	size_t pos = filename.find_last_of("/\\");
	if (pos != std::string::npos)
	{
		return filename.substr(0, pos + 1);
	}
	else
	{
		return std::string();
	}
}

bool endsWith(const std::string& value, const std::string& suffix)
{
	if (suffix.length() > value.length())
		return false;

	return std::equal(suffix.rbegin(), suffix.rend(), value.rbegin());
}

GfxTextureRef loadBitmap(const char* filename, bool flipY)
{
	GfxTextureRef texture;

	int w, h, comp;
	stbi_set_flip_vertically_on_load(flipY);
	u8* pixels = stbi_load(filename, &w, &h, &comp, 4);

	if (pixels)
	{
		std::vector<std::unique_ptr<u8>> mips;
		mips.reserve(16);

		std::vector<GfxTextureData> textureData;
		textureData.reserve(16);
		textureData.push_back(makeTextureData(pixels, 0));

		u32 mipWidth  = w;
		u32 mipHeight = h;

		while (mipWidth != 1 && mipHeight != 1)
		{
			u32 nextMipWidth  = max<u32>(1, mipWidth / 2);
			u32 nextMipHeight = max<u32>(1, mipHeight / 2);

			u8* nextMip = new u8[nextMipWidth * nextMipHeight * 4];
			mips.push_back(std::unique_ptr<u8>(nextMip));

			const u32 mipPitch     = mipWidth * 4;
			const u32 nextMipPitch = nextMipWidth * 4;
			int resizeResult = stbir_resize_uint8((const u8*)textureData.back().pixels, mipWidth, mipHeight, mipPitch,
			    nextMip, nextMipWidth, nextMipHeight, nextMipPitch, 4);
			RUSH_ASSERT(resizeResult);

			textureData.push_back(makeTextureData(nextMip, (u32)textureData.size()));

			mipWidth  = nextMipWidth;
			mipHeight = nextMipHeight;
		}

		GfxTextureDesc desc = GfxTextureDesc::make2D(w, h);
		desc.mips           = (u32)textureData.size();

		texture.takeover(Gfx_CreateTexture(desc, textureData.data(), (u32)textureData.size()));

		stbi_image_free(pixels);
	}

	return texture;
}

GfxTextureRef loadDDS(const char* filename)
{
	GfxTextureRef result;

	FileIn stream(filename);
	if (!stream.valid())
	{
		return result;
	}

	std::vector<char> textureBuffer;
	textureBuffer.resize(stream.length());

	stream.read(textureBuffer.data(), (u32)textureBuffer.size());

	gli::texture texture = gli::load_dds(textureBuffer.data(), textureBuffer.size());

	const u32 levelCount = (u32)texture.levels();
	const u32 faceCount  = (u32)texture.faces();

	static const gli::dx formatTranslator;

	gli::dx::format dxFormat = formatTranslator.translate(texture.format());

	std::vector<GfxTextureData> textureData;
	textureData.reserve(levelCount * faceCount);

	auto textureExtent = texture.extent(0);

	GfxTextureDesc desc;

	switch (texture.target())
	{
	case gli::TARGET_2D: desc.type = TextureType::Tex2D; break;
	case gli::TARGET_CUBE: desc.type = TextureType::TexCube; break;
	default: Log::error("Unsupported texture type."); return result;
	}

	desc.width  = textureExtent.x;
	desc.height = textureExtent.y;
	desc.depth  = 1;
	desc.mips   = levelCount;

	bool requireConversion = false;

	switch (dxFormat.DXGIFormat.DDS)
	{
	default:
		Log::error("Unsupported texture format");
		desc.format = GfxFormat_Unknown;
		return result;
	case gli::dx::DXGI_FORMAT_D24_UNORM_S8_UINT:
		desc.format       = GfxFormat_R32_Float;
		requireConversion = true;
		break;
	case gli::dx::DXGI_FORMAT_BC3_UNORM: desc.format = GfxFormat_BC3_Unorm; break;
	case gli::dx::DXGI_FORMAT_BC3_UNORM_SRGB: desc.format = GfxFormat_BC3_Unorm_sRGB; break;
	case gli::dx::DXGI_FORMAT_BC4_UNORM: desc.format = GfxFormat_BC4_Unorm; break;
	case gli::dx::DXGI_FORMAT_BC6H_UF16: desc.format = GfxFormat_BC6H_UFloat; break;
	case gli::dx::DXGI_FORMAT_BC6H_SF16: desc.format = GfxFormat_BC6H_SFloat; break;
	case gli::dx::DXGI_FORMAT_BC7_UNORM: desc.format = GfxFormat_BC7_Unorm; break;
	case gli::dx::DXGI_FORMAT_BC7_UNORM_SRGB: desc.format = GfxFormat_BC7_Unorm_sRGB; break;
	case gli::dx::DXGI_FORMAT_B8G8R8A8_UNORM: desc.format = GfxFormat_BGRA8_Unorm; break;
	}

	typedef std::vector<char>     TextureLevelData;
	std::vector<TextureLevelData> convertedLevels;

	if (requireConversion)
	{
		convertedLevels.reserve(levelCount * faceCount);
	}

	for (u32 levelIt = 0; levelIt < levelCount; ++levelIt)
	{
		auto levelExtent = texture.extent(levelIt);
		for (u32 faceIt = 0; faceIt < faceCount; ++faceIt)
		{
			auto levelData = texture.data(0, faceIt, levelIt);

			GfxTextureData td;
			td.slice  = faceIt;
			td.mip    = levelIt;
			td.width  = levelExtent.x;
			td.height = levelExtent.y;
			td.depth  = levelExtent.z;

			if (requireConversion)
			{
				u32 pixelCount = td.width * td.height * td.depth;

				convertedLevels.push_back(TextureLevelData());
				TextureLevelData& convertedLevelData = convertedLevels.back();

				convertedLevelData.resize((pixelCount * getBitsPerPixel(desc.format)) / 8);

				if (dxFormat.DXGIFormat.DDS == gli::dx::DXGI_FORMAT_D24_UNORM_S8_UINT &&
				    desc.format == GfxFormat_R32_Float)
				{
					const u32* srcData = reinterpret_cast<const u32*>(levelData);
					float*     dstData = reinterpret_cast<float*>(convertedLevelData.data());

					for (u32 i = 0; i < pixelCount; ++i)
					{
						dstData[i] = float(srcData[i] & 0x00FFFFFF) / float(0xFFFFFF);
					}
				}
				else
				{
					Log::error("Unsupported texture format");
				}

				td.pixels = convertedLevelData.data();
			}
			else
			{
				td.pixels = levelData;
			}

			textureData.push_back(td);
		}
	}

	result.takeover(Gfx_CreateTexture(desc, textureData.data(), (u32)textureData.size()));

	return result;
}
