#include <vd/sdl.hpp>
#include <vd/ffmpeg.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

namespace vd {

SdlVideoFrame::SdlVideoFrame(SDL_Overlay* overlay)
:	IFrame(nullptr),
	overlay_(overlay)
{
}

SdlVideoFrame::~SdlVideoFrame()
{
	SDL_FreeYUVOverlay(overlay_);
}

SDL_Overlay* SdlBlitter::sdl_overlay(SdlVideoFrame* frame)
{
	return frame->overlay_;
}

SdlFfmpegBlit::SdlFfmpegBlit(SwsContext* img_convert_ctx)
:	img_convert_ctx_(img_convert_ctx)
{
}

void SdlFfmpegBlit::blit(SdlVideoFrame* d, void* s)
{
	FfmpegFrame* src = (FfmpegFrame*) s;
	AVPicture pict;

	SDL_Overlay* dst = sdl_overlay(d);
	SDL_LockYUVOverlay(dst);
	pict.data[0] = dst->pixels[0];
	pict.data[1] = dst->pixels[2];
	pict.data[2] = dst->pixels[1];

	pict.linesize[0] = dst->pitches[0];
	pict.linesize[1] = dst->pitches[2];
	pict.linesize[2] = dst->pitches[1];

	int r = sws_scale(img_convert_ctx_, src->frame->data, 
		src->frame->linesize, 0, 
		src->frame->height, 
		pict.data, pict.linesize);
	SDL_UnlockYUVOverlay(dst);
}

SdlRenderer::SdlRenderer(QWidget* parent, Qt::WindowFlags f) 
:	QWidget(parent, f),
	screen_(nullptr),
	overlay_(nullptr)
{
    setAttribute(Qt::WA_PaintOnScreen);
    setUpdatesEnabled(false);

    char variable[64];
    _snprintf(variable, sizeof(variable), "SDL_WINDOWID=0x%lx", winId());
    _putenv(variable);

    SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);

    if((SDL_Init(SDL_INIT_VIDEO) == -1))
    	VD_ERR("Could not initialize SDL: " << SDL_GetError());

    screen_ = SDL_SetVideoMode(640, 480, 24, 0);
    if (screen_ == nullptr)
    	VD_ERR("Couldn't set video mode: " << SDL_GetError());
  
}

SdlRenderer::~SdlRenderer() 
{
    if(SDL_WasInit(SDL_INIT_VIDEO) != 0) 
	{
    	SDL_QuitSubSystem(SDL_INIT_VIDEO);
    	screen_ = nullptr;
    }
}

void SdlRenderer::init_overlay(int width, int height) 
{
	screen_  = SDL_SetVideoMode(width, height, 24, 0);
	overlay_ = SDL_CreateYUVOverlay(width, height,
		SDL_YV12_OVERLAY, screen_);

	setMaximumSize(QSize(width, height));

	black_screen_ = SDL_CreateYUVOverlay(width, height,
		SDL_YV12_OVERLAY, screen_);
	SDL_LockYUVOverlay(black_screen_);
	memset(black_screen_->pixels[0], 250, black_screen_->pitches[0]);
	memset(black_screen_->pixels[1], 231, black_screen_->pitches[1]);
	memset(black_screen_->pixels[2], 250, black_screen_->pitches[2]);
	SDL_UnlockYUVOverlay(black_screen_);

	setMinimumSize(width, height);
}

SdlVideoFrame* SdlRenderer::new_frame()
{
	return new SdlVideoFrame(SDL_CreateYUVOverlay(screen_->w, screen_->h,
		SDL_YV12_OVERLAY, screen_));
}

void SdlRenderer::free_frame(SdlVideoFrame* frame)
{
	delete frame;
}

