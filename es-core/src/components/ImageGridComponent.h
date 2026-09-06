#pragma once
#ifndef ES_CORE_COMPONENTS_IMAGE_GRID_COMPONENT_H
#define ES_CORE_COMPONENTS_IMAGE_GRID_COMPONENT_H

#include "Log.h"
#include "animations/LambdaAnimation.h"
#include "components/IList.h"
#include "resources/TextureResource.h"
#include "GridTileComponent.h"

#include <algorithm>

#define EXTRAITEMS 2

enum ScrollDirection
{
	SCROLL_VERTICALLY,
	SCROLL_HORIZONTALLY
};

enum ImageSource
{
	THUMBNAIL,
	IMAGE,
	MARQUEE
};

struct ImageGridData
{
	std::string texturePath;
};

template<typename T>
class ImageGridComponent : public IList<ImageGridData, T>
{
protected:
	using IList<ImageGridData, T>::mEntries;
	using IList<ImageGridData, T>::mScrollTier;
	using IList<ImageGridData, T>::listUpdate;
	using IList<ImageGridData, T>::listInput;
	using IList<ImageGridData, T>::listRenderTitleOverlay;
	using IList<ImageGridData, T>::getTransform;
	using IList<ImageGridData, T>::mSize;
	using IList<ImageGridData, T>::mCursor;
	using IList<ImageGridData, T>::mEntry;
	using IList<ImageGridData, T>::mWindow;

public:
	using IList<ImageGridData, T>::size;
	using IList<ImageGridData, T>::isScrolling;
	using IList<ImageGridData, T>::stopScrolling;

	ImageGridComponent(Window* window);

	void add(const std::string& name, const std::string& imagePath, const T& obj);

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;
	virtual void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;

	void onSizeChanged() override;
	inline void setCursorChangedCallback(const std::function<void(CursorState state)>& func) { mCursorChangedCallback = func; }

	ImageSource	getImageSource() { return mImageSource; };

protected:
	virtual void onCursorChanged(const CursorState& state) override;

private:
	// TILES
	void buildTiles();
	void updateTiles(bool allowAnimation = true, bool updateSelectedState = true);
	void updateTileAtPos(int tilePos, int imgPos, bool allowAnimation, bool updateSelectedState);
	void calcGridDimension();
	bool isScrollLoop();

	bool isVertical() const { return mScrollDirection == SCROLL_VERTICALLY; };

	// Helpers
	int getPrimaryVisibleCount() const;
	int getSecondaryVisibleCount() const;
	int getVisibleCapacity() const;
	bool hasScrollableOverflow() const;
	Vector2f getStaticCenterOffset() const;
	bool shouldCarouselFill() const;
	int wrapEntryIndex(int idx) const;

	// IMAGES & ENTRIES
	bool mEntriesDirty;
	int mLastCursor;
	std::string mDefaultGameTexture;
	std::string mDefaultFolderTexture;

	// TILES
	bool mLastRowPartial;
	Vector2f mAutoLayout;
	float mAutoLayoutZoom;

	Vector4f mPadding;
	Vector2f mMargin;
	Vector2f mTileSize;
	Vector2i mGridDimension;
	std::shared_ptr<ThemeData> mTheme;
	std::vector< std::shared_ptr<GridTileComponent> > mTiles;

	int mStartPosition;

	float mCamera;
	float mCameraDirection;

	// MISCELLANEOUS
	bool mAnimate;
	bool mCenterSelection;
	bool mScrollLoop;
	bool mCarouselMode;
	bool mCarouselFill;
	ScrollDirection mScrollDirection;
	ImageSource mImageSource;
	std::function<void(CursorState state)> mCursorChangedCallback;
};

template<typename T>
ImageGridComponent<T>::ImageGridComponent(Window* window) : IList<ImageGridData, T>(window)
{
	Vector2f screen = Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	mCamera = 0.0f;
	mCameraDirection = 1.0f;

	mAutoLayout = Vector2f::Zero();
	mAutoLayoutZoom = 1.0f;

	mStartPosition = 0;

	mEntriesDirty = true;
	mLastCursor = 0;
	mDefaultGameTexture = ":/cartridge.png";
	mDefaultFolderTexture = ":/folder.svg";

	mSize = screen * 0.80f;
	mMargin = screen * 0.07f;
	mPadding = Vector4f::Zero();
	mTileSize = GridTileComponent::getDefaultTileSize();

	mAnimate = true;
	mCenterSelection = false;
	mScrollLoop = false;
	mCarouselMode = false;
	mCarouselFill = false;
	mScrollDirection = SCROLL_VERTICALLY;
	mImageSource = THUMBNAIL;
}

