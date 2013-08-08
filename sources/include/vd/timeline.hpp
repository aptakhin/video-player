/** VD */
#pragma once

#include <vd/common.hpp>
#include <vd/proto.hpp>
#include <QObject>
#include <QScrollBar>
#include <QGraphicsItem>
#include <QGraphicsView>
#include <QGraphicsScene>

namespace vd {

struct StreamConf
{
	double time_base;
};

class MediaDecoder
{
public:
	MediaDecoder();

	virtual ~MediaDecoder() {}

	DecodingStatePtr decode(MediaPtr media);
	DecodingStatePtr decode(Media* media);

	static MediaDecoder& i() { VD_ASSERT2(instance_, "MediaDecoder singleton wasn't created"); return *instance_; }

protected:
	static MediaDecoder* instance_;

	typedef std::map<AString, DecodingStatePtr> Decoders;
	typedef std::map<AString, DecodingStatePtr>::iterator DecodersIter;

	Decoders decoders_;
};

class PreviewState;

class TimeLineObject
{
public:

	TimeLineObject(TimeLineTrack* track);

	void set_track(TimeLineTrack* track) { track_ = track; }
	TimeLineTrack* track() { return track_; }
	const TimeLineTrack* track() const { return track_; }

	virtual void set_start(time_mark start) { start_ = start; }
	time_mark start() const { return start_; }

	virtual void set_length(time_mark length) { length_ = length; }
	time_mark length() const { return length_; }

	virtual void set_playing(time_mark playing) { playing_ = playing; }
	time_mark playing() const { return playing_; }

	virtual void seek(time_mark t) = 0;

	virtual IFramePtr show_next() = 0;

private:
	TimeLineTrack* track_;

	time_mark start_;
	time_mark length_;
	time_mark playing_;
};

class TimeLineContainer
{
public:

	virtual void add(TimeLineRectPtr rect) = 0;
};

class MediaClip
{
public:
	MediaClip(MediaPtr media);

	void set_start(time_mark start) { start_ = start; }
	time_mark start() const { return start_; }

	void set_length(time_mark length) { length_ = length; }
	time_mark length() const { return length_; }

	MediaPtr media() const { return media_; }

protected:
	
	MediaPtr media_;
	time_mark start_;
	time_mark length_;
};

class MediaPipelineTransformer;

class MediaPipeline
{
public:

};

class FrameFormat;

class MediaPipelineTransformer
{
public:
	virtual IFramePtr transform(IFramePtr frame) = 0;

	virtual bool accepts(const FrameFormat& format) = 0;

protected:

};

class MediaLoader
{
public:
	virtual void seek(time_mark t) = 0;

	virtual IFramePtr load_next() = 0;
};

class MediaBackend
{
protected:
	struct Node
	{
		time_mark start;
		time_mark end;
		std::deque<IFramePtr> frames;
	};

	friend class Iter;

public:

	class Iter
	{
	public:
		IFramePtr show_next();

		time_mark time() const { return t_; }

	protected:
		MediaBackend* backend_;
		Node* cached_;
		time_mark t_;
	};
};

class MediaObject : public TimeLineObject
{
public:
	MediaObject();

	void setup(MediaClip* clip, int stream_id, PresenterPtr presenter);

	void seek(time_mark t) VD_OVERRIDE;

	IFramePtr show_next() VD_OVERRIDE;

	void set_length(time_mark length) VD_OVERRIDE;

	time_mark time_base() const;

	DecodingStatePtr decoder() { return decoder_; }

protected:
	void preload_next();

public:
	MediaClipUPtr clip_;

	int stream_id_;

	/// Pts which corresponds first frame in frames_ queue
	time_mark ready_pts_;
	
	std::deque<IFramePtr> frames_;

	/// Pts which corresponds last decoded frame in frames_ queue
	time_mark read_pts_;
	DecodingStatePtr decoder_;
	PresenterPtr presenter_;
};

struct PreviewPreset
{
	float audio_volume;

	PreviewPreset() : audio_volume(1.f) {}
};

class PreviewState
{
public:

	struct Pres
	{
		double time_base;
	};

