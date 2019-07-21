#pragma once

#include <Rush/GfxCommon.h>
#include <Rush/GfxDevice.h>

struct GpuBuffer
{
	GpuBuffer(u32 elementSize, u32 initialElementCount = 0, GfxBufferFlags flags = GfxBufferFlags::Storage)
	: m_elementSize(elementSize), m_elementCount(0), m_flags(flags), m_format(GfxFormat_Unknown)
	{
		resize(initialElementCount);
	}

	GpuBuffer(GfxFormat format, u32 initialElementCount = 0, GfxBufferFlags flags = GfxBufferFlags::Storage)
	: m_elementSize(getBitsPerPixel(format) / 8), m_elementCount(0), m_flags(flags), m_format(format)
	{
		resize(initialElementCount);
	}

	// Re-allocates GPU buffer, returns new buffer size in bytes
	u32 resize(u32 elementCount)
	{
		if (elementCount == 0)
		{
			m_buffer = InvalidResourceHandle();
		}
		else if (m_elementCount != elementCount)
		{
			GfxBufferDesc bufferDesc;

			bufferDesc.count  = elementCount;
			bufferDesc.stride = m_elementSize;
			bufferDesc.flags  = m_flags;
			bufferDesc.format = m_format;

			m_buffer = Gfx_CreateBuffer(bufferDesc);
		}

		m_elementCount = elementCount;

		return m_elementCount * m_elementSize;
	}

	GfxBuffer get() { return m_buffer.get(); }

	GfxOwn<GfxBuffer> m_buffer;
	u32               m_elementSize;
	u32               m_elementCount;
	GfxBufferFlags    m_flags;
	GfxFormat         m_format;
};

template <typename T> struct GpuBufferT : GpuBuffer
{
	GpuBufferT(u32 initialElementCount = 0, GfxBufferFlags flags = GfxBufferFlags::Storage)
	: GpuBuffer((u32)sizeof(T), initialElementCount, flags)
	{
	}
};

template <GfxFormat format> struct GpuTypedBufferT : GpuBuffer
{
	GpuTypedBufferT(u32 initialElementCount = 0, GfxBufferFlags flags = GfxBufferFlags::Storage)
	: GpuBuffer(format, initialElementCount, flags)
	{
	}
};