template<typename T>
void ImageGridComponent<T>::add(const std::string& name, const std::string& imagePath, const T& obj)
{
	typename IList<ImageGridData, T>::Entry entry;
	entry.name = name;
	entry.object = obj;
	entry.data.texturePath = imagePath;

	static_cast<IList< ImageGridData, T >*>(this)->add(entry);
	mEntriesDirty = true;
}

template<typename T>
bool ImageGridComponent<T>::input(InputConfig* config, Input input)
{
	if(input.value != 0)
	{
		int idx = isVertical() ? 0 : 1;

		Vector2i dir = Vector2i::Zero();
		if(config->isMappedLike("up", input))
			dir[1 ^ idx] = -1;
		else if(config->isMappedLike("down", input))
			dir[1 ^ idx] = 1;
		else if(config->isMappedLike("left", input))
			dir[0 ^ idx] = -1;
		else if(config->isMappedLike("right", input))
			dir[0 ^ idx] = 1;

		if(dir != Vector2i::Zero())
		{
			if (isVertical())
				listInput(dir.x() + dir.y() * mGridDimension.x());
			else
				listInput(dir.x() + dir.y() * mGridDimension.y());
			return true;
		}
	}
	else
	{
		if(config->isMappedLike("up", input) || config->isMappedLike("down", input) || config->isMappedLike("left", input) || config->isMappedLike("right", input))
		{
			stopScrolling();
		}
	}

	return GuiComponent::input(config, input);
}

template<typename T>
void ImageGridComponent<T>::update(int deltaTime)
{
	GuiComponent::update(deltaTime);
	listUpdate(deltaTime);

	for(auto it = mTiles.begin(); it != mTiles.end(); it++)
		(*it)->update(deltaTime);
}

template<typename T>
void ImageGridComponent<T>::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = getTransform() * parentTrans;
	Transform4x4f tileTrans = trans;

	float offsetX = isVertical() ? 0.0f : mCamera * mCameraDirection * (mTileSize.x() + mMargin.x());
	float offsetY = isVertical() ? mCamera * mCameraDirection * (mTileSize.y() + mMargin.y()) : 0.0f;

	// Si NO usamos fill y no hay suficientes items para scroll real,
	// centramos visualmente el bloque real.
	Vector2f staticOffset = getStaticCenterOffset();

	tileTrans.translate(Vector3f(offsetX + staticOffset.x(), offsetY + staticOffset.y(), 0.0f));

	if(mEntriesDirty)
	{
		updateTiles();
		mEntriesDirty = false;
	}

	float scaleX = trans.r0().x();
	float scaleY = trans.r1().y();

	Vector2i pos((int)Math::round(trans.translation()[0]), (int)Math::round(trans.translation()[1]));
	Vector2i size((int)Math::round(mSize.x() * scaleX), (int)Math::round(mSize.y() * scaleY));

	Renderer::pushClipRect(pos, size);

	std::shared_ptr<GridTileComponent> selectedTile = NULL;
	for(auto it = mTiles.begin(); it != mTiles.end(); it++)
	{
		std::shared_ptr<GridTileComponent> tile = (*it);

		if(tile->isSelected())
			selectedTile = tile;
		else
			tile->render(tileTrans);
	}

	Renderer::popClipRect();

	if (selectedTile != NULL)
		selectedTile->render(tileTrans);

	listRenderTitleOverlay(trans);

	GuiComponent::renderChildren(trans);
}

