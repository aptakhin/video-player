#include <vd/timeline.hpp>
#include <vd/sdl.hpp>
#include <vd/ffmpeg.hpp>
#include <QPainter>
#include <QWheelEvent>
#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QLineEdit>
#include <QGraphicsProxyWidget>
#include <QTimer>

namespace vd {

//
// MediaDecoder
//
MediaDecoder* MediaDecoder::instance_ = nullptr;

MediaDecoder::MediaDecoder()
{
	VD_ASSERT2(!instance_, "MediaDecoder must have only instance!");
	instance_ = this;
}

DecodingStatePtr MediaDecoder::decode(MediaPtr media)
{
	return decode(media.get());
}

DecodingStatePtr MediaDecoder::decode(Media* media)
{
	DecodersIter found = decoders_.find(media->filename());
	if (found != decoders_.end())
		return found->second;

	FfmpegDecoder decoder;
	FfmpegDecodingState* state = (FfmpegDecodingState*) decoder.try_decode(media->filename());
	DecodingStatePtr decoder_ptr = std::shared_ptr<DecodingState>(state);
	decoders_.insert(std::make_pair(media->filename(), decoder_ptr));
	return decoder_ptr;
}

//
// PreviewState
//
PreviewState::PreviewState(Project* project)
:	project_(project),
	scene_(nullptr),
	time_base_(1. / 24. * AV_TIME_BASE),
	playing_(0)
{
	sync(0);
}

void PreviewState::sync(time_mark t)
{
	scene_ = project_->time_line()->scene_from_time(t);

	if (scene_)
	{
		playing_ = t;

		printf("T: %lld\n", playing_);

		video_clip_ = peek_video_clip(playing_);
		
		if (video_clip_.get())
			video_clip_->seek(0);

		audio_clip_ = peek_audio_clip(playing_);
		if (audio_clip_.get())
			audio_clip_->seek(0);

		printf("VA: %p %p\n", video_clip_.get(), audio_clip_.get());
	}
}

IFramePtr PreviewState::next_video()
{
	playing_ += time_base_;

	MediaObjectPtr clip = peek_video_clip(playing_);

	if (clip != video_clip_)
	{
		video_clip_ = clip;
		if (video_clip_.get())
			video_clip_->seek(0);
	}

	IFramePtr video_frame;
	if (video_clip_.get())
	{
		clip->set_playing(playing_ - clip->start());
		video_frame = clip->show_next();
	}
	
	return video_frame;
}

MovieResourcePtr PreviewState::next_audio()
{
	MediaObjectPtr clip = peek_audio_clip(playing_);

	if (clip != audio_clip_)
	{
		video_clip_ = clip;
		if (audio_clip_.get())
			audio_clip_->seek(0);
	}

	IFramePtr audio_frame;
	if (audio_clip_.get())
	{
		clip->set_playing(playing_ - clip->start());
		audio_frame = clip->show_next();
	}
	
	return audio_frame;
}

void PreviewState::update_preset(const PreviewPreset& preset)
{
	preset_ = preset;
}

time_mark PreviewState::time_base()
{
	return time_base_;
}

MediaObjectPtr PreviewState::peek_video_clip(time_mark t)
{
	MediaObjectPtr clip = fun::find<MediaObjectPtr>(scene_->tracks_[0]->comps_, MediaObjectPtr(),
		[&] (const MediaObjectPtr& clip)->bool {
			return clip->start() <= t && t <= clip->start() + clip->length();
	});
	return clip;
}

MediaObjectPtr PreviewState::peek_audio_clip(time_mark t)
{
	MediaObjectPtr clip = fun::find<MediaObjectPtr>(scene_->tracks_[1]->comps_, MediaObjectPtr(),
		[&] (const MediaObjectPtr& clip)->bool {
			return clip->start() <= t && t <= clip->start() + clip->length();
	});
	return clip;
}


//
// MediaClip
//
MediaClip::MediaClip(MediaPtr media)
:	media_(media),
	start_(0),
	length_(0)
{
}

//
// MediaObject
//
MediaObject::MediaObject()
:	TimeLineObject(nullptr),
	stream_id_(-1)
{
}

void MediaObject::setup(MediaClip* clip, int stream_id, PresenterPtr presenter)
{
	clip_.reset(clip);
	decoder_   = MediaDecoder::i().decode(clip->media());
	presenter_ = presenter;
	stream_id_ = stream_id;
}

void MediaObject::seek(time_mark t)
{
	frames_.clear(); 
	read_pts_ = ready_pts_ = t;
	decoder_->seek(clip_->start() + t);
	preload_next();
}

void MediaObject::preload_next()
{
	int skipped = 0;
	while (frames_.size() < 30)
	{
		IFramePtr frame = decoder_->peek_frame(stream_id_);

		

		//if (frame->pts() > clip_->start() + length())
		//	break; 

		if (frame->pts() < read_pts_)
		{
			printf("(%lld) ", frame->pts());
			++skipped;
			continue;
		}
		else
			printf("%lld ", frame->pts());

		

		IFramePtr prepared_frame = presenter_->prepare(frame);
		frames_.push_back(prepared_frame);
		//read_pts_ = std::max(prepared_frame->pts(), read_pts_);
	}

	printf("SK: %d\n", skipped);

	if (!frames_.empty())
		ready_pts_ = frames_.front()->pts();
}

IFramePtr MediaObject::show_next()
{
	if (frames_.empty())
		preload_next();

	if (frames_.empty())
		return IFramePtr(nullptr);

	IFramePtr prepared_frame = frames_.front();
	frames_.pop_front();
	return prepared_frame;
}

void MediaObject::set_length(time_mark length)
{
	clip_->set_length(length);
	TimeLineObject::set_length(length);
}

time_mark MediaObject::time_base() const
{
	return decoder_->time_base(stream_id_);
}


//
// TimeLineWidget
//
TimeLineWidget* TimeLineWidget::instance_ = 0;

TimeLineWidget::TimeLineWidget(QWidget* parent)
:	QGraphicsView(parent),
	offset_(0.),
	scale_(1.),
	current_(10.),
	length_(100.),
	cursor_(nullptr),
	time_lbl_(nullptr),
	preview_time_(0),
	playing_(false),
	preview_(nullptr)
{
	instance_ = this;

	setScene(&scene_);
	scene_.setSceneRect(QRectF(0, 0, 4000, 150));
	scene_.setItemIndexMethod(QGraphicsScene::NoIndex);

	setMinimumSize(640, 250);
	
	setRenderHint(QPainter::Antialiasing);

	cursor_ = new TimeLineCursorWidget(this);
	cursor_->setCursor(QCursor(Qt::SizeHorCursor));
	cursor_->setPos(0, 0);
	cursor_->setZValue(1.); 
	cursor_->sync();
	scene_.addItem(cursor_);

	time_lbl_ = new QLabel;
	update_time_label();

	timer_ = new QTimer(this);
	connect(timer_, SIGNAL(timeout()), this, SLOT(update_timer()));
    
	set_playing(false);

	connect(this, SIGNAL(set_playing(bool)), this, SLOT(setup_timer(bool)));
    
	/*QGraphicsProxyWidget* proxy = scene_.addWidget(time_lbl_);
	proxy->setPos(100, 50);
	proxy->setZValue(0.5);*/
}

void TimeLineWidget::setup_timer(bool playing)
{
	playing_ = playing; 
	if (playing_)
 		timer_->start(20);
	else
		timer_->stop();
}

void TimeLineWidget::update_timer()
{
	if (playing_)
	{
		current_ = time2pos(preview_time_);
		cursor_->setPos(QPointF(current_, cursor_->pos().y()));
		ensureVisible(cursor_);
		repaint();
	}
}

void TimeLineWidget::bind(TimeLine* tm)
{
	sync(tm);
}

void TimeLineWidget::update_time_label()
{
	QString time;
	char buffer [80];

	time_t t = (time_t)(preview_time_ / AV_TIME_BASE);
	tm* timeinfo = localtime(&t);

	strftime(buffer, 80, "%H:%M:%S", timeinfo);
	time = buffer;
	time_lbl_->setText(time);
}

void TimeLineWidget::sync(TimeLine* tm)
{
	tm_ = tm;

	scenes_.clear();
	for (TimeLine::ScenesIter i = tm_->scenes_.begin(); i != tm_->scenes_.end(); ++i)
	{
		TimeLineSceneWidgetPtr scene = std::make_shared<TimeLineSceneWidget>(i->get(), this);
		scenes_.push_back(scene);
	}
}

void TimeLineWidget::add(TimeLineRectPtr rect)
{
	scene_.addItem(rect.get());
}

int TimeLineWidget::cd(float c)
{
	return int((c - offset_) * scale_);
}

float TimeLineWidget::dc(int d)
{
	return float(d) / scale_ + offset_;
}

void TimeLineWidget::scroll_moved(int value)
{
	offset_ = value;
	update_ctrls();
}

void TimeLineWidget::update_ctrls()
{
	pose_cursor(current_);
	repaint();
}

qreal TimeLineWidget::time2pos(time_mark t)
{
	time_mark div = t / AV_TIME_BASE;
	time_mark r   = t - div * AV_TIME_BASE;

	qreal r2 = qreal(r) / AV_TIME_BASE;

	return (qreal(div) + r2) * scale_ * 100;
}

time_mark TimeLineWidget::pos2time(qreal pos)
{
	return time_mark(pos / scale_ / 100. * AV_TIME_BASE);
}

void TimeLineWidget::wheelEvent(QWheelEvent* ev)
{
	scale_ += float(ev->delta()) / 120;
	scale_ = std::max(0.5f, scale_);
	scale_ = std::min(1.5f, scale_);
	centerOn(cursor_->pos());
	std::cout << scale_ << std::endl;
};

void TimeLineWidget::mouseDoubleClickEvent(QMouseEvent* ev)
{
	ev->accept();
	cursor_->setPos(QPointF(mapToScene(ev->pos()).x(), 0.));
	cursor_updated();
}

void TimeLineWidget::cursor_updated()
{
	current_ = mapFromScene(QPointF(cursor_->current(), 0)).x();
	time_mark time = pos2time(current_);
	notify_current_preview_time(time);
	preview_->seek(time);
	repaint();
}

void TimeLineWidget::pose_cursor(float current)
{
	current_ = current;
	repaint();
}

QPointF TimeLineWidget::transf(const QPointF& p)
{
	return QPointF(p);
}

QPointF TimeLineWidget::scale(const QPointF& p)
{
	return QPointF(p.x() * scale_, p.y());
}

void TimeLineWidget::notify_current_preview_time(time_mark t)
{
	preview_time_ = t;
	//update_time_label();
}

//
// TimeLineTrackWidget
//

TimeLineTrackWidget::TimeLineTrackWidget(TimeLineTrack* track, TimeLineWidget* parent_widget)
:	track_(track),
	parent_widget_(parent_widget)
{
	sync();
}

void TimeLineTrackWidget::sync()
{
	for (TimeLineTrack::MediaClipsIter i = track_->comps_.begin(); i != track_->comps_.end(); ++i)
		add(*i);
}

QRectF TimeLineTrackWidget::boundingRect(void) const 
{
	return QRectF(0, 0, 2000, 150);
}

void TimeLineTrackWidget::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	for (MediaClipsIter i = comps_.begin(); i != comps_.end(); ++i)
		(*i)->paint(painter, option, widget);
}

