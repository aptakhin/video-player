#include <stdio.h>

#ifdef _MSC_VER
#	define BIKE_INLINE __forceinline
#endif

#ifndef BIKE_INLINE
#	define BIKE_INLINE inline
#endif

namespace bike {

template <typename _Arch>
void arch_int(int& v, _Arch& arch)
{
	arch_simple(v, arch);
}

template <typename _Arch>
void arch_uint(unsigned int& v, _Arch& arch)
{
	arch_simple(v, arch);
}

template <typename _Arch>
void arch_float(float& v, _Arch& arch)
{
	arch_simple(v, arch);
}

class ArchiveReader
{
public:
	ArchiveReader(FILE* f)
	:	f_(f),
		buf_sz_(BufSize),
		offset_(BufSize) // This make read from file first
	{
	}

	BIKE_INLINE void read(void* dst, size_t bytes)
	{
		fread(dst, 1, bytes, f_);
		//char* write_to = reinterpret_cast<char*>(dst);
		//
		//while (offset_ + bytes > buf_sz_)
		//{
		//	size_t to_copy = BufSize - offset_;
		//	if (offset_ == 0 || offset_ == BufSize) 
		//	{
		//		if (offset_ == BufSize)
		//			write_to = buf_;

		//		// We have to read more than in internal buffer. No need to copy 
		//		// in internal buffer, just write primarily to destination
		//		buf_sz_ = fread(write_to, 1, BufSize, f_);
		//		if (buf_sz_ == 0)
		//			return;
		//		offset_ = 0;
		//	}
		//	else
		//	{
		//		// Copy least bytes in internal buffer
		//		memcpy(write_to, buf_ + offset_, to_copy);
		//		offset_ = BufSize;
		//	}
		//	
		//	write_to += to_copy;
		//	bytes    -= to_copy;
		//}

		//memcpy(write_to, buf_ + offset_, bytes);
		//offset_ += bytes;
	}

protected:
	FILE* f_;
	static const size_t BufSize = 4096;
	char buf_[BufSize];
	size_t buf_sz_;
	size_t offset_;
};

class SizeCalculator
{
public:
	SizeCalculator()
	:	size_(0) {
	}

	void add(size_t bytes)
	{
		size_ += bytes;
	}

protected:
	size_t size_;
};

template <typename _T>
BIKE_INLINE void arch_simple(_T& v, ArchiveReader& rd)
{
	rd.read(&v, sizeof(_T));
}

template <typename _T>
BIKE_INLINE void arch_simple(_T& v, SizeCalculator& sz)
{
	sz.add(sizeof(_T));
}

template <typename _T, typename _Arch>
BIKE_INLINE void arch_struct(_T& v, _Arch& arch)
{
	archive(v, arch);
}

template <typename _T, typename _Arch>
BIKE_INLINE void arch_tlvv(unsigned int tag, _T& v, _Arch& arch)
{
	arch_uint(tag);

	SizeCalculator sz;
	arch_arch(v, sz);
	unsigned int len = sz.size();
	arch_uint(len);

	int version = -1;
	arch_int(version);

	arch_arch(v, arch);
}

struct Tlvv
{
	unsigned int tag;
	unsigned int len;
	int version;
	std::vector<char> value;
};

class ArchiveWriter
{
public:
	ArchiveWriter(FILE* f)
	:	f_(f)
	{
	}

	BIKE_INLINE void write(void* src, size_t bytes)
	{
		fwrite(src, 1, bytes, f_);
	}

protected:
	FILE* f_;
	//const size_t BufSize = 4096;
	//char buf_[BufSize];
	//size_t offset_;
};

template <typename T>
BIKE_INLINE void arch_simple(T& v, ArchiveWriter& wt)
{
	wt.write(&v, sizeof(T));
}

} // namespace bike {