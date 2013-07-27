/** VD */
#pragma once

#include <vd/common.hpp>
#include <vd/timeline.hpp>
#include <QObject>
#include <QScrollBar>
#include <SDL/SDL.h>

struct AVFrame;

namespace vd {

class IFramePresenter
{
public:
	virtual ~IFramePresenter() {}

	virtual IFramePtr prepare(IFramePtr frame) = 0;

protected:
};

class IFrameManager
{
public:
	virtual ~IFrameManager() {}

	virtual IFramePresenter* presenter() = 0;

protected:
};


class MovieResource
{
public:
	MovieResource() : pts_(0) {}

	virtual ~MovieResource() {}

	time_mark pts() const { return pts_; }
	void set_pts(time_mark pts) { pts_ = pts; }

protected:
	time_mark pts_;
};

/// Can't be zero length
class Id8
{
public:
	union
	{
		char name_[8];
		u64 num_;
	};

	Id8(const char* name);

	Id8(const Id8& cpy);

	Id8& operator =(const Id8& cpy);

	AString to_str();

	friend bool operator == (const Id8& a, const Id8& b);

	friend bool operator < (const Id8& a, const Id8& b);
};

bool operator == (const Id8& a, const Id8& b);
bool operator < (const Id8& a, const Id8& b);

class FrameFormat
{
public:
	FrameFormat(const Id8 id);
	
protected:
	const Id8 id_;
};

class IFrame : public MovieResource
{
public:
	IFrame(IFrameManager* mgr) : mgr_(mgr) {}

	virtual ~IFrame() {}

protected:
	IFrameManager* mgr_;
};

class Media 
{
public:
	Media(const AString& filename);

	const AString& filename() const { return filename_; }

	DecodingStatePtr decoder();

protected:

	AString filename_;

	DecodingStatePtr decoder_;
};


class DecodingState 
{
public:
	virtual IFramePtr peek_frame(size_t stream_id) = 0;

	virtual void seek(time_mark t) = 0;

	virtual time_mark length() const = 0;

	virtual time_mark time_base(size_t stream_id) const = 0;

	virtual int width() const = 0;
	virtual int height() const = 0;

protected:
};

class Decoder
{
public:
	virtual DecodingState* try_decode(const AString& filename) = 0;	
};

class Preview : public QObject
{
	Q_OBJECT
public:

	Preview(SdlRenderer* renderer);

	void bind(Project* project);

	void _play_video(const AString& filename);

	void _audio_callback(Uint8* stream, int len);

	void set_audio_volume(float volume);

	bool paused() const { return pause_; }

	void seek(time_mark t);

	SdlAudio* audio() { return audio_; }
	
protected:

public slots:

	void _start_play();

	void stop();

	void continue_play();
	void pause_play();

protected:
	SdlRenderer* renderer_;
	SdlAudio* audio_;
	SdlAudioChannel* audio_channel_;
	PreviewPreset* preset_;
	bool stopped_;
	PreviewState* backend_;
	Project* project_;
	bool pause_;
	time_mark playing_;
	time_mark prev_frame_;

	//QMutex mutex_;
	//QWaitCondition wait_;
};

}// namespace vd