void TimeLineTrackWidget::pose_rect(TimeLineRectPtr rect)
{
	qreal x = parent_widget_->time2pos(rect->composed()->start());
	rect->setPos(transf(QPointF(x,  100 * track_->track())));
	QPointF size;
	size.setX(parent_widget_->time2pos(rect->composed()->length()));
	size.setY(100);
	rect->sync(scale(size));
}

void TimeLineTrackWidget::add(MediaObjectPtr composed)
{
	TimeLineRectPtr rect = std::make_shared<TimeLineRectWidget>(composed, this);
	pose_rect(rect);
	comps_.push_back(rect);
	parent_widget_->add(rect);
}

QPointF TimeLineTrackWidget::transf(const QPointF& p)
{
	return parent_widget_->transf(p);
}

QPointF TimeLineTrackWidget::scale(const QPointF& p)
{
	return parent_widget_->scale(p);
}

//
// TimeLineCursorWidget
//

TimeLineCursorWidget::TimeLineCursorWidget(QGraphicsView* parent)
:	parent_(dynamic_cast<TimeLineWidget*>(parent)),
	click_width_div2(4)
{
}

void TimeLineCursorWidget::set_to(int center)
{
}

void TimeLineCursorWidget::sync()
{
	height_ = 200.;
}

QRectF TimeLineCursorWidget::boundingRect() const
{
	return QRectF(-click_width_div2, 0, click_width_div2 * 2, height_);
}

void TimeLineCursorWidget::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
    ev->accept();
    offset_ = ev->pos();
}

void TimeLineCursorWidget::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
    ev->accept();
	qreal x = pos().x() + (ev->pos() - offset_).x();
	setPos(std::max(x, 0.), 0);
	emit parent_->cursor_updated();
}

void TimeLineCursorWidget::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
    ev->accept();
    offset_ = QPoint();
}

void TimeLineCursorWidget::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    QPen pen(Qt::red);
	pen.setWidthF(1.4);
	painter->setPen(pen);
	painter->setBrush(Qt::NoBrush);
	painter->drawLine(0, 0, 0, 100);
}

int TimeLineCursorWidget::current() const
{
	return pos().x();
}


//
// TimeLineRectWidget
//
TimeLineRectWidget::TimeLineRectWidget(MediaObjectPtr comp, QGraphicsItem* parent)
:	QGraphicsItem(parent),
	composed_(comp)
{
}

QRectF TimeLineRectWidget::boundingRect() const
{
	return QRectF(0, 0, size_.x(), size_.y());
}

void TimeLineRectWidget::sync(const QPointF& size)
{
	size_ = size;
}

void TimeLineRectWidget::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    QPen pen(Qt::gray);
    painter->setPen(pen);
	painter->setBrush(Qt::Dense5Pattern);
	painter->drawRect(0, 0, size_.x(), size_.y());
	painter->setRenderHint(QPainter::Antialiasing);
}