	PreviewState(Project* project);

	void sync(time_mark t);

	IFramePtr next_video();

	MovieResourcePtr next_audio();

	time_mark time_base();

	void update_preset(const PreviewPreset& preset);

protected:
	MediaObjectPtr peek_video_clip(time_mark t);

	MediaObjectPtr peek_audio_clip(time_mark t);

protected:
	Project* project_;
	Scene* scene_;
	time_mark time_base_;
	time_mark playing_;
	PreviewPreset preset_;
	MediaObjectPtr video_clip_;
	MediaObjectPtr audio_clip_;

	std::vector<MediaObjectPtr> clips_;
};
	
class TimeLineTrack 
{
public:

	TimeLineTrack(int track) : track_(track) {}

	int track() const { return track_; }

protected:
public:
	int track_;

	typedef std::vector<MediaObjectPtr> MediaClips;
	typedef std::vector<MediaObjectPtr>::iterator MediaClipsIter;
	std::vector<MediaObjectPtr> comps_;
};

class TimeLine
{
public:

	typedef std::vector<ScenePtr> Scenes;
	typedef std::vector<ScenePtr>::iterator ScenesIter;
	

public:

	TimeLine();

	void notify_project(Project* project);

	void _create_test() ;

	void query(time_mark t);

	Scene* scene_from_time(time_mark t);

protected:
public:
	Scenes scenes_;

	Project* project_;

	
};

class TimeLineRectWidget : public QGraphicsItem
{
	friend class TimeLineWidget;

	QRectF boundingRect() const;
public:
	TimeLineRectWidget(MediaObjectPtr composed, QGraphicsItem* parent);

	void sync(const QPointF& size);

	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);

	void mousePressEvent(QGraphicsSceneMouseEvent* ev);
	void mouseMoveEvent(QGraphicsSceneMouseEvent* ev);
	void mouseReleaseEvent(QGraphicsSceneMouseEvent* ev);

	MediaObjectPtr composed() { return composed_; }

protected:
	MediaObjectPtr composed_;
	QPointF offset_;
	QPointF size_;
	TimeLineTrackWidget* parent_track_;
};

class TimeLineTrackWidget : public QGraphicsObject 
{
	friend class TimeLineTrack;

	Q_OBJECT

public:
	typedef std::vector<TimeLineRectPtr> MediaClips;
	typedef std::vector<TimeLineRectPtr>::iterator MediaClipsIter;

public:
	TimeLineTrackWidget(TimeLineTrack* track, TimeLineWidget* parent_widget);

	QRectF boundingRect(void) const;

	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);

	void pose_rect(TimeLineRectPtr rect);

	void add(MediaObjectPtr composed);

	QPointF transf(const QPointF& p);
	QPointF scale(const QPointF& p);

protected:

	void sync();

public:

	TimeLineWidget* parent_widget_;
	TimeLineTrack* track_;
	std::vector<TimeLineRectPtr> comps_;
};

class TimeLineCursorWidget : public QGraphicsItem
{
	friend class TimeLineWidget;

	QRectF boundingRect() const;
public:
	TimeLineCursorWidget(QGraphicsView* parent);
	
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);

	void mousePressEvent(QGraphicsSceneMouseEvent* ev);
	void mouseMoveEvent(QGraphicsSceneMouseEvent* ev);
	void mouseReleaseEvent(QGraphicsSceneMouseEvent* ev);

	void sync();

	int current() const;

	void set_to(int center);

protected:
	TimeLineWidget* parent_;
	QPointF offset_;
	qreal height_;
	int click_width_div2;
};


class TimeLineSceneWidget : public QGraphicsObject, public TimeLineContainer
{
	Q_OBJECT

public:
	
	typedef std::vector<TimeLineTrackWidgetPtr> Tracks;
	typedef std::vector<TimeLineTrackWidgetPtr>::iterator TracksIter;

public:

	TimeLineSceneWidget(Scene* scene, TimeLineWidget* time_line);