template<typename T>
void ImageGridComponent<T>::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	GuiComponent::applyTheme(theme, view, element, properties ^ ThemeFlags::SIZE);

	mTheme = theme;

	Vector2f screen = Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "imagegrid");
	if (elem)
	{
		if (elem->has("margin"))
			mMargin = elem->get<Vector2f>("margin") * screen;

		if (elem->has("padding"))
			mPadding = elem->get<Vector4f>("padding") * Vector4f(screen.x(), screen.y(), screen.x(), screen.y());

		if (elem->has("autoLayout"))
			mAutoLayout = elem->get<Vector2f>("autoLayout");

		if (elem->has("autoLayoutSelectedZoom"))
			mAutoLayoutZoom = elem->get<float>("autoLayoutSelectedZoom");

		if (elem->has("imageSource"))
		{
			auto direction = elem->get<std::string>("imageSource");
			if (direction == "image")
				mImageSource = IMAGE;
			else if (direction == "marquee")
				mImageSource = MARQUEE;
			else
				mImageSource = THUMBNAIL;
		}
		else
			mImageSource = THUMBNAIL;

		if (elem->has("scrollDirection"))
			mScrollDirection = (ScrollDirection)(elem->get<std::string>("scrollDirection") == "horizontal");

		// Modo carrusel: preset cómodo
		if (elem->has("carouselMode"))
			mCarouselMode = elem->get<bool>("carouselMode");
		else
			mCarouselMode = false;

		if (mCarouselMode)
		{
			mCenterSelection = true;
			mScrollLoop = true;
			mCarouselFill = true;
		}

		if (elem->has("centerSelection"))
			mCenterSelection = elem->get<bool>("centerSelection");

		if (elem->has("scrollLoop"))
			mScrollLoop = elem->get<bool>("scrollLoop");

		if (elem->has("carouselFill"))
			mCarouselFill = elem->get<bool>("carouselFill");

		if (elem->has("animate"))
			mAnimate = elem->get<bool>("animate");
		else
			mAnimate = true;

		if (elem->has("gameImage"))
		{
			std::string path = elem->get<std::string>("gameImage");

			if (!ResourceManager::getInstance()->fileExists(path))
			{
				LOG(LogWarning) << "Could not replace default game image, check path: " << path;
			}
			else
			{
				std::string oldDefaultGameTexture = mDefaultGameTexture;
				mDefaultGameTexture = path;

				for (auto it = mEntries.begin(); it != mEntries.end(); it++)
				{
					if ((*it).data.texturePath == oldDefaultGameTexture)
						(*it).data.texturePath = mDefaultGameTexture;
				}
			}
		}

		if (elem->has("folderImage"))
		{
			std::string path = elem->get<std::string>("folderImage");

			if (!ResourceManager::getInstance()->fileExists(path))
			{
				LOG(LogWarning) << "Could not replace default folder image, check path: " << path;
			}
			else
			{
				std::string oldDefaultFolderTexture = mDefaultFolderTexture;
				mDefaultFolderTexture = path;

				for (auto it = mEntries.begin(); it != mEntries.end(); it++)
				{
					if ((*it).data.texturePath == oldDefaultFolderTexture)
						(*it).data.texturePath = mDefaultFolderTexture;
				}
			}
		}
	}

	elem = theme->getElement(view, "default", "gridtile");

	mTileSize = elem && elem->has("size") ?
				elem->get<Vector2f>("size") * screen :
				GridTileComponent::getDefaultTileSize();

	GuiComponent::applyTheme(theme, view, element, ThemeFlags::SIZE);

	if (!elem)
		buildTiles();
}

template<typename T>
void ImageGridComponent<T>::onSizeChanged()
{
	buildTiles();
	updateTiles();
}

template<typename T>
void ImageGridComponent<T>::onCursorChanged(const CursorState& state)
{
	if (mEntries.empty())
		return;

	if (mLastCursor == mCursor)
	{
		if (state == CURSOR_STOPPED && mCursorChangedCallback)
			mCursorChangedCallback(state);

		return;
	}

	bool direction = mCursor >= mLastCursor;
	int diff = direction ? mCursor - mLastCursor : mLastCursor - mCursor;
	if (isScrollLoop() && diff == (int)mEntries.size() - 1)
	{
		direction = !direction;
	}

	int oldStart = mStartPosition;

	int dimScrollable = (isVertical() ? mGridDimension.y() : mGridDimension.x()) - 2 * EXTRAITEMS;
	int dimOpposite = isVertical() ? mGridDimension.x() : mGridDimension.y();

	int centralCol = (int)(dimScrollable - 0.5f) / 2;
	int maxCentralCol = dimScrollable / 2;

	int oldCol = (mLastCursor / dimOpposite);
	int col = (mCursor / dimOpposite);

	int lastCol = (((int)mEntries.size() - 1) / dimOpposite);
	int lastScroll = std::max(0, (lastCol + 1 - dimScrollable));

	float startPos = 0.0f;
	float endPos = 1.0f;

	if (((GuiComponent*)this)->isAnimationPlaying(2))
	{
		startPos = 0.0f;
		((GuiComponent*)this)->cancelAnimation(2);
		updateTiles(false, false);
	}

	if (mAnimate)
	{
		std::shared_ptr<GridTileComponent> oldTile = nullptr;
		std::shared_ptr<GridTileComponent> newTile = nullptr;

		int oldIdx = mLastCursor - mStartPosition + (dimOpposite * EXTRAITEMS);
		if (oldIdx >= 0 && oldIdx < (int)mTiles.size())
			oldTile = mTiles[oldIdx];

		int newIdx = mCursor - mStartPosition + (dimOpposite * EXTRAITEMS);
		if (newIdx >= 0 && newIdx < (int)mTiles.size())
			newTile = mTiles[newIdx];

		for (auto it = mTiles.begin(); it != mTiles.end(); it++)
		{
			if ((*it)->isSelected() && *it != oldTile && *it != newTile)
			{
				startPos = 0.0f;
				(*it)->setSelected(false, false, nullptr);
			}
		}

		Vector3f oldPos = Vector3f::Zero();

		if (oldTile != nullptr && oldTile != newTile)
		{
			oldPos = oldTile->getBackgroundPosition();
			oldTile->setSelected(false, true, nullptr, true);
		}

		if (newTile != nullptr)
			newTile->setSelected(true, true, oldPos == Vector3f::Zero() ? nullptr : &oldPos, true);
	}

	int firstVisibleCol = mStartPosition / dimOpposite;

	if ((col < centralCol || (col == 0 && col == centralCol)) && !mCenterSelection)
		mStartPosition = 0;
	else if ((col - centralCol) > lastScroll && !mCenterSelection && !isScrollLoop())
		mStartPosition = lastScroll * dimOpposite;
	else if ((maxCentralCol != centralCol && col == firstVisibleCol + maxCentralCol) || col == firstVisibleCol + centralCol)
	{
		if (col == firstVisibleCol + maxCentralCol)
			mStartPosition = (col - maxCentralCol) * dimOpposite;
		else
			mStartPosition = (col - centralCol) * dimOpposite;
	}
	else
	{
		if (oldCol == firstVisibleCol + maxCentralCol)
			mStartPosition = (col - maxCentralCol) * dimOpposite;
		else
			mStartPosition = (col - centralCol) * dimOpposite;
	}

	// Si usamos fill carrusel, dejamos siempre el anclaje activo aunque haya pocos.
	if (mCenterSelection && !hasScrollableOverflow() && !shouldCarouselFill())
	{
		mStartPosition = 0;
	}

	if (!isScrollLoop())
	{
		const int minStart = 0;
		const int maxStart = lastScroll * dimOpposite;

		if (mStartPosition < minStart)
			mStartPosition = minStart;

		if (mStartPosition > maxStart)
			mStartPosition = maxStart;
	}

	auto lastCursor = mLastCursor;
	mLastCursor = mCursor;

	mCameraDirection = direction ? -1.0f : 1.0f;
	mCamera = 0.0f;

	if (lastCursor < 0 || !mAnimate)
	{
		updateTiles(mAnimate && (lastCursor >= 0 || isScrollLoop()));

		if (mCursorChangedCallback)
			mCursorChangedCallback(state);

		return;
	}

	if (mCursorChangedCallback)
		mCursorChangedCallback(state);

	bool moveCamera = (oldStart != mStartPosition);

	auto func = [this, startPos, endPos, moveCamera](float t)
	{
		if (!moveCamera)
			return;

		t -= 1.0f;
		float pct = Math::lerp(0.0f, 1.0f, t * t * t + 1.0f);
		t = startPos * (1.0f - pct) + endPos * pct;

		mCamera = t;
	};

	((GuiComponent*)this)->setAnimation(new LambdaAnimation(func, 250), 0, [this] {
		mCamera = 0.0f;
		updateTiles(false);
	}, false, 2);
}