void SdlRenderer::render_video(MovieResourcePtr video_frame) 
{
	if (SdlVideoFrame* frame = dynamic_cast<SdlVideoFrame*>(video_frame.get()))
	{
		SDL_Rect rect;
		rect.x = 0;
		rect.y = 0;
		rect.w = frame->overlay_->w;
		rect.h = frame->overlay_->h;

		SDL_DisplayYUVOverlay(frame->overlay_, &rect);
	}
	else
	{
		SDL_Rect rect;
		rect.x = 0;
		rect.y = 0;
		rect.w = black_screen_->w;
		rect.h = black_screen_->h;

		SDL_DisplayYUVOverlay(black_screen_, &rect);
	}
}

SdlFfmpegAudioDecoder::SdlFfmpegAudioDecoder(SdlAudio* audio)
:	audio_(audio)
{
}

SdlVideoPresenter::SdlVideoPresenter(SdlRenderer* renderer)
:	renderer_(renderer),
	ffmpeg_blitter_(nullptr)
{
}

IFramePtr SdlVideoPresenter::prepare(IFramePtr frame)
{
	if (FfmpegFrame* ff_frame = dynamic_cast<FfmpegFrame*>(frame.get()))
	{
		if (!ff_frame)
			return IFramePtr();

		SdlVideoFrame* sdl_frame = renderer_->new_frame();
		sdl_frame->set_pts(frame->pts());

		SdlBlitter* blitter = get_blitter(frame);
		blitter->blit(sdl_frame, ff_frame);
		return IFramePtr(sdl_frame);
	}

	return IFramePtr();
}

