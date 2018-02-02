#pragma once

#include <Rush/GfxCommon.h>
#include <Rush/GfxRef.h>

struct GpuBuffer
{
	GpuBuffer(u32     elementSize,
	    u32           initialElementCount = 0,
	    GfxBufferMode mode                = GfxBufferMode::Static,
	    GfxBufferType type                = GfxBufferType::Storage)
	: m_elementSize(elementSize), m_elementCount(0), m_mode(mode), m_type(type), m_format(GfxFormat_Unknown)
	{
		resize(initialElementCount);
	}

	GpuBuffer(GfxFormat format,
	    u32             initialElementCount = 0,
	    GfxBufferMode   mode                = GfxBufferMode::Static,
	    GfxBufferType   type                = GfxBufferType::Storage)
	: m_elementSize(getBitsPerPixel(format) / 8), m_elementCount(0), m_mode(mode), m_type(type), m_format(format)
	{
		resize(initialElementCount);
	}

	// Re-allocates GPU buffer, returns new buffer size in bytes
	u32 resize(u32 elementCount)
	{
		if (elementCount == 0)
		{
			m_buffer = GfxBufferRef();
		}
		else if (m_elementCount != elementCount)
		{
			GfxBufferDesc bufferDesc;

			bufferDesc.count  = elementCount;
			bufferDesc.stride = m_elementSize;
			bufferDesc.mode   = m_mode;
			bufferDesc.format = m_format;
			bufferDesc.type   = m_type;

			m_buffer.takeover(Gfx_CreateBuffer(bufferDesc));
		}

		m_elementCount = elementCount;

		return m_elementCount * m_elementSize;
	}

	GfxBuffer get() { return m_buffer.get(); }

	GfxBufferRef  m_buffer;
	u32           m_elementSize;
	u32           m_elementCount;
	GfxBufferMode m_mode;
	GfxBufferType m_type;
	GfxFormat     m_format;
};

template <typename T> struct GpuBufferT : GpuBuffer
{
	GpuBufferT(u32    initialElementCount = 0,
	    GfxBufferMode mode                = GfxBufferMode::Static,
	    GfxBufferType type                = GfxBufferType::Storage)
	: GpuBuffer((u32)sizeof(T), initialElementCount, mode, type)
	{
	}
};

template <GfxFormat format> struct GpuTypedBufferT : GpuBuffer
{
	GpuTypedBufferT(u32 initialElementCount = 0,
	    GfxBufferMode   mode                = GfxBufferMode::Static,
	    GfxBufferType   type                = GfxBufferType::Storage)
	: GpuBuffer(format, initialElementCount, mode, type)
	{
	}
};
