#pragma once

#include <Rush/GfxDevice.h>
#include <Rush/MathTypes.h>
#include <Rush/UtilFile.h>

#include <new>
#include <string>
#include <vector>
#include <xmmintrin.h>

#include <TaskScheduler.h>

template <typename T, size_t SIZE> struct MovingAverage
{
	MovingAverage() { reset(); }

	inline void reset()
	{
		idx = 0;
		sum = 0;
		for (size_t i = 0; i < SIZE; ++i)
		{
			buf[i] = 0;
		}
	}

	inline void add(T v)
	{
		sum += v;
		sum -= buf[idx];
		buf[idx] = v;
		idx      = (idx + 1) % SIZE;
	}

	inline T get() const { return sum / SIZE; }

	size_t idx;
	T      sum;
	T      buf[SIZE];
};

template <typename T> static void writeContainer(DataStream& stream, const std::vector<T>& data)
{
	u32 count = (u32)data.size();
	stream.writeT(count);
	stream.write(data.data(), count * (u32)sizeof(T));
}

template <typename T> static void readContainer(DataStream& stream, std::vector<T>& data)
{
	u32 count = 0;
	stream.readT(count);
	data.clear();
	data.resize(count);
	stream.read(data.data(), count * (u32)sizeof(T));
}

bool        endsWith(const std::string& value, const std::string& suffix);
std::string directoryFromFilename(const std::string& filename);

#define USE_PARALLEL_ALGORITHMS 1

inline enki::TaskScheduler* getTaskScheduler()
{
	static enki::TaskScheduler ts;
	if (!ts.GetNumTaskThreads())
	{
		ts.Initialize();
	}
	return &ts;
}

template <typename I, typename F>
inline void parallelFor(I begin, I end, F fun)
{
#if USE_PARALLEL_ALGORITHMS
	enki::TaskScheduler* ts = getTaskScheduler();
	enki::TaskSet taskSet(end - begin,
		[&](enki::TaskSetPartition range, I threadnum)
	{
		for (I i = range.start; i != range.end; ++i)
		{
			fun(i);
		}
	});
	ts->AddTaskSetToPipe(&taskSet);
	ts->WaitforTask(&taskSet);
#else // USE_PARALLEL_ALGORITHMS
	for (I i = begin; i != end; ++i)
	{
		fun(i);
	}
#endif // USE_PARALLEL_ALGORITHMS
}

template <typename I, typename F>
inline void parallelForEach(I begin, I end, F fun)
{
#if USE_PARALLEL_ALGORITHMS
	enki::TaskScheduler* ts = getTaskScheduler();
	enki::TaskSet taskSet(u32(std::distance(begin, end)),
		[&](enki::TaskSetPartition range, u32 threadnum)
	{
		I rangeBegin = begin;
		std::advance(rangeBegin, range.start);

		I rangeEnd = rangeBegin;
		std::advance(rangeEnd, range.end - range.start);

		for (I i = rangeBegin; i!=rangeEnd; ++i)
		{
			fun(*i);
		}
	});
	ts->AddTaskSetToPipe(&taskSet);
	ts->WaitforTask(&taskSet);
#else // USE_PARALLEL_ALGORITHMS
	for (I i = begin; i != end; ++i)
	{
		fun(*i);
	}
#endif // USE_PARALLEL_ALGORITHMS
}

inline u32 interlockedIncrement(u32& x)
{
#ifdef _MSC_VER
	return (u32)_InterlockedIncrement(reinterpret_cast<volatile long*>(&x));
#else
	return __atomic_add_fetch(reinterpret_cast<volatile long*>(&x), 1, 0);
#endif
}

inline u16 interlockedIncrement(u16& x)
{
#ifdef _MSC_VER
	return (u16)_InterlockedIncrement16(reinterpret_cast<volatile short*>(&x));
#else
	return __atomic_add_fetch(reinterpret_cast<volatile short*>(&x), 1, 0);
#endif
}

template <typename T> struct AlignedArray
{
	AlignedArray(const AlignedArray&) = delete;
	void operator=(const AlignedArray&) = delete;

	AlignedArray() = default;
	AlignedArray(size_t capacity, size_t alignment = 16)
	: m_capacity(capacity), m_data((T*)_mm_malloc(capacity * sizeof(T), alignment))
	{
	}
	~AlignedArray() { _mm_free(m_data); }

	void operator=(AlignedArray&& rhs)
	{
		_mm_free(m_data);
		m_data     = rhs.m_data;
		m_capacity = rhs.m_capacity;
		m_count    = rhs.m_count;

		rhs.m_data     = nullptr;
		rhs.m_capacity = 0;
		rhs.m_count    = 0;
	}

	void resize(size_t newSize, size_t alignment = 16)
	{
		if (m_capacity < newSize)
		{
			_mm_free(m_data);
			m_data     = (T*)_mm_malloc(newSize * sizeof(T), alignment);
			m_capacity = newSize;
		}

		m_count = newSize;
	}

	T&       operator[](size_t index) { return m_data[index]; }
	const T& operator[](size_t index) const { return m_data[index]; }

	T* begin() { return m_data; }
	T* end() { return m_data + m_count; }

	const T* begin() const { return m_data; }
	const T* end() const { return m_data + m_count; }

	void   clear() { m_count = 0; }
	size_t size() const { return m_count; }

	T*       data() { return m_data; }
	const T* data() const { return m_data; }

	T*     m_data     = nullptr;
	size_t m_capacity = 0;
	size_t m_count    = 0;
};

template <typename ArrayType> u32 updateBufferFromArray(GfxContext* rc, GfxBuffer h, const ArrayType& data)
{
	u32 dataSize = (u32)(data.size() * sizeof(data[0]));
	Gfx_UpdateBuffer(rc, h, data.data(), dataSize);
	return dataSize;
}

GfxOwn<GfxTexture> loadDDS(const char* filename);
GfxOwn<GfxTexture> loadBitmap(const char* filename, bool flipY = false);

inline GfxOwn<GfxBuffer> Gfx_CreateConstantBuffer(GfxBufferFlags flags, u32 size, const void* data = nullptr)
{
	GfxBufferDesc desc(GfxBufferFlags::Constant | flags, GfxFormat_Unknown, 1, size);
	return Gfx_CreateBuffer(desc, data);
}

inline GfxTextureData makeTextureData(const void* pixels, u32 mipLevel)
{
	GfxTextureData result;
	result.pixels = pixels;
	result.mip    = mipLevel;
	return result;
}
