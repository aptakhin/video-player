/** VD */
#pragma once

#include <cstdio>
#include <cstdint>
#include <string>
#include <queue>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <deque>
#include <iomanip>
#include <memory>
#include <QString>

#ifdef Q_OS_WIN32
#
#	include <windows.h>
#
#endif

#ifdef _MSC_VER
#	if _MSC_VER >= 1600 // Younger or equal than MSVS 10
#		include <functional>
#		// MSVS 10 doesn't support a few features of C++11, but we try to avoid them
#		define VD_CPP11
#	endif
#endif//#ifdef _MSC_VER

#ifndef NDEBUG
#	pragma comment(linker, "/SUBSYSTEM:CONSOLE")
#endif

#ifndef VD_CPP11 // For cpp11 compilers
//	Now all compilers are C++11-compatible
#	define VD_CPP11 
#endif

#ifdef VD_CPP11
#	define VD_OVERRIDE override
#else
#	define VD_OVERRIDE
#endif

#ifdef _MSC_VER
#	define VD_INLINE __forceinline
#endif

#ifndef VD_INLINE
#	define VD_INLINE inline
#endif

#define VD_CAT(a, b) a##b
#define VD_ASSERT3(expr, msg, ret) { if (!(expr)) { vd::Logger::i().wh(vd::Logger::M_ERROR); vd::Logger::i().wc() << msg << " "; vd::Logger::i().wp(__FILE__, __LINE__) << std::endl; ret; } }
#define VD_ASSERT2(expr, msg) { if (!(expr)) { vd::Logger::i().wh(vd::Logger::M_ERROR); vd::Logger::i().wc() << msg << " "; vd::Logger::i().wp(__FILE__, __LINE__) << std::endl; } }
#define VD_ERR2(msg, ret) { vd::Logger::i().wh(vd::Logger::M_ERROR); vd::Logger::i().wc() << msg << " "; vd::Logger::i().wp(__FILE__, __LINE__) << std::endl; ret; }
#define VD_ERR(msg) { vd::Logger::i().wh(vd::Logger::M_ERROR); vd::Logger::i().wc() << msg << " "; vd::Logger::i().wp(__FILE__, __LINE__) << std::endl; }
#define VD_LOG(msg) { vd::Logger::i().wh(vd::Logger::M_DEFAULT); vd::Logger::i().wc() << msg << " "; vd::Logger::i().wp(__FILE__, __LINE__) << std::endl; }
#define VD_LOG_SCOPE_IDENT() vd::LogIdent vd_log_scope_inst;

#include "fun.hpp"
#include "fwd.hpp"

namespace vd 
{
	
struct FrameMark 
{
public:
	uint64_t frame;
};

class Logger
{
public:
	Logger();
	~Logger();

	enum Message
	{
		M_DEFAULT,
		M_ERROR,
		M_WARNING
	};

	static Logger& i();

	void write_header(Message msg);
	void wh(Message msg = M_DEFAULT) { write_header(msg); } 
	std::ostream& write_content();
	std::ostream& wc() { return write_content(); }

	std::ostream& write_line();
	std::ostream& wl() { return write_line(); }

	std::ostream& wp(const char* file, int line);

	int inc_indent();
	int dec_indent();

protected:
	static Logger* _instance;

	std::ofstream _log;
	int indent_;
};

class LogIdent
{
public:
	LogIdent() { vd::Logger::i().inc_indent(); }
	~LogIdent() { vd::Logger::i().dec_indent(); }
};

std::ostream& operator << (std::ostream& out, const QString& str);




template <typename T>
class CircularBuffer
{
public:

	CircularBuffer::CircularBuffer(size_t elems)
	:	buf_(new T[bytes]),
		last_(buf_ + bytes),
		start_(buf_),
		end_(buf_)
	{
	}

	void CircularBuffer::write(T* ptr, size_t elems)
	{
		 
	}

	void CircularBuffer::read(T* ptr, size_t elems) const
	{
		 
	}

	bool CircularBuffer::has(size_t elems) const
	{
		if (end_ >= start_)
			return end_ - start_ >= bytes; 
		else
			return (last_ - buf_) - (start_ - end_) >= bytes;
	}

protected:
	
	T* CircularBuffer::index(T* ptr)
	{
		if (ptr > last_)
		{
			T* c = start_ + (ptrdiff_t )(ptr - last_);
			VD_ASSERT2(c < last_, "Bad request");
			return c;
		}
		return ptr;
	}

protected:
	T* buf_;
	T* last_;
	T* start_;
	T* end_;
};

template <typename T>
class CircularBufferLink
{
protected:
	std::shared_ptr<CircularBuffer<T> > buf_;
};


typedef CircularBuffer<u8> AudioBuffer;
typedef std::shared_ptr<AudioBuffer> AudioBufferPtr;



}

