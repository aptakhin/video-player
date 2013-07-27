/** VD */
#include <vd/ffmpeg.hpp>

namespace vd {

void log_callback(void *ptr, int level, const char *fmt, va_list vargs);

void FfmpegPlugin::install() {
	av_register_all();
	// And somewhere after ffmpeg initialization
	av_log_set_callback(log_callback);
	av_log_set_level(AV_LOG_WARNING);
}

#define IF_FF_FAIL(op) ((err = op) < 0)


#define VDFF4(op, msg, res, ret)\
	if (((res = op) < 0)) {\
		VD_LOG();\
	}

void log_callback(void *ptr, int level, const char *fmt, va_list vargs)
{
	static char message[8192];
	const char *module = NULL;

	// Get module name
	if (ptr)
	{
		AVClass *avc = (AVClass*) ptr;
		//module = avc->item_name(ptr);
    }

	// Create the actual message
	vsnprintf(message, sizeof(message), fmt, vargs);

	// Append the message to the logfile
	if (module)
	{
		std::cout << module << " -------------------" << std::endl;
	}
	std::cout << "lvl: " << level << std::endl << "msg: " << message << std::endl;
}

FfmpegFrame::FfmpegFrame() 
:	IFrame(nullptr), 
	frame(nullptr) 
{
}

FfmpegFrame::~FfmpegFrame() 
{
	/*if (frame)
	{
		av_buffer_unref(frame->buf);
		av_frame_free(&frame);
	}
	frame = nullptr;*/
}

bool FfmpegDecoder::fill_stream_data(FfmpegStream* data, const AVFormatContext* format_ctx, int stream_id)
{
	data->stream_id = stream_id;

	VD_LOG_SCOPE_IDENT();
	if (format_ctx->streams[stream_id]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		data->type = FfmpegStream::T_VIDEO;
	if (format_ctx->streams[stream_id]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
		data->type = FfmpegStream::T_AUDIO;

	data->codec_ctx = format_ctx->streams[stream_id]->codec;

	data->codec = avcodec_find_decoder(data->codec_ctx->codec_id);
	//VD_LOG("Finding codec");
	if (!data->codec) {
		VD_ERR("Codec wasn't found");
		return false;
	}

	//VD_LOG("Opening codec");
	
	if (avcodec_open2(data->codec_ctx, data->codec, NULL) < 0)
	{
		VD_ERR("Codec wasn't opened");
		return false;
	}

	return true;
}

DecodingState* FfmpegDecoder::try_decode(const AString& filename) 
{
	AVFormatContext* format_ctx = NULL;
	int err;

	AVInputFormat* fmt = av_find_input_format("mp4");
	VD_LOG("FFmpegDecoder trying to open " << filename);
	err = avformat_open_input(&format_ctx, filename.c_str(), fmt, NULL);

	VD_LOG_SCOPE_IDENT();
	if (err < 0) 
	{
		VD_LOG("FFmpegDecoder can't open this format");
		return nullptr;
    }

	if ((err = av_find_stream_info(format_ctx)) < 0)
	{
		VD_LOG("FFmpegDecoder can't find stream info");
		return nullptr;
	}

	// Find the first video stream
	std::vector<FfmpegStream> streams;
	FfmpegStream stream;
	int video_streams = 0;
	int audio_streams = 0;
	int ok = 1;
	VD_LOG(format_ctx->nb_streams << " streams found");
	for (unsigned int i = 0; i < format_ctx->nb_streams; i++)
	{
		VD_LOG_SCOPE_IDENT();
		if (format_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) 
		{
			ok = fill_stream_data(&stream, format_ctx, i);
			streams.push_back(stream);
			++video_streams;
		}
	}
	for (unsigned int i = 0; i < format_ctx->nb_streams; i++)
	{
		VD_LOG_SCOPE_IDENT();
		if (format_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			ok = fill_stream_data(&stream, format_ctx, i);
			streams.push_back(stream);
			++audio_streams;
		}
	}

	FfmpegDecodingState* state = new FfmpegDecodingState;
	state->format_ctx_ = format_ctx;
	state->streams_    = streams;

	VD_ASSERT2(video_streams == 1, "Expected only one video stream");
	VD_ASSERT2(audio_streams <= 1, "Expected only one audio stream");

	return state;
}

FfmpegDecodingState::FfmpegDecodingState()
:	format_ctx_(nullptr),
	conv_ctx_(nullptr),
	offset_(0)
{
}

time_mark FfmpegDecodingState::time_base(size_t stream_id) const
{
	return time_mark(av_q2d(streams_[stream_id].codec_ctx->time_base) * AV_TIME_BASE);
}

bool FfmpegDecodingState::read()
{
	int end = 1;
	int packets_read = 0;

	AVPacket packet;
	av_init_packet(&packet);

	while (packets_read < 10 && (end = av_read_frame(format_ctx_, &packet)) >= 0) 
	{
		for (size_t i = 0; i < streams_.size(); ++i)
		{
			FfmpegStream& stream = streams_[i];
			if (packet.stream_index == stream.stream_id) 
			{
				AVPacket copy;
				av_copy_packet(&copy, &packet);
				stream.pool.push_back(copy);
				++packets_read;
			}
		}
		av_free_packet(&packet);
	}

	return end >= 0;
}

void FfmpegDecodingState::seek(time_mark seek_target)
{
	AVRational q = {1, AV_TIME_BASE};
	seek_target = av_rescale_q((uint64_t)seek_target, q, format_ctx_->streams[0]->time_base);
	if (av_seek_frame(format_ctx_, 0, (uint64_t)seek_target, AVSEEK_FLAG_FRAME) < 0)
	{
		VD_ERR("Error in seeking");
	}

	flush_pools();

	offset_ = 0;
	prev_frame_ = nullptr;
}

void FfmpegDecodingState::flush_pools()
{
	for (size_t i = 0; i < streams_.size(); ++i)
	{
		FfmpegStream& stream = streams_[i];

		for (size_t j = 0; j < stream.pool.size(); ++j)
			av_free_packet(&stream.pool[j]);

		streams_[i].pool.clear();
	}
}

time_mark FfmpegDecodingState::length() const
{
	return 0;
}

int FfmpegDecodingState::width() const
{
	return streams_[0].codec_ctx->width;
}

int FfmpegDecodingState::height() const
{
	return streams_[0].codec_ctx->height;
}

IFramePtr FfmpegDecodingState::peek_frame(size_t stream_id)
{
	int frame_finished = 0;
	int result = 1;
	int have_smth = 1;

	AVFrame* frame = avcodec_alloc_frame();
	FfmpegStream& stream = streams_[stream_id];
	size_t data_sz = 0;

	while (have_smth)
	{
		while (have_smth && stream.pool.empty())
			have_smth = read();
		if (stream.pool.empty()) // No more frames. Stream finished.
			break;

		AVPacket& packet = stream.pool.front();

		if (stream.type == FfmpegStream::T_VIDEO) 
		{
			avcodec_decode_video2(stream.codec_ctx, frame, &frame_finished, &packet);
		}

		if (stream.type == FfmpegStream::T_AUDIO) 
		{
			int sz = packet.size;
			int read = avcodec_decode_audio4(stream.codec_ctx, frame, &frame_finished, &packet);
			data_sz += read;

			AVRational tb = {1, frame->sample_rate};
			if (frame->pts != AV_NOPTS_VALUE)
				frame->pts = av_rescale_q(frame->pts, stream.codec_ctx->time_base, tb);
			if (frame->pts == AV_NOPTS_VALUE && packet.pts != AV_NOPTS_VALUE)
				frame->pts = av_rescale_q(packet.pts, stream.codec_ctx->time_base, tb);
			if (packet.pts != AV_NOPTS_VALUE)
				packet.pts += (double) frame->nb_samples / frame->sample_rate / av_q2d(stream.codec_ctx->time_base);

			if (read != sz)
			{
				VD_ERR("Unhandled multuply audio data in codec! We were warned:(.");
			}
			//memcpy(stream, ff_frame->frame->data[0], data_size);

			if (stream.codec->capabilities & CODEC_CAP_DELAY)
			{
				VD_ERR("CODEC_CAP_DELAY.");
			}
		}

		av_free_packet(&packet);
		stream.pool.pop_front();

		if (frame_finished)
			break;
	}

	if (!frame_finished)
		return IFramePtr(nullptr);

	time_mark base = time_mark(av_q2d(stream.codec_ctx->time_base) * AV_TIME_BASE);
	time_mark pts  = frame->pts * base - offset_;

	if (prev_frame_ && stream_id == 0)
	{
		if (abs(int64_t(prev_frame_->pts() - pts)) > 2 * int64_t(base))
		{
			//offset_ = pts - prev_frame_->pts() - base;
			//pts -= offset_;
			int p = 0;
		}
	}

	FfmpegFrame* ff_frame = new FfmpegFrame;
	ff_frame->frame     = frame;
	ff_frame->codec_ctx = stream.codec_ctx;
	ff_frame->data_sz   = data_sz;
	ff_frame->set_pts(pts);
	prev_frame_ = IFramePtr(ff_frame);
	return prev_frame_;
}

}// namespace vd