template<typename T>
void ImageGridComponent<T>::buildTiles()
{
	mStartPosition = 0;
	mTiles.clear();

	calcGridDimension();

	// Si hay overflow, dejamos el cursor visual en zona central.
	// Si no hay overflow pero usamos carouselFill, también conviene predesplazar.
	if (mCenterSelection && (hasScrollableOverflow() || shouldCarouselFill()))
	{
		int dimScrollable = (isVertical() ? mGridDimension.y() : mGridDimension.x()) - 2 * EXTRAITEMS;
		mStartPosition -= (int)Math::floorf(dimScrollable / 2.0f);
	}

	Vector2f tileDistance = mTileSize + mMargin;

	if (mAutoLayout.x() != 0 && mAutoLayout.y() != 0)
	{
		auto x = (mSize.x() - (mMargin.x() * (mAutoLayout.x() - 1)) - mPadding.x() - mPadding.z()) / (int)mAutoLayout.x();
		auto y = (mSize.y() - (mMargin.y() * (mAutoLayout.y() - 1)) - mPadding.y() - mPadding.w()) / (int)mAutoLayout.y();

		mTileSize = Vector2f(x, y);
		tileDistance = mTileSize + mMargin;
	}

	bool vert = isVertical();

	Vector2f startPosition = mTileSize / 2.0f;
	startPosition += mPadding.v2();

	int X, Y;

	for (int y = 0; y < (vert ? mGridDimension.y() : mGridDimension.x()); y++)
	{
		for (int x = 0; x < (vert ? mGridDimension.x() : mGridDimension.y()); x++)
		{
			auto tile = std::make_shared<GridTileComponent>(mWindow);

			X = vert ? x : y - EXTRAITEMS;
			Y = vert ? y - EXTRAITEMS : x;

			tile->setPosition(X * tileDistance.x() + startPosition.x(), Y * tileDistance.y() + startPosition.y());
			tile->setOrigin(0.5f, 0.5f);
			tile->setImage("");

			if (mTheme)
				tile->applyTheme(mTheme, "grid", "gridtile", ThemeFlags::ALL);

			if (mAutoLayout.x() != 0 && mAutoLayout.y() != 0)
				tile->forceSize(mTileSize, mAutoLayoutZoom);

			mTiles.push_back(tile);
		}
	}
}

template<typename T>
void ImageGridComponent<T>::updateTiles(bool allowAnimation, bool updateSelectedState)
{
	if (!mTiles.size())
		return;

	if (mScrollTier == 3)
	{
		for (int ti = 0; ti < (int)mTiles.size(); ti++)
		{
			std::shared_ptr<GridTileComponent> tile = mTiles.at(ti);

			tile->setSelected(false);
			tile->setImage(mDefaultGameTexture);
			tile->setVisible(false);
		}
		return;
	}

	std::vector<std::shared_ptr<TextureResource>> previousTextures;
	for (int ti = 0; ti < (int)mTiles.size(); ti++)
	{
		std::shared_ptr<GridTileComponent> tile = mTiles.at(ti);
		previousTextures.push_back(tile->getTexture());
	}

	int firstImg = mStartPosition - EXTRAITEMS * (isVertical() ? mGridDimension.x() : mGridDimension.y());
	for (int ti = 0; ti < (int)mTiles.size(); ti++)
		updateTileAtPos(ti, firstImg + ti, allowAnimation, updateSelectedState);

	if (updateSelectedState)
		mLastCursor = mCursor;

	mLastCursor = mCursor;
}

template<typename T>
void ImageGridComponent<T>::updateTileAtPos(int tilePos, int imgPos, bool allowAnimation, bool updateSelectedState)
{
	std::shared_ptr<GridTileComponent> tile = mTiles.at(tilePos);

	const bool carouselFill = shouldCarouselFill();

	if (carouselFill)
		imgPos = wrapEntryIndex(imgPos);
	else if (isScrollLoop())
		imgPos = wrapEntryIndex(imgPos);

	if (mEntries.empty() || imgPos < 0 || imgPos >= size() || tilePos < 0 || tilePos >= (int)mTiles.size())
	{
		if (updateSelectedState)
			tile->setSelected(false, allowAnimation);

		tile->reset();
		tile->setVisible(false);
	}
	else
	{
		tile->setVisible(true);

		std::string imagePath = mEntries.at(imgPos).data.texturePath;

		if (ResourceManager::getInstance()->fileExists(imagePath))
			tile->setImage(imagePath);
		else if (mEntries.at(imgPos).object->getType() == 2)
			tile->setImage(mDefaultFolderTexture);
		else
			tile->setImage(mDefaultGameTexture);

		if (updateSelectedState)
		{
			if (imgPos == mCursor && mCursor != mLastCursor)
			{
				int dif = mCursor - tilePos;
				int idx = mLastCursor - dif;

				if (idx < 0 || idx >= (int)mTiles.size())
					idx = 0;

				Vector3f pos = mTiles.at(idx)->getBackgroundPosition();
				tile->setSelected(true, allowAnimation, &pos);
			}
			else
			{
				tile->setSelected(imgPos == mCursor, allowAnimation);
			}
		}
	}
}

