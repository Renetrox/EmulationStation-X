#pragma once
#ifndef ES_CORE_RESOURCES_MAX_SIZE_INFO_H
#define ES_CORE_RESOURCES_MAX_SIZE_INFO_H

#include "math/Vector2f.h"

// ES-X: optional texture load limit.
// This is used to decode large images at a smaller size before uploading them to VRAM.
// externalZoom=false -> contain behavior.
// externalZoom=true  -> cover behavior, useful for setMinSize()/cropped images.
class MaxSizeInfo
{
public:
	MaxSizeInfo() :
		mSize(0.0f, 0.0f),
		mExternalZoom(false),
		mExternalZoomKnown(false)
	{
	}

	MaxSizeInfo(float width, float height) :
		mSize(width, height),
		mExternalZoom(false),
		mExternalZoomKnown(false)
	{
	}

	MaxSizeInfo(const Vector2f& size) :
		mSize(size),
		mExternalZoom(false),
		mExternalZoomKnown(false)
	{
	}

	MaxSizeInfo(float width, float height, bool externalZoom) :
		mSize(width, height),
		mExternalZoom(externalZoom),
		mExternalZoomKnown(true)
	{
	}

	MaxSizeInfo(const Vector2f& size, bool externalZoom) :
		mSize(size),
		mExternalZoom(externalZoom),
		mExternalZoomKnown(true)
	{
	}

	bool empty() const
	{
		return mSize.x() <= 1.0f || mSize.y() <= 1.0f;
	}

	float x() const { return mSize.x(); }
	float y() const { return mSize.y(); }

	bool externalZoom() const { return mExternalZoom; }
	bool isExternalZoomKnown() const { return mExternalZoomKnown; }

private:
	Vector2f mSize;
	bool mExternalZoom;
	bool mExternalZoomKnown;
};

#endif // ES_CORE_RESOURCES_MAX_SIZE_INFO_H
