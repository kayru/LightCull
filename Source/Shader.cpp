#include "Shader.h"

ShaderBingingsBuilder::ShaderBingingsBuilder(std::initializer_list<ShaderResourceBinding> bindings)
{
	u32 bindingIndices[255] = {};

	u32 bindingIndex = 0;
	for (const auto& binding : bindings)
	{
		u32 counterIndex = binding.type;
		if (binding.type == GfxBindingType_RWTypedBuffer)
		{
			counterIndex = GfxBindingType_RWBuffer;
		}

		Item item;
		item.name  = nullptr;
		item.data  = nullptr;
		item.count = 1;
		item.idx   = bindingIndices[counterIndex];
		item.type  = binding.type;
		resourceOffsets.pushBack(binding.offset);

		u32 resourceSize = 0;
		switch (binding.type)
		{
		case GfxBindingType_PushConstants: resourceSize = binding.size; break;
		case GfxBindingType_ConstantBuffer:
		case GfxBindingType_Sampler:
		case GfxBindingType_Texture:
		case GfxBindingType_RWImage:
		case GfxBindingType_RWBuffer:
		case GfxBindingType_RWTypedBuffer: resourceSize = (u32)sizeof(UntypedResourceHandle); break;
		default: Log::error("Unsupported binding type");
		}

		resourceSizes.pushBack(resourceSize);
		bindingIndices[counterIndex]++;

		if (binding.type == GfxBindingType_PushConstants)
		{
			pushConstantBindingIndex      = bindingIndex;
			item.pushConstants.size       = binding.size;
			item.pushConstants.stageFlags = GfxStageFlags::Compute;
		}

		items.pushBack(item);

		bindingIndex++;
	}
}

void ShaderBingingsBuilder::setResources(GfxContext* ctx, const void* resources)
{
	u32 itemCount = (u32)items.size();
	for (u32 i = 0; i < itemCount; ++i)
	{
		const auto& binding = items[i];
		GfxBuffer   buffer  = {};
		switch (binding.type)
		{
		case GfxBindingType_PushConstants:
			// nothing
			break;
		case GfxBindingType_ConstantBuffer:
			memcpy(&buffer, (u8*)resources + resourceOffsets[i], resourceSizes[i]);
			Gfx_SetConstantBuffer(ctx, binding.idx, buffer);
			break;
		case GfxBindingType_RWBuffer:
		case GfxBindingType_RWTypedBuffer:
			memcpy(&buffer, (u8*)resources + resourceOffsets[i], resourceSizes[i]);
			Gfx_SetStorageBuffer(ctx, binding.idx, buffer);
			break;
		default: Log::error("Unsupported binding type");
		}
	}
}

bool ShaderBingingsBuilder::addConstantBuffer(const char* name, u32 idx)
{
	desc.constantBuffers++;

	Item it;
	it.name  = name;
	it.data  = nullptr;
	it.count = 1;
	it.idx   = idx;
	it.type  = GfxBindingType_ConstantBuffer;
	return items.pushBack(it);
}

bool ShaderBingingsBuilder::addTexture(const char* name, u32 idx)
{
	desc.textures++;

	Item it;
	it.name  = name;
	it.data  = nullptr;
	it.count = 1;
	it.idx   = idx;
	it.type  = GfxBindingType_Texture;
	return items.pushBack(it);
}

bool ShaderBingingsBuilder::addSampler(const char* name, u32 idx)
{
	desc.samplers++;

	Item it;
	it.name  = name;
	it.data  = nullptr;
	it.count = 1;
	it.idx   = idx;
	it.type  = GfxBindingType_Sampler;
	return items.pushBack(it);
}

bool ShaderBingingsBuilder::addStorageImage(const char* name, u32 idx)
{
	desc.rwImages++;

	Item it;
	it.name  = name;
	it.data  = nullptr;
	it.count = 1;
	it.idx   = idx;
	it.type  = GfxBindingType_RWImage;
	return items.pushBack(it);
}

bool ShaderBingingsBuilder::addRWBuffer(const char* name, u32 idx)
{
	desc.rwBuffers++;

	Item it;
	it.name  = name;
	it.data  = nullptr;
	it.count = 1;
	it.idx   = idx;
	it.type  = GfxBindingType_RWBuffer;
	return items.pushBack(it);
}

bool ShaderBingingsBuilder::addTypedRWBuffer(const char* name, u32 idx)
{
	desc.rwTypedBuffers++;

	Item it;
	it.name  = name;
	it.data  = nullptr;
	it.count = 1;
	it.idx   = idx;
	it.type  = GfxBindingType_RWTypedBuffer;
	return items.pushBack(it);
}

bool ShaderBingingsBuilder::addPushConstants(const char* name, GfxStageFlags stageFlags, u32 size)
{
	RUSH_ASSERT(stageFlags == GfxStageFlags::Vertex ||
	            stageFlags == GfxStageFlags::Compute); // only vertex and compute push constants are implemented

	desc.pushConstants          = size;
	desc.pushConstantStageFlags = stageFlags;

	Item it;
	it.name                     = name;
	it.pushConstants.size       = size;
	it.pushConstants.stageFlags = stageFlags;
	it.type                     = GfxBindingType_PushConstants;
	return items.pushBack(it);
}
