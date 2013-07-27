#include <vd/proto.hpp>
#include <vd/ffmpeg.hpp>
#include <vd/sdl.hpp>
#include <QMutex>
#include <QWaitCondition>
#include <QPainter>
#include <QWheelEvent>


extern "C" {
#include <libavutil/time.h>
}

namespace vd {

Id8::Id8(const char* name)
:	num_(0)
{
	size_t sz = strlen(name);
	if (sz == 0 || sz > 8)
		throw std::runtime_error("Name in id8 can't be longer 8 symbols.");
	memcpy(name_, name, sz);
}

Id8::Id8(const Id8& cpy)
:	num_(cpy.num_)
{
}

Id8& Id8::operator =(const Id8& cpy)
{
	num_ = cpy.num_;
	return *this;
}

AString Id8::to_str()
{
	size_t last = 7;
	while (last >= 0 && name_[last] == 0)
		--last;
	return AString(name_, last + 1); // Can't be zero length
}

bool operator == (const Id8& a, const Id8& b)
{
	return a.num_ == b.num_;
}

bool operator < (const Id8& a, const Id8& b)
{
	return a.num_ < b.num_;
}


Preview::Preview(SdlRenderer* renderer)
:	renderer_(renderer),
	audio_(nullptr),
	stopped_(false),
	backend_(nullptr),
	project_(nullptr),
	pause_(true),
	playing_(0),
	prev_frame_(0)
{
	audio_ = new SdlAudio();
	preset_ = new PreviewPreset;
	audio_channel_ = new SdlAudioChannel();
	audio_channel_->volume = 1.;
}

void Preview::_play_video(const AString& filename) 
{
}

void PlaySound(char *file);

void Preview::_start_play()
{
	stopped_ = false;

	time_mark time_base = backend_->time_base();
	
	printf("q %lld\n", time_base);

	SDL_PauseAudio(0); // switch on audio

	time_mark pres_time = 0;

	while (!stopped_)
	{
		QCoreApplication::processEvents();

		if (pause_)
		{
			if (prev_frame_ != 0)
				TimeLineWidget::i().set_playing(false);
			prev_frame_ = 0;
			continue;
		}

		if (prev_frame_ == 0)
		{
			prev_frame_ = (time_mark) av_gettime();
			TimeLineWidget::i().set_playing(true);
		}

		MovieResourcePtr video_frame = backend_->next_video();

		if (video_frame)
			pres_time = video_frame->pts();
		else
			pres_time += backend_->time_base();

		backend_->update_preset(*preset_);

		do
		{
			while (!audio_->enough_audio())
			{
				MovieResourcePtr audio_data = backend_->next_audio();
				audio_->queue_audio(*audio_channel_, audio_data);
			}

			time_mark curtime = av_gettime();
			time_mark dt = curtime - prev_frame_;
			playing_ += dt;
			prev_frame_ = curtime;
			TimeLineWidget::i().notify_current_preview_time(playing_);
		}
		while (playing_ < pres_time);
		//printf("%lld %lld\n", playing_, pres_time);
		
		renderer_->render_video(video_frame);

		//mutex_.unlock();
	}
}

void Preview::continue_play()
{
	pause_ = false;
}

void Preview::pause_play()
{
	pause_ = true;
}

void Preview::stop()
{
	stopped_ = true;
}

void Preview::seek(time_mark t)
{
	playing_ = t;
	backend_->sync(t);
	MovieResourcePtr video_frame = backend_->next_video();
	if (video_frame)
		printf("Found: %lld %lld\n", t, video_frame->pts());
	renderer_->render_video(video_frame);
}

#define NUM_SOUNDS 2
struct sample {
    Uint8 *data;
    Uint32 dpos;
    Uint32 dlen;
} sounds[NUM_SOUNDS];

void mixaudio(void *unused, Uint8 *stream, int len)
{
    Uint32 i;
    Uint32 amount;

    for ( i=0; i<NUM_SOUNDS; ++i ) {
        amount = (sounds[i].dlen-sounds[i].dpos);
        if ( amount > (Uint32)len ) {
            amount = len;
        }
        SDL_MixAudio(stream, &sounds[i].data[sounds[i].dpos], amount, SDL_MIX_MAXVOLUME);
        sounds[i].dpos += amount;
    }
}

void PlaySound(char *file)
{
    int index;
    SDL_AudioSpec wave;
    Uint8 *data;
    Uint32 dlen;
    SDL_AudioCVT cvt;

    /* Look for an empty (or finished) sound slot */
    for ( index=0; index<NUM_SOUNDS; ++index ) {
        if ( sounds[index].dpos == sounds[index].dlen ) {
            break;
        }
    }
    if ( index == NUM_SOUNDS )
        return;

    /* Load the sound file and convert it to 16-bit stereo at 22kHz */
    if ( SDL_LoadWAV(file, &wave, &data, &dlen) == NULL ) {
        fprintf(stderr, "Couldn't load %s: %s\n", file, SDL_GetError());
        return;
    }
    SDL_BuildAudioCVT(&cvt, wave.format, wave.channels, wave.freq,
                            AUDIO_S16,   2,             22050);
    cvt.buf = (Uint8*) malloc(dlen*cvt.len_mult);
    memcpy(cvt.buf, data, dlen);
    cvt.len = dlen;
    SDL_ConvertAudio(&cvt);
    SDL_FreeWAV(data);

    /* Put the sound data in the slot (it starts playing immediately) */
    if ( sounds[index].data ) {
        free(sounds[index].data);
    }
    SDL_LockAudio();
    sounds[index].data = cvt.buf;
    sounds[index].dlen = cvt.len_cvt;
    sounds[index].dpos = 0;
    SDL_UnlockAudio();
}

struct AudioParams 
{
    int freq;
    int channels;
    int64_t channel_layout;
    enum AVSampleFormat fmt;
};

void Preview::bind(Project* project)
{
	project_ = project;
	MediaObjectPtr com = project_->time_line_->scenes_[0]->tracks_[0]->comps_[0];

	FrameMark frame;
	frame.frame = 0;
	//backend_->fetch_video(renderer_, com, frame);

	FfmpegDecodingState* state = dynamic_cast<FfmpegDecodingState*>(com->decoder_.get());
	renderer_->init_overlay(state->width(), state->height());

	AVCodecContext* audio_codec_ctx = state->streams_[1].codec_ctx;

	SdlAudioSpec spec;
	spec.freq     = audio_codec_ctx->sample_rate;
	spec.channels = audio_codec_ctx->channels;
	audio_->open(spec);

	backend_ = new PreviewState(project_);
}

void Preview::set_audio_volume(float audio_volume)
{
	audio_channel_->volume = audio_volume;
}

Media::Media(const AString& filename)
:	filename_(filename)
{
}

DecodingStatePtr Media::decoder()
{
	if (decoder_.get() == nullptr)
		decoder_ = MediaDecoder::i().decode(this);

	return decoder_;
}

}