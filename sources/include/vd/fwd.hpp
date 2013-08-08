/** VD */
#pragma once

#include <memory>
#include <cstdint>

namespace vd 
{

class Decoder;
class TimeLine;
class FramesView;
class FfmpegDecoder;
class Modifier;
class MediaClip;
class VideoClip;
class VideoMedia;

class IFrame;
class IFrameManager;
class FfmpegFrame;
class SdlAudio;
struct SdlAudioChannel;
class TimeLineTrack;
class Image;
class Compositor;
class IFramePresenter;

class Media;
class MediaObject;
class MediaClip;

class FfmpegDecodingState;
class TimeLineTrack;
class Project;
class SdlRenderer;
class FfmpegDecodingState;

class TimeLineRectWidget;
class TimeLineWidget;
class DecodingState;

class SdlBlitter;
class SdlVideoFrame;
class FfmpegFrame;

class Scene;
class PreviewBackend;

class TimeLineObject;
class TimeLineTrackWidget;
class TimeLineTrack;
class TimeLineSceneWidget;
class TimeLine;

class Image;
class Compositor;
class MovieResource;
class Preview;
class PreviewState;
struct PreviewPreset;

typedef std::string AString;
typedef size_t IFramePresenterId;

typedef int CompositorId;
typedef unsigned char u8;
typedef uint64_t u64;

typedef std::shared_ptr<TimeLineRectWidget> TimeLineRectPtr;

typedef std::shared_ptr<IFrame> IFramePtr;
typedef std::shared_ptr<Media> MediaPtr;
typedef std::shared_ptr<MediaObject> MediaObjectPtr;
typedef std::unique_ptr<MediaClip> MediaClipUPtr;

typedef std::unique_ptr<Project> ProjectUPtr;

typedef std::shared_ptr<IFramePresenter> PresenterPtr;

typedef AString Name;
typedef std::shared_ptr<Image> ImagePtr;
typedef std::shared_ptr<Compositor> CompositorPtr;
typedef std::shared_ptr<MovieResource> MovieResourcePtr;

typedef std::unique_ptr<TimeLine> TimeLinePtr;
typedef std::shared_ptr<TimeLineObject> TimeLineObjectPtr;
typedef std::shared_ptr<TimeLineTrackWidget> TimeLineTrackWidgetPtr;
typedef std::shared_ptr<TimeLineTrack> TimeLineTrackPtr;
typedef std::weak_ptr<TimeLineTrack> TimeLineTrackWPtr;
typedef std::shared_ptr<TimeLineSceneWidget> TimeLineSceneWidgetPtr;

typedef std::shared_ptr<DecodingState> DecodingStatePtr;

typedef std::shared_ptr<Scene> ScenePtr;
typedef std::shared_ptr<Modifier> ModifierPtr;

typedef uint64_t time_mark;

} // namespace vd