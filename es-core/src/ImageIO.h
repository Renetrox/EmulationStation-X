#pragma once
#ifndef ES_CORE_IMAGE_IO
#define ES_CORE_IMAGE_IO

#include "math/Vector2i.h"

#include <stdlib.h>
#include <vector>

class ImageIO
{
public:
	static std::vector<unsigned char> loadFromMemoryRGBA32(
		const unsigned char* data,
		const size_t size,
		size_t& width,
		size_t& height);

	// ES-X: optimized loader. If maxWidth/maxHeight are valid, the image is
	// resized before being returned as RGBA32.
	static std::vector<unsigned char> loadFromMemoryRGBA32Ex(
		const unsigned char* data,
		const size_t size,
		size_t& width,
		size_t& height,
		int maxWidth,
		int maxHeight,
		bool externalZoom,
		Vector2i& baseSize,
		Vector2i& packedSize);

	static Vector2i adjustPictureSize(Vector2i imageSize, Vector2i maxSize, bool externalZoom = false);

	static void flipPixelsVert(unsigned char* imagePx, const size_t& width, const size_t& height);
};

#endif // ES_CORE_IMAGE_IO
