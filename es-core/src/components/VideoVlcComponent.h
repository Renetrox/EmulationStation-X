#pragma once
#ifndef ES_CORE_COMPONENTS_VIDEO_VLC_COMPONENT_H
#define ES_CORE_COMPONENTS_VIDEO_VLC_COMPONENT_H

#include "VideoComponent.h"
#include "renderers/Renderer.h"
#include <vector>

struct SDL_mutex;
struct SDL_Surface;
struct libvlc_instance_t;
struct libvlc_media_t;
struct libvlc_media_player_t;

struct VideoContext {
	SDL_Surface* surface;
	SDL_mutex*   mutex;
	bool         valid;
};

class VideoVlcComponent : public VideoComponent
{
	// Structure that groups together the configuration of the video component
	struct Configuration
	{
		unsigned    startDelay;
		bool        showSnapshotNoVideo;
		bool        showSnapshotDelay;
		std::string defaultVideoPath;
	};

public:
	static void setupVLC(std::string subtitles);

	VideoVlcComponent(Window* window, std::string subtitles);
	virtual ~VideoVlcComponent();

	void render(const Transform4x4f& parentTrans) override;

	// Resize the video to fit this size. If one axis is zero, scale that axis to maintain aspect ratio.
	// If both are non-zero, potentially break the aspect ratio.  If both are zero, no resizing.
	// Can be set before or after a video is loaded.
	// setMaxSize() and setResize() are mutually exclusive.
	void setResize(float width, float height) override;

	// Resize the video to be as large as possible but fit within a box of this size.
	// Can be set before or after a video is loaded.
	// Never breaks the aspect ratio. setMaxSize() and setResize() are mutually exclusive.
	void setMaxSize(float width, float height) override;

private:
	void resize();
	// Only rebuild rounded textured geometry when frame dimensions/radius change.
	void updateRoundedVideoGeometry();
	virtual void startVideo() override;
	virtual void stopVideo() override;
	virtual void handleLooping() override;

	void setMuteMode();
	void setupContext();
	void freeContext();

	// Ducking helper (solo para no repetir)
	void notifyDucking(bool playing);

private:
	static libvlc_instance_t* mVLC;

	libvlc_media_t*        mMedia;
	libvlc_media_player_t* mMediaPlayer;

	VideoContext                    mContext;
	std::shared_ptr<TextureResource> mTexture;

	// TEST: reusable triangle strip for opt-in rounded frames. Zero new textures.
	std::vector<Renderer::Vertex> mRoundedVideoVertices;
	Vector2f mRoundedVideoSize;
	float mRoundedVideoRadius;
};

#endif // ES_CORE_COMPONENTS_VIDEO_VLC_COMPONENT_H
