#pragma once

#include <Rush/GfxCommon.h>
#include <Rush/GfxDevice.h>
#include <Rush/GfxRef.h>

enum GfxBindingType
{
	GfxBindingType_Unknown,

	GfxBindingType_PushConstants,
	GfxBindingType_ConstantBuffer,
	GfxBindingType_Sampler,
	GfxBindingType_Texture,
	GfxBindingType_RWImage,
	GfxBindingType_RWBuffer,
	GfxBindingType_RWTypedBuffer,
};

struct ShaderResourceBinding
{
	ShaderResourceBinding(GfxBindingType _type = GfxBindingType_Unknown, u32 _offset = ~0u, u32 _size = 0)
	: type(_type), offset(_offset), size(_size)
	{
	}

	GfxBindingType type   = GfxBindingType_Unknown;
	u32            offset = ~0u;
	u32            size   = 0;
};

struct ShaderBingingsBuilder
{
	struct Item
	{
		const char* name;
		const void* data;
		union {
			struct
			{
				u32           size;
				GfxStageFlags stageFlags;
			} pushConstants;
			struct
			{
				u32 count;
				u32 idx;
			};
		};

		GfxBindingType type;
	};

	ShaderBingingsBuilder() = default;
	ShaderBingingsBuilder(std::initializer_list<ShaderResourceBinding> bindings);

	bool addConstantBuffer(const char* name, u32 idx);
	bool addSampler(const char* name, u32 idx);
	bool addTexture(const char* name, u32 idx);
	bool addStorageImage(const char* name, u32 idx);
	bool addStorageBuffer(const char* name, u32 idx) { return addRWBuffer(name, idx); }
	bool addPushConstants(const char* name, GfxStageFlags stageFlags, u32 size);

	bool addRWBuffer(const char* name, u32 idx);
	bool addTypedRWBuffer(const char* name, u32 idx);

	void setResources(GfxContext* ctx, const void* resources);

	StaticArray<u32, 128> resourceOffsets;
	StaticArray<u32, 128> resourceSizes;

	u32 pushConstantBindingIndex = ~0u;

	StaticArray<Item, 128> items;

	GfxShaderBindingDesc desc;
};

class ComputeShader
{
public:
	ComputeShader(const GfxShaderSource& source, std::initializer_list<ShaderResourceBinding> bindings)
	: m_bindings(bindings)
	{
		auto cs = Gfx_CreateComputeShader(source);
		m_technique.takeover(Gfx_CreateTechnique(GfxTechniqueDesc(cs, m_bindings.desc)));
		Gfx_Release(cs);
	}

	void dispatch(GfxContext* ctx, u32 x, u32 y = 1, u32 z = 1)
	{
		Gfx_SetTechnique(ctx, m_technique);

		m_bindings.setResources(ctx, this);

		if (m_bindings.pushConstantBindingIndex != ~0u)
		{
			const auto& pushConstantsBinding = m_bindings.items[m_bindings.pushConstantBindingIndex];
			const void* pushConstants =
			    (const u8*)this + m_bindings.resourceOffsets[m_bindings.pushConstantBindingIndex];
			u32 pushConstantsSize = m_bindings.resourceSizes[m_bindings.pushConstantBindingIndex];
			Gfx_Dispatch(ctx, x, y, z, pushConstants, pushConstantsSize);
		}
		else
		{
			Gfx_Dispatch(ctx, x, y, z);
		}
	}

	void dispatchIndirect(GfxContext* ctx, GfxBuffer args, u32 argsOffset = 0)
	{
		Gfx_SetTechnique(ctx, m_technique);

		m_bindings.setResources(ctx, this);

		if (m_bindings.pushConstantBindingIndex != ~0u)
		{
			const auto& pushConstantsBinding = m_bindings.items[m_bindings.pushConstantBindingIndex];
			const void* pushConstants =
			    (const u8*)this + m_bindings.resourceOffsets[m_bindings.pushConstantBindingIndex];
			u32 pushConstantsSize = m_bindings.resourceSizes[m_bindings.pushConstantBindingIndex];
			Gfx_DispatchIndirect(ctx, args, argsOffset, pushConstants, pushConstantsSize);
		}
		else
		{
			Gfx_DispatchIndirect(ctx, args, argsOffset);
		}
	}

private:
	GfxTechniqueRef       m_technique;
	ShaderBingingsBuilder m_bindings;
};
