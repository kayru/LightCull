#pragma once

#include <Rush/GfxDevice.h>
#include <Rush/GfxRef.h>
#include <Rush/MathTypes.h>
#include <Rush/UtilFile.h>

#include <new>
#include <string>
#include <vector>

#ifdef _MSC_VER
#include <ppl.h>
#else
#include <tbb/compat/ppl.h>
#endif

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

#if USE_PARALLEL_ALGORITHMS

template <typename T, typename F> inline void parallelForEach(T begin, T end, F f)
{
	Concurrency::parallel_for_each(begin, end, f);
}

#else

template <typename T, typename F> inline void parallelForEach(T begin, T end, F f)
{
	for (auto it = begin; it != end; ++it)
	{
		f(*it);
	}
}
#endif

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

template <typename T> u32 Gfx_UpdateBufferT(GfxContext* rc, GfxBuffer h, const AlignedArray<T>& data)
{
	u32 dataSize = (u32)(data.size() * sizeof(data[0]));
	Gfx_UpdateBuffer(rc, h, data.data(), dataSize);
	return dataSize;
}

GfxTextureRef loadDDS(const char* filename);
GfxTextureRef loadBitmap(const char* filename, bool flipY = false);
