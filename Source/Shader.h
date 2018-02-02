#pragma once

#include <Rush/GfxCommon.h>
#include <Rush/GfxDevice.h>
#include <Rush/GfxRef.h>

struct ShaderResourceBinding
{
	ShaderResourceBinding(GfxBindingType _type = GfxBindingType_Unknown, u32 _offset = ~0u, u32 _size = 0)
	: type(GfxShaderBindings::BindingType(_type)), offset(_offset), size(_size)
	{
	}

	ShaderResourceBinding(
	    GfxShaderBindings::BindingType _type = GfxShaderBindings::BindingType_Unknown, u32 _offset = ~0u, u32 _size = 0)
	: type(_type), offset(_offset), size(_size)
	{
	}

	GfxShaderBindings::BindingType type   = GfxShaderBindings::BindingType_Unknown;
	u32                            offset = ~0u;
	u32                            size   = 0;
};

struct ShaderBingingsBuilder : GfxShaderBindings
{
	ShaderBingingsBuilder(std::initializer_list<ShaderResourceBinding> bindings)
	{
		u32 bindingIndices[255] = {};

		u32 bindingIndex = 0;
		for (const auto& binding : bindings)
		{
			u32 counterIndex = binding.type;
			if (binding.type == BindingType_RWTypedBuffer)
			{
				counterIndex = BindingType_RWBuffer;
			}

			GfxShaderBindings::Item item;
			item.name  = nullptr;
			item.data  = nullptr;
			item.count = 1;
			item.idx   = bindingIndices[counterIndex];
			item.type  = binding.type;
			resourceOffsets.pushBack(binding.offset);

			u32 resourceSize = 0;
			switch (binding.type)
			{
			case BindingType_PushConstants: resourceSize = binding.size; break;
			case BindingType_ConstantBuffer:
			case BindingType_CombinedSampler:
			case BindingType_Sampler:
			case BindingType_Texture:
			case BindingType_RWImage:
			case BindingType_RWBuffer:
			case BindingType_RWTypedBuffer: resourceSize = (u32)sizeof(UntypedResourceHandle); break;
			default: Log::error("Unsupported binding type");
			}

			resourceSizes.pushBack(resourceSize);
			bindingIndices[counterIndex]++;

			if (binding.type == GfxShaderBindings::BindingType_PushConstants)
			{
				pushConstantBindingIndex      = bindingIndex;
				item.pushConstants.size       = binding.size;
				item.pushConstants.stageFlags = GfxStageFlags::Compute;
			}

			items.pushBack(item);

			bindingIndex++;
		}
	}

	void setResources(GfxContext* ctx, const void* resources)
	{
		u32 itemCount = (u32)items.size();
		for (u32 i = 0; i < itemCount; ++i)
		{
			const auto& binding = items[i];
			GfxBuffer   buffer  = {};
			switch (binding.type)
			{
			case BindingType_PushConstants:
				// nothing
				break;
			case BindingType_ConstantBuffer:
				memcpy(&buffer, (u8*)resources + resourceOffsets[i], resourceSizes[i]);
				Gfx_SetConstantBuffer(ctx, binding.idx, buffer);
				break;
			case BindingType_RWBuffer:
			case BindingType_RWTypedBuffer:
				memcpy(&buffer, (u8*)resources + resourceOffsets[i], resourceSizes[i]);
				Gfx_SetStorageBuffer(ctx, GfxStage::Compute, binding.idx, buffer);
				break;
			default: Log::error("Unsupported binding type");
			}
		}
	}

	StaticArray<u32, 128> resourceOffsets;
	StaticArray<u32, 128> resourceSizes;

	u32 pushConstantBindingIndex = ~0u;
};

class ComputeShader
{
public:
	ComputeShader(const GfxShaderSource& source, std::initializer_list<ShaderResourceBinding> bindings)
	: m_bindings(bindings)
	{
		auto cs = Gfx_CreateComputeShader(source);
		m_technique.takeover(Gfx_CreateTechnique(GfxTechniqueDesc(cs, &m_bindings)));
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