template<typename T>
void ImageGridComponent<T>::calcGridDimension()
{
	Vector2f gridDimension = (mSize + mMargin) / (mTileSize + mMargin);

	if (mAutoLayout.x() != 0 && mAutoLayout.y() != 0)
		gridDimension = mAutoLayout;

	mLastRowPartial = Math::floorf(gridDimension.y()) != gridDimension.y();

	mGridDimension = Vector2i(gridDimension.x(), Math::ceilf(gridDimension.y()));

	if (mGridDimension.x() < 1)
		LOG(LogError) << "Theme defined grid X dimension below 1";
	if (mGridDimension.y() < 1)
		LOG(LogError) << "Theme defined grid Y dimension below 1";

	if (isVertical())
		mGridDimension.y() += 2 * EXTRAITEMS;
	else
		mGridDimension.x() += 2 * EXTRAITEMS;
}

template<typename T>
int ImageGridComponent<T>::getPrimaryVisibleCount() const
{
	return std::max(0, (isVertical() ? mGridDimension.y() : mGridDimension.x()) - 2 * EXTRAITEMS);
}

template<typename T>
int ImageGridComponent<T>::getSecondaryVisibleCount() const
{
	return isVertical() ? mGridDimension.x() : mGridDimension.y();
}

template<typename T>
int ImageGridComponent<T>::getVisibleCapacity() const
{
	return getPrimaryVisibleCount() * getSecondaryVisibleCount();
}

template<typename T>
bool ImageGridComponent<T>::hasScrollableOverflow() const
{
	if (!mCenterSelection)
		return false;

	return (int)mEntries.size() > getVisibleCapacity();
}

template<typename T>
bool ImageGridComponent<T>::shouldCarouselFill() const
{
	if (!mCarouselFill || !mCenterSelection)
		return false;

	return !mEntries.empty();
}

template<typename T>
int ImageGridComponent<T>::wrapEntryIndex(int idx) const
{
	if (mEntries.empty())
		return -1;

	const int count = (int)mEntries.size();
	int wrapped = idx % count;
	if (wrapped < 0)
		wrapped += count;
	return wrapped;
}

template<typename T>
Vector2f ImageGridComponent<T>::getStaticCenterOffset() const
{
	if (!mCenterSelection)
		return Vector2f::Zero();

	if (mEntries.empty())
		return Vector2f::Zero();

	// En modo carrusel con fill, NO centramos bloque real:
	// dejamos que la banda visual se llene repitiendo elementos.
	if (shouldCarouselFill())
		return Vector2f::Zero();

	if (hasScrollableOverflow())
		return Vector2f::Zero();

	const int visiblePrimary = getPrimaryVisibleCount();
	const int visibleSecondary = getSecondaryVisibleCount();

	if (visiblePrimary <= 0 || visibleSecondary <= 0)
		return Vector2f::Zero();

	int occupiedCols = 0;
	int occupiedRows = 0;
	const int count = (int)mEntries.size();

	if (isVertical())
	{
		occupiedCols = std::min(visibleSecondary, count);
		occupiedRows = (int)Math::ceilf((float)count / (float)visibleSecondary);
	}
	else
	{
		occupiedRows = std::min(visibleSecondary, count);
		occupiedCols = (int)Math::ceilf((float)count / (float)visibleSecondary);
	}

	occupiedCols = std::max(1, occupiedCols);
	occupiedRows = std::max(1, occupiedRows);

	float contentW = occupiedCols * mTileSize.x() + std::max(0, occupiedCols - 1) * mMargin.x();
	float contentH = occupiedRows * mTileSize.y() + std::max(0, occupiedRows - 1) * mMargin.y();

	float availableW = mSize.x() - mPadding.x() - mPadding.z();
	float availableH = mSize.y() - mPadding.y() - mPadding.w();

	float offsetX = std::max(0.0f, (availableW - contentW) * 0.5f);
	float offsetY = std::max(0.0f, (availableH - contentH) * 0.5f);

	return Vector2f(offsetX, offsetY);
}

template<typename T>
bool ImageGridComponent<T>::isScrollLoop()
{
	if (!mScrollLoop)
		return false;

	if (isVertical())
		return (mGridDimension.x() * (mGridDimension.y() - 2 * EXTRAITEMS)) <= (int)mEntries.size();

	return (mGridDimension.y() * (mGridDimension.x() - 2 * EXTRAITEMS)) <= (int)mEntries.size();
}

#endif // ES_CORE_COMPONENTS_IMAGE_GRID_COMPONENT_H