SdlBlitter* SdlVideoPresenter::get_blitter(IFramePtr frame)
{
	if (FfmpegFrame* ff_frame = dynamic_cast<FfmpegFrame*>(frame.get()))
	{
		if (ffmpeg_blitter_)
			return ffmpeg_blitter_;

		AVCodecContext* video_ctx = ff_frame->codec_ctx;
		SwsContext* conv_ctx = sws_getContext(video_ctx->width, video_ctx->height, 
			video_ctx->pix_fmt, video_ctx->width, video_ctx->height, 
			PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

		ffmpeg_blitter_ = new SdlFfmpegBlit(conv_ctx);
		return ffmpeg_blitter_;
	}

	return nullptr;
}

SdlAudioFrame::SdlAudioFrame()
:	IFrame(nullptr)
{
}

SdlAudioPresenter::SdlAudioPresenter(SdlAudio* audio)
:	audio_(audio),
	audio_decoder_(nullptr)
{
	audio_decoder_ = new SdlFfmpegAudioDecoder(audio_);
}

IFramePtr SdlAudioPresenter::prepare(IFramePtr frame)
{
	if (FfmpegFrame* ff_frame = dynamic_cast<FfmpegFrame*>(frame.get()))
	{
		SdlAudioFrame* sdl_audio_frame = new SdlAudioFrame();
		if (audio_decoder_->decode(sdl_audio_frame, ff_frame))
		{
			//audio_->write(audio_channel_, *sdl_audio_frame);
			//audio_->sigwrite();
			return IFramePtr(sdl_audio_frame);
		}
	}

	return nullptr;
}

static inline
int64_t get_valid_channel_layout(int64_t channel_layout, int channels)
{
    if (channel_layout && av_get_channel_layout_nb_channels(channel_layout) == channels)
        return channel_layout;
    else
        return 0;
}

int audio_diff_avg_count = 0;
int audio_diff_cum = 0;

#define SAMPLE_CORRECTION_PERCENT_MAX 10

#define AV_SYNC_THRESHOLD_MIN 0.01
/* AV sync correction is done if above the maximum AV sync threshold */
#define AV_SYNC_THRESHOLD_MAX 0.1
/* If a frame duration is longer than this, it will not be duplicated to compensate AV sync */
#define AV_SYNC_FRAMEDUP_THRESHOLD 0.1
/* no AV correction is done if too big error */
#define AV_NOSYNC_THRESHOLD 10.0

/* maximum audio speed change to get correct sync */
#define SAMPLE_CORRECTION_PERCENT_MAX 10

/* we use about AUDIO_DIFF_AVG_NB A-V differences to make the average */
#define AUDIO_DIFF_AVG_NB   20

double sdl_audio_diff = 0.;

double audio_diff_avg_coef = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
double audio_diff_threshold;

static int synchronize_audio(const SdlAudioSpec& spec, int nb_samples)
{
    int wanted_nb_samples = nb_samples;

    /* if not master, then we try to remove or add samples to correct the clock */
    if (1) {
        double diff = sdl_audio_diff;

        if (/*!isnan(diff) && */fabs(diff) < AV_NOSYNC_THRESHOLD) {
            audio_diff_cum = diff + audio_diff_avg_coef * audio_diff_cum;
            if (audio_diff_avg_count < AUDIO_DIFF_AVG_NB) {
                /* not enough measures to have a correct estimate */
                audio_diff_avg_count++;
            } else {
                /* estimate the A-V difference */
                double avg_diff = audio_diff_cum * (1.0 - audio_diff_avg_coef);

                if (fabs(avg_diff) >= audio_diff_threshold) {
                    wanted_nb_samples = nb_samples + (int)(diff * spec.freq);
                    int min_nb_samples = ((nb_samples * (100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    int max_nb_samples = ((nb_samples * (100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    wanted_nb_samples = FFMIN(FFMAX(wanted_nb_samples, min_nb_samples), max_nb_samples);
                }
                /*av_dlog(NULL, "diff=%f adiff=%f sample_diff=%d apts=%0.3f %f\n",
                        diff, avg_diff, wanted_nb_samples - nb_samples,
                        audio_clock, audio_diff_threshold);*/
            }
        } else {
            /* too big difference : may be initial PTS errors, so
               reset A-V filter */
            audio_diff_avg_count = 0;
            audio_diff_cum       = 0;
        }
    }

    return wanted_nb_samples;
}

bool SdlFfmpegAudioDecoder::decode(SdlAudioFrame* dst_frame, FfmpegFrame* src_frame)
{
	AVFrame* frame = src_frame->frame;
	const SdlAudioSpec& spec = audio_->spec();

	audio_diff_threshold = 2.0 * 1024 / av_samples_get_buffer_size(NULL, spec.channels, spec.freq, spec.format, 1);

	int64_t dec_channel_layout = get_valid_channel_layout(frame->channel_layout, av_frame_get_channels(frame));

	SwrContext* swr_ctx = swr_alloc_set_opts(NULL,
		dec_channel_layout,                    spec.format,          spec.freq,
		dec_channel_layout, (AVSampleFormat) frame->format, frame->sample_rate,
        0, NULL);

	int err;
	if (!swr_ctx || (err = swr_init(swr_ctx)) < 0)
	{
		VD_ERR("Context wasn't created!");
		return false;
	}

	int64_t wanted_nb_samples = synchronize_audio(spec, frame->nb_samples);
	//int64_t wanted_nb_samples = frame->nb_samples;

	if (swr_ctx)
	{
		const uint8_t** src = (const uint8_t**) src_frame->frame->extended_data;
		uint8_t* dst        = dst_frame->buf;
		int dst_max_samples = wanted_nb_samples * spec.freq / frame->sample_rate + 256;
		int out_size  = av_samples_get_buffer_size(NULL, spec.channels, dst_max_samples, spec.format, 0);

		if (dst_frame->buf_allocated <= out_size)
		{
			VD_ERR("Out of frame size");
			return false;
		}

		if (wanted_nb_samples != frame->nb_samples) 
		{
			if (swr_set_compensation(swr_ctx, (wanted_nb_samples - frame->nb_samples) * spec.freq / frame->sample_rate,
				wanted_nb_samples * spec.freq / frame->sample_rate) < 0)
				VD_ERR("swr_set_compensation() failed\n");
		}

		int conv_sz = swr_convert(swr_ctx, &dst, dst_max_samples, src, frame->nb_samples);
		int resampled_data_size = conv_sz * spec.channels * av_get_bytes_per_sample(spec.format);

        if (conv_sz < 0) {
            VD_ERR("swr_convert() failed");
        }
        if (conv_sz == dst_frame->buf_allocated) {
            VD_ERR("audio buffer is probably too small");
        }

		swr_free(&swr_ctx);

		dst_frame->size = resampled_data_size;

		return true;
	}
	
	return false;
}

void sdl_audio_callback_impl(void* audio_ptr, uint8_t* stream, int len)
{
	SdlAudio* audio = reinterpret_cast<SdlAudio*>(audio_ptr);
	audio->_audio_callback(stream, len);
}

SdlAudio::SdlAudio()
:	write_(false),
	audio_buf_index_(0)
{
}

void SdlAudio::open(const SdlAudioSpec& sp)
{
	spec_ = sp;
	write_ = false;

	spec_.format = AV_SAMPLE_FMT_S16;
	spec_.channel_layout = 0;

	SDL_AudioSpec wanted_spec, spec;
	
	wanted_spec.freq     = sp.freq; //acodec_ctx->sample_rate;
	wanted_spec.format   = AUDIO_S16SYS;
	wanted_spec.channels = sp.channels;
	wanted_spec.silence  = 0;
	wanted_spec.samples  = 1024;
	wanted_spec.callback = sdl_audio_callback_impl;
	wanted_spec.userdata = this;

	if (SDL_OpenAudio(&wanted_spec, &spec) < 0) 
	{
		VD_ERR("SDL_OpenAudio error: " << SDL_GetError());
	}

	if (spec.format != AUDIO_S16SYS) 
	{
        VD_ERR("SDL advised audio format AUDIO_S16SYS is not supported!\n");
    }

	audio_buf_index_ = 0;
}


void SdlAudio::sigwrite()
{
	QMutexLocker lock(&mutex_);
	write_ = true;
}

bool SdlAudio::enough_audio() 
{ 
	QMutexLocker lock(&mutex_);
	return write_; 
}

void SdlAudio::_audio_callback(uint8_t* stream, int len)
{
	QMutexLocker lock(&mutex_);

	//while (!write_ && audio_buf_index_ >= (size_t)len) {}

	if (write_ && audio_buf_index_ >= (size_t)len)
	{
		// Copy to sdl stream
		memcpy(stream, audio_buf_, len);

		// Move least bytes to buffer beginning 
		memmove(audio_buf_, audio_buf_ + len, audio_buf_index_ - len);
		audio_buf_index_ -= len;
		memset(audio_buf_ + audio_buf_index_ + 1, 0, audio_buf_sz_ - audio_buf_index_ - 1);

		write_ = false;
	}
	else
	{
		memset(stream, 0, len);
		//VD_ERR("Few");
	}
}

void SdlAudio::queue_audio(const SdlAudioChannel& channel, MovieResourcePtr ptr)
{
	if (SdlAudioFrame* sdl_audio_frame = dynamic_cast<SdlAudioFrame*>(ptr.get()))
	{
		write(channel, *sdl_audio_frame);
	}
}

bool SdlAudio::write(const SdlAudioChannel& channel, const SdlAudioFrame& frame)
{
	QMutexLocker lock(&mutex_);
	size_t written = frame.size;
	SDL_MixAudio(audio_buf_ + audio_buf_index_, frame.buf, written, channel.volume * SDL_MIX_MAXVOLUME);
	//memcpy(audio_buf_ + audio_buf_index_, frame.buf, written);
	audio_buf_index_ += written;
	write_ = audio_buf_index_ > audio_buf_sz_ / 2;
	return audio_buf_index_ > audio_buf_sz_ / 2;
}

void SdlAudio::render_audio(MovieResourcePtr audio)
{
}

}// namespace vd
