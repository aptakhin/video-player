/** VD */
#pragma once

#include "common.hpp"
#include "sdl.hpp"
#include "proto.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

namespace vd {

class FfmpegPlugin 
{
public:
	void install();

	void uninstall();

protected:
};

struct FfmpegStream
{
	enum Type
	{
		T_VIDEO,
		T_AUDIO
	};

	AVCodecContext* codec_ctx;
	AVCodec* codec;
	Type type;
	int stream_id;
	std::deque<AVPacket> pool;
};

class FfmpegFrame : public IFrame 
{
public:
	FfmpegFrame();

	~FfmpegFrame();

protected:
public:
	AVFrame* frame;
	AVCodecContext* codec_ctx;
	size_t data_sz;
};

class FfmpegDecodingState : public DecodingState 
{
public:
	friend class FfmpegDecoder;

	FfmpegDecodingState();

	IFramePtr peek_frame(size_t stream_id) VD_OVERRIDE;

	void seek(time_mark t) VD_OVERRIDE;

	time_mark length() const VD_OVERRIDE;

	time_mark time_base(size_t stream_id) const VD_OVERRIDE;

	int width() const VD_OVERRIDE;
	int height() const VD_OVERRIDE;

protected:

	bool read();

	void flush_pools();

public:

	AVFormatContext* format_ctx_;
	std::vector<FfmpegStream> streams_;
	SwsContext* conv_ctx_;
	IFramePtr prev_frame_;
	time_mark offset_;
};

class FfmpegDecoder : public Decoder 
{
public:
	DecodingState* try_decode(const AString& filename);

protected:

	bool fill_stream_data(FfmpegStream* data, const AVFormatContext* format_ctx, int stream_id);
};

}// namespace vd