void TimeLineRectWidget::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
    ev->accept();
    offset_ = ev->pos();
}

void TimeLineRectWidget::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
    ev->accept();
	qreal x = pos().x() + (ev->pos() - offset_).x();
    setPos(std::max(x, 0.), pos().y());
}

void TimeLineRectWidget::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
    ev->accept();
    offset_ = QPoint();
}

void TimeLineSceneWidget::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	for (TracksIter i = tracks_.begin(); i != tracks_.end(); ++i)
		(*i)->paint(painter, option, widget);
}

TimeLineSceneWidget::TimeLineSceneWidget(Scene* scene, TimeLineWidget* time_line)
:	scene_(scene),
	time_line_(time_line)
{
	sync();
}

void TimeLineSceneWidget::add(TimeLineRectPtr rect)
{
	time_line_->add(rect);
}

void TimeLineSceneWidget::sync()
{
	for (Scene::TracksIter i = scene_->tracks_.begin(); i != scene_->tracks_.end(); ++i)
	{
		TimeLineTrackWidgetPtr track = std::make_shared<TimeLineTrackWidget>(i->get(), time_line_);
		tracks_.push_back(track);
	}
}

QPointF TimeLineSceneWidget::transf(const QPointF& p)
{
	return time_line_->transf(p);
}

QPointF TimeLineSceneWidget::scale(const QPointF& p)
{
	return time_line_->scale(p);
}

//
// Project
//
Project::Project(PresenterPtr video_presenter, PresenterPtr audio_presenter)
:	video_presenter_(video_presenter),
	audio_presenter_(audio_presenter),
	time_line_(new TimeLine) 
{
	time_line_->notify_project(this);
}