	QRectF boundingRect() const { return QRectF(); }

	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);

	QPointF transf(const QPointF& p);
	QPointF scale(const QPointF& p);

	void add(TimeLineRectPtr rect) VD_OVERRIDE;

protected:

	void sync();

public:
	Scene* scene_;
	TimeLineWidget* time_line_;

	std::vector<TimeLineTrackWidgetPtr> tracks_;
};

class TimeLineWidget : public QGraphicsView, public TimeLineContainer
{
	Q_OBJECT

	friend class TimeLine;

public:
	typedef std::vector<TimeLineSceneWidgetPtr> Scenes;
	typedef std::vector<TimeLineSceneWidgetPtr>::iterator ScenesIter;

public:
	TimeLineWidget(QWidget* parent = 0);

	void bind(TimeLine* tm);
	//void bind_ctrls(QScrollBar* scroll);

	void wheelEvent(QWheelEvent* ev);

	void mouseDoubleClickEvent(QMouseEvent * ev);

	void add(TimeLineRectPtr rect) VD_OVERRIDE;

	QPointF transf(const QPointF& p);
	QPointF scale(const QPointF& p);

	qreal time2pos(time_mark t);
	time_mark pos2time(qreal pos);

	static TimeLineWidget& i() { VD_ASSERT2(instance_, "No TimeLineWidget instance!"); return *instance_; }

	void notify_current_preview_time(time_mark t);

	
	void set_preview(Preview* preview) { preview_ = preview; }

signals:
	void set_playing(bool playing);

public slots:
	void cursor_updated();

	

public slots:
	void scroll_moved(int value);

	int cd(float c);
	float dc(int d);

protected slots:
	void update_timer();

	void setup_timer(bool playing);

protected:

	void sync(TimeLine* tm);

	void pose_cursor(float current);

	void update_ctrls();

	void update_time_label();

protected:
	static TimeLineWidget* instance_;
	TimeLine* tm_;
	QGraphicsScene scene_;
	float offset_;
	float scale_;
	float current_;
	float length_;
	QScrollBar* scroll_;
	TimeLineCursorWidget* cursor_;

	Scenes scenes_;
	QLabel* time_lbl_;
	time_mark preview_time_;

	QTimer* timer_;
	bool playing_;
	Preview* preview_;
};

class Scene 
{
public:
	typedef std::vector<TimeLineTrackPtr> Tracks;
	typedef std::vector<TimeLineTrackPtr>::iterator TracksIter;

	typedef std::vector<CompositorPtr> Compositor;
	typedef std::vector<CompositorPtr>::iterator CompositorIter;

	Scene(Project* project);
	
public:
	

	void _create_test();
	
	time_mark duration() const;

protected:
public:
	Project* project_;

	Tracks tracks_;

	Compositor comps_;
};

template <typename _T>
class VarCtrl
{
public:
	virtual ~VarCtrl() = 0;

	virtual _T value(time_mark t) const = 0;
};

class TimeLineParam : public TimeLineObject
{
public:
	TimeLineParam(const Name& name) : TimeLineObject(nullptr), name_(name) {}

	const Name& name() const { return name_; }

protected:
	Name name_;
};

template <typename _T>
class TimeLineVar : public TimeLineParam
{
public:
	TimeLineVar(Name& name) : TimeLineParam(name) {}

	_T value(time_mark t) {
		return value_ctrl_->value(t);
	}

	_T* ctrl() { return value_ctrl_; }
	const _T* ctrl() const { return value_ctrl_; }

	void set_ctrl(_T* ctrl) { value_ctrl_ = ctrl; }

protected:
	Name name_;
	_T* value_ctrl_;
};


class Project 
{
public:

	Project(PresenterPtr video_presenter, PresenterPtr audio_presenter);
	~Project();
		
	void _create_test();

	TimeLine* time_line() { return time_line_.get(); }

	PresenterPtr video_presenter() { return video_presenter_; }
	PresenterPtr audio_presenter() { return audio_presenter_; }

protected:
public:
	AString name_;
	PresenterPtr video_presenter_;
	PresenterPtr audio_presenter_;
	int frame_rate_;
	TimeLinePtr time_line_;
};

}// namespace vd