Project::~Project() 
{
}

void Project::_create_test()
{
	time_line_->_create_test();
}

//
// TimeLine
//
TimeLine::TimeLine()
:	project_(nullptr)
{
}

void TimeLine::notify_project(Project* project)
{
	project_ = project;
}

void TimeLine::_create_test() 
{
	ScenePtr scene = std::make_shared<Scene>(project_);
	scene->_create_test();
	scenes_.push_back(scene);

	//CompositorPtr show(new SdlShowCompositor(renderer));
	//comps_.push_back(show);
}

template <typename Orig, typename New, typename Cnt>
New* find(const Cnt& cnt, std::function<bool (const Orig& )> find, std::function<New* (const Orig& )> cast)
{
	Cnt::const_iterator i = cnt.begin(), end = cnt.end();
	for (; i != end; ++i)
	{
		if (find(*i))
			return cast(*i);
	}
	return nullptr;
}

template <typename T>
class remove_shared
{
public:
	T* operator() (const std::shared_ptr<T>& ptr)
	{
		return ptr.get();
	}
};

template <typename T>
class remove_weak
{
public:
	T* operator() (const std::weak_ptr<T>& ptr)
	{
		return ptr.get();
	}
};

Scene* TimeLine::scene_from_time(time_mark t)
{
	time_mark offset = 0;
	time_mark add = 0;

	Scene* scene = find<ScenePtr, Scene>(scenes_, [&] (const ScenePtr& scene)->bool { 
		offset += add;
		add = scene->duration();
		return offset <= t && t <= offset + add;
	}, remove_shared<Scene>());

	return scene;
}


//
// Scene
//
Scene::Scene(Project* project)
:	project_(project)
{
}

void Scene::_create_test() 
{
	MediaPtr media = std::make_shared<Media>("test.mp4");

	PresenterPtr video_presenter = project_->video_presenter();
	PresenterPtr audio_presenter = project_->audio_presenter();
	
	MediaClip* media_clip = new MediaClip(media);
	MediaClip* media_clip2 = new MediaClip(media);
	MediaClip* media_clip3 = new MediaClip(media);

	media_clip3->set_start(1000 * AV_TIME_BASE);
	

	MediaObjectPtr composed = std::make_shared<MediaObject>();
	composed->setup(media_clip, 0, video_presenter);
	composed->set_length(2 * AV_TIME_BASE);

	MediaObjectPtr composed2 = std::make_shared<MediaObject>();
	composed2->setup(media_clip2, 1, audio_presenter);
	composed2->set_length(1000 * AV_TIME_BASE);

	MediaObjectPtr composed3 = std::make_shared<MediaObject>();
	composed3->setup(media_clip3, 0, video_presenter);
	composed3->set_start(3 * AV_TIME_BASE);
	composed3->set_length(10 * AV_TIME_BASE);

	
	

	TimeLineTrackPtr track = std::make_shared<TimeLineTrack>(0);
	track->comps_.push_back(composed);
	track->comps_.push_back(composed3);
	//track->comps_.push_back(composed2);
	tracks_.push_back(track);

	TimeLineTrackPtr track2 = std::make_shared<TimeLineTrack>(1);
	//track2->comps_.push_back(composed);
	track2->comps_.push_back(composed2);
	tracks_.push_back(track2);
}

time_mark Scene::duration() const
{
	auto get_duration = [] (MediaObjectPtr comp) { 
		return comp->start() + comp->length(); 
	};

	time_mark longest = fun::best<TimeLineTrackPtr, time_mark>(0, tracks_, std::greater<time_mark>(),
		[&] (TimeLineTrackPtr track) {
			return fun::best<MediaObjectPtr, time_mark>(0, track->comps_, 
				std::greater<time_mark>(), get_duration);
	});

	return longest;
}

//
// TimeLineObject
//
TimeLineObject::TimeLineObject(TimeLineTrack* track)
:	track_(track),
	start_(0),
	length_(0),
	playing_(0)
{
}


}// namespace vd
