#pragma once
#ifndef ES_CORE_COMPONENTS_MODERN_GRID_COMPONENT_H
#define ES_CORE_COMPONENTS_MODERN_GRID_COMPONENT_H

#include "Log.h"
#include "components/GridTileComponent.h"
#include "components/IList.h"
#include "resources/ResourceManager.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <vector>

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

struct ModernGridData
{
	std::string texturePath;
};

// ES-X modern grid.
//
// This intentionally does not inherit the old ImageGrid camera/EXTRAITEMS model.
// Logical selection, viewport and visual tiles are separate states:
//
//   mCursor               -> selected game
//   mFirstVisiblePrimary  -> first visible row/column
//   mTiles                -> visible render pool only
//
// A tile is fully reset every time it is rebound to another entry, so selection,
// animation, visibility and frame state cannot leak from a previous game.
template<typename T>
class ModernGridComponent : public IList<ModernGridData, T>
{
protected:
	using List = IList<ModernGridData, T>;
	using List::mEntries;
	using List::mCursor;
	using List::mScrollTier;
	using List::mScrollVelocity;
	using List::mSize;
	using List::getTransform;
	using List::listRenderTitleOverlay;

public:
	using List::getSelected;
	using List::setCursor;
	using List::size;

	ModernGridComponent(Window* window);

	void add(const std::string& name, const std::string& imagePath, const T& obj);
	void clear();
	bool remove(const T& obj);

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;
	void applyTheme(const std::shared_ptr<ThemeData>& theme,
	                const std::string& view,
	                const std::string& element,
	                unsigned int properties) override;
	void onSizeChanged() override;

	inline void setCursorChangedCallback(const std::function<void(CursorState state)>& func)
	{
		mCursorChangedCallback = func;
	}

	ImageSource getImageSource() const { return mImageSource; }
	bool isScrolling() const { return mRepeating; }
	void stopScrolling();

protected:
	void onCursorChanged(const CursorState& state) override;

private:
	bool isVertical() const { return mScrollDirection == SCROLL_VERTICALLY; }

	void calculateLayout();
	void rebuildTiles();
	void updateViewport();
	void bindVisibleTiles();
	int logicalIndexForCell(int localRow, int localCol) const;

	void beginDirection(int deltaRow, int deltaCol);
	void endDirection();
	bool stepCursor(int deltaRow, int deltaCol);
	int wrapIndex(int index) const;

	int visiblePrimaryCount() const;
	int visibleSecondaryCount() const;
	int totalPrimaryCount() const;
	bool canLoop() const;

	std::shared_ptr<ThemeData> mTheme;
	std::string mThemeView;

	std::vector<std::shared_ptr<GridTileComponent>> mTiles;
	Vector2f mTileSize;
	Vector2f mMargin;
	Vector4f mPadding;
	Vector2f mAutoLayout;
	float mAutoLayoutZoom;
	Vector2i mGridDimension;

	int mFirstVisiblePrimary;
	bool mLayoutValid;
	bool mEntriesDirty;

	std::string mDefaultGameTexture;
	std::string mDefaultFolderTexture;
	ScrollDirection mScrollDirection;
	ImageSource mImageSource;
	bool mCenterSelection;
	bool mScrollLoop;
	bool mAnimate;

	// Directional repeat is owned by the grid. The inherited IList repeat logic
	// is linear and can cross 2D row/column boundaries unexpectedly.
	int mHeldRow;
	int mHeldCol;
	int mHoldTime;
	int mRepeatTime;
	bool mRepeating;

	static const int REPEAT_START_DELAY = 500;
	static const int REPEAT_STEP_DELAY = 160;

	std::function<void(CursorState state)> mCursorChangedCallback;
};

template<typename T>
ModernGridComponent<T>::ModernGridComponent(Window* window)
	: IList<ModernGridData, T>(window)
{
	Vector2f screen((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	mTileSize = GridTileComponent::getDefaultTileSize();
	mMargin = screen * 0.07f;
	mPadding = Vector4f::Zero();
	mAutoLayout = Vector2f::Zero();
	mAutoLayoutZoom = 1.0f;
	mGridDimension = Vector2i(1, 1);

	mFirstVisiblePrimary = 0;
	mLayoutValid = false;
	mEntriesDirty = true;

	mDefaultGameTexture = ":/cartridge.png";
	mDefaultFolderTexture = ":/folder.svg";
	mScrollDirection = SCROLL_VERTICALLY;
	mImageSource = THUMBNAIL;
	mCenterSelection = false;
	mScrollLoop = false;
	mAnimate = false;

	mHeldRow = 0;
	mHeldCol = 0;
	mHoldTime = 0;
	mRepeatTime = 0;
	mRepeating = false;
	mScrollVelocity = 0;
	mScrollTier = 0;

	mSize = screen * 0.80f;
}

template<typename T>
void ModernGridComponent<T>::add(const std::string& name,
                                 const std::string& imagePath,
                                 const T& obj)
{
	typename List::Entry entry;
	entry.name = name;
	entry.object = obj;
	entry.data.texturePath = imagePath;
	List::add(entry);
	mEntriesDirty = true;
}

template<typename T>
void ModernGridComponent<T>::clear()
{
	endDirection();
	mEntries.clear();
	mCursor = 0;
	mFirstVisiblePrimary = 0;
	mEntriesDirty = true;
	bindVisibleTiles();
}

template<typename T>
bool ModernGridComponent<T>::remove(const T& obj)
{
	for (auto it = mEntries.begin(); it != mEntries.end(); ++it)
	{
		if (it->object != obj)
			continue;

		const int removedIndex = (int)std::distance(mEntries.begin(), it);
		mEntries.erase(it);

		if (mEntries.empty())
			mCursor = 0;
		else if (mCursor >= (int)mEntries.size())
			mCursor = (int)mEntries.size() - 1;
		else if (removedIndex < mCursor)
			--mCursor;

		mEntriesDirty = true;
		updateViewport();
		bindVisibleTiles();

		if (mCursorChangedCallback)
			mCursorChangedCallback(CURSOR_STOPPED);
		return true;
	}

	return false;
}

template<typename T>
bool ModernGridComponent<T>::input(InputConfig* config, Input input)
{
	if (!mLayoutValid)
		calculateLayout();

	if (input.value != 0)
	{
		if (config->isMappedLike("up", input))
		{
			beginDirection(-1, 0);
			return true;
		}
		if (config->isMappedLike("down", input))
		{
			beginDirection(1, 0);
			return true;
		}
		if (config->isMappedLike("left", input))
		{
			beginDirection(0, -1);
			return true;
		}
		if (config->isMappedLike("right", input))
		{
			beginDirection(0, 1);
			return true;
		}
	}
	else if (config->isMappedLike("up", input) ||
	         config->isMappedLike("down", input) ||
	         config->isMappedLike("left", input) ||
	         config->isMappedLike("right", input))
	{
		endDirection();
		return true;
	}

	return GuiComponent::input(config, input);
}

template<typename T>
void ModernGridComponent<T>::beginDirection(int deltaRow, int deltaCol)
{
	mHeldRow = deltaRow;
	mHeldCol = deltaCol;
	mHoldTime = 0;
	mRepeatTime = 0;
	mRepeating = false;
	mScrollTier = 0;

	if (deltaRow != 0)
		mScrollVelocity = deltaRow;
	else
		mScrollVelocity = deltaCol;

	stepCursor(deltaRow, deltaCol);
}

template<typename T>
void ModernGridComponent<T>::endDirection()
{
	const bool wasRepeating = mRepeating;

	mHeldRow = 0;
	mHeldCol = 0;
	mHoldTime = 0;
	mRepeatTime = 0;
	mRepeating = false;
	mScrollVelocity = 0;
	mScrollTier = 0;

	if (wasRepeating && mCursorChangedCallback)
		mCursorChangedCallback(CURSOR_STOPPED);
}

template<typename T>
void ModernGridComponent<T>::stopScrolling()
{
	endDirection();
}

template<typename T>
bool ModernGridComponent<T>::stepCursor(int deltaRow, int deltaCol)
{
	if (mEntries.empty())
		return false;

	const int columns = std::max(1, mGridDimension.x());
	const int rows = std::max(1, mGridDimension.y());
	int target = mCursor;

	if (isVertical())
	{
		if (deltaRow != 0)
			target += deltaRow * columns;
		else
			target += deltaCol;
	}
	else
	{
		if (deltaCol != 0)
			target += deltaCol * rows;
		else
			target += deltaRow;
	}

	if (target < 0 || target >= size())
	{
		if (!canLoop())
			return false;
		target = wrapIndex(target);
	}

	if (target == mCursor)
		return false;

	mCursor = target;
	onCursorChanged(mRepeating ? CURSOR_SCROLLING : CURSOR_STOPPED);
	return true;
}

template<typename T>
int ModernGridComponent<T>::wrapIndex(int index) const
{
	if (mEntries.empty())
		return 0;

	const int count = (int)mEntries.size();
	while (index < 0)
		index += count;
	while (index >= count)
		index -= count;
	return index;
}

template<typename T>
void ModernGridComponent<T>::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	if (mHeldRow != 0 || mHeldCol != 0)
	{
		mHoldTime += deltaTime;

		if (mHoldTime >= REPEAT_START_DELAY)
		{
			if (!mRepeating)
			{
				mRepeating = true;
				mScrollTier = 1;
				mRepeatTime = REPEAT_STEP_DELAY;
			}
			else
			{
				mRepeatTime += deltaTime;
			}

			// Deliberately perform at most one logical move per rendered frame.
			// Slow texture loading must not make the cursor skip several games.
			if (mRepeatTime >= REPEAT_STEP_DELAY)
			{
				mRepeatTime %= REPEAT_STEP_DELAY;
				stepCursor(mHeldRow, mHeldCol);
			}
		}
	}

	if (mEntriesDirty)
	{
		updateViewport();
		bindVisibleTiles();
		mEntriesDirty = false;
	}

	for (auto& tile : mTiles)
		tile->update(deltaTime);
}

template<typename T>
void ModernGridComponent<T>::render(const Transform4x4f& parentTrans)
{
	if (!mLayoutValid)
		calculateLayout();

	if (mEntriesDirty)
	{
		updateViewport();
		bindVisibleTiles();
		mEntriesDirty = false;
	}

	Transform4x4f trans = getTransform() * parentTrans;

	float scaleX = trans.r0().x();
	float scaleY = trans.r1().y();
	Vector2i pos((int)Math::round(trans.translation()[0]), (int)Math::round(trans.translation()[1]));
	Vector2i sizePx((int)Math::round(mSize.x() * scaleX), (int)Math::round(mSize.y() * scaleY));

	Renderer::pushClipRect(pos, sizePx);

	std::shared_ptr<GridTileComponent> selectedTile;
	for (auto& tile : mTiles)
	{
		if (!tile->isVisible())
			continue;

		if (tile->isSelected())
			selectedTile = tile;
		else
			tile->render(trans);
	}

	Renderer::popClipRect();

	// Legacy-compatible selected layer: zoom/frame is allowed to extend beyond
	// the viewport while all non-selected entries remain clipped.
	if (selectedTile)
		selectedTile->render(trans);

	listRenderTitleOverlay(trans);
	GuiComponent::renderChildren(trans);
}

template<typename T>
void ModernGridComponent<T>::applyTheme(const std::shared_ptr<ThemeData>& theme,
                                        const std::string& view,
                                        const std::string& element,
                                        unsigned int properties)
{
	GuiComponent::applyTheme(theme, view, element, properties ^ ThemeFlags::SIZE);

	mTheme = theme;
	mThemeView = view;

	Vector2f screen((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "imagegrid");

	if (elem)
	{
		if (elem->has("margin"))
			mMargin = elem->get<Vector2f>("margin") * screen;
		if (elem->has("padding"))
			mPadding = elem->get<Vector4f>("padding") *
				Vector4f(screen.x(), screen.y(), screen.x(), screen.y());
		if (elem->has("autoLayout"))
			mAutoLayout = elem->get<Vector2f>("autoLayout");
		if (elem->has("autoLayoutSelectedZoom"))
			mAutoLayoutZoom = elem->get<float>("autoLayoutSelectedZoom");

		if (elem->has("imageSource"))
		{
			const std::string source = elem->get<std::string>("imageSource");
			if (source == "image")
				mImageSource = IMAGE;
			else if (source == "marquee")
				mImageSource = MARQUEE;
			else
				mImageSource = THUMBNAIL;
		}
		else
		{
			mImageSource = THUMBNAIL;
		}

		if (elem->has("scrollDirection"))
			mScrollDirection = (elem->get<std::string>("scrollDirection") == "horizontal") ?
				SCROLL_HORIZONTALLY : SCROLL_VERTICALLY;
		if (elem->has("centerSelection"))
			mCenterSelection = elem->get<bool>("centerSelection");
		if (elem->has("scrollLoop"))
			mScrollLoop = elem->get<bool>("scrollLoop");
		if (elem->has("animate"))
			mAnimate = elem->get<bool>("animate");

		if (elem->has("gameImage"))
			mDefaultGameTexture = elem->get<std::string>("gameImage");
		if (elem->has("folderImage"))
			mDefaultFolderTexture = elem->get<std::string>("folderImage");
	}

	const ThemeData::ThemeElement* tileElem = theme->getElement(view, "default", "gridtile");
	mTileSize = (tileElem && tileElem->has("size")) ?
		tileElem->get<Vector2f>("size") * screen : GridTileComponent::getDefaultTileSize();

	GuiComponent::applyTheme(theme, view, element, ThemeFlags::SIZE);

	mLayoutValid = false;
	calculateLayout();
	updateViewport();
	bindVisibleTiles();
	mEntriesDirty = false;
}

template<typename T>
void ModernGridComponent<T>::onSizeChanged()
{
	mLayoutValid = false;
	calculateLayout();
	updateViewport();
	bindVisibleTiles();
}

template<typename T>
void ModernGridComponent<T>::onCursorChanged(const CursorState& state)
{
	updateViewport();
	bindVisibleTiles();

	if (mCursorChangedCallback)
		mCursorChangedCallback(state);
}

template<typename T>
void ModernGridComponent<T>::calculateLayout()
{
	Vector2f available(
		std::max(1.0f, mSize.x() - mPadding.x() - mPadding.z()),
		std::max(1.0f, mSize.y() - mPadding.y() - mPadding.w()));

	int columns = 1;
	int rows = 1;

	if (mAutoLayout.x() > 0.0f && mAutoLayout.y() > 0.0f)
	{
		columns = std::max(1, (int)Math::round(mAutoLayout.x()));
		rows = std::max(1, (int)Math::round(mAutoLayout.y()));

		const float tileW =
			(available.x() - (mMargin.x() * (columns - 1))) / (float)columns;
		const float tileH =
			(available.y() - (mMargin.y() * (rows - 1))) / (float)rows;

		mTileSize = Vector2f(std::max(1.0f, tileW), std::max(1.0f, tileH));
	}
	else
	{
		const float stepX = std::max(1.0f, mTileSize.x() + mMargin.x());
		const float stepY = std::max(1.0f, mTileSize.y() + mMargin.y());
		const float fitX = (available.x() + mMargin.x()) / stepX;
		const float fitY = (available.y() + mMargin.y()) / stepY;

		// Match legacy visual behavior: the scrolling axis may expose a partial
		// final line, while the secondary axis only contains complete cells.
		if (isVertical())
		{
			columns = std::max(1, (int)std::floor(fitX));
			rows = std::max(1, (int)std::ceil(fitY));
		}
		else
		{
			columns = std::max(1, (int)std::ceil(fitX));
			rows = std::max(1, (int)std::floor(fitY));
		}
	}

	mGridDimension = Vector2i(columns, rows);
	mLayoutValid = true;
	rebuildTiles();
}

template<typename T>
void ModernGridComponent<T>::rebuildTiles()
{
	mTiles.clear();

	const int columns = std::max(1, mGridDimension.x());
	const int rows = std::max(1, mGridDimension.y());
	const Vector2f start(
		mPadding.x() + (mTileSize.x() * 0.5f),
		mPadding.y() + (mTileSize.y() * 0.5f));
	const Vector2f step = mTileSize + mMargin;

	for (int row = 0; row < rows; ++row)
	{
		for (int col = 0; col < columns; ++col)
		{
			auto tile = std::make_shared<GridTileComponent>(this->mWindow);
			tile->setPosition(
				start.x() + (col * step.x()),
				start.y() + (row * step.y()));
			tile->setOrigin(0.5f, 0.5f);
			tile->setImage("");

			if (mTheme)
				tile->applyTheme(mTheme, mThemeView, "gridtile", ThemeFlags::ALL);

			if (mAutoLayout.x() > 0.0f && mAutoLayout.y() > 0.0f)
				tile->forceSize(mTileSize, mAutoLayoutZoom);

			mTiles.push_back(tile);
		}
	}
}

template<typename T>
void ModernGridComponent<T>::updateViewport()
{
	if (!mLayoutValid)
		calculateLayout();

	if (mEntries.empty())
	{
		mFirstVisiblePrimary = 0;
		return;
	}

	const int secondary = visibleSecondaryCount();
	const int primaryVisible = visiblePrimaryCount();
	const int totalPrimary = totalPrimaryCount();
	const int selectedPrimary = mCursor / std::max(1, secondary);
	const int maxFirst = std::max(0, totalPrimary - primaryVisible);

	if (mCenterSelection)
	{
		mFirstVisiblePrimary = selectedPrimary - (primaryVisible / 2);
	}
	else
	{
		if (selectedPrimary < mFirstVisiblePrimary)
			mFirstVisiblePrimary = selectedPrimary;
		else if (selectedPrimary >= mFirstVisiblePrimary + primaryVisible)
			mFirstVisiblePrimary = selectedPrimary - primaryVisible + 1;
	}

	mFirstVisiblePrimary = std::max(0, std::min(maxFirst, mFirstVisiblePrimary));
}

template<typename T>
void ModernGridComponent<T>::bindVisibleTiles()
{
	if (!mLayoutValid || mTiles.empty())
		return;

	const int columns = std::max(1, mGridDimension.x());
	const int rows = std::max(1, mGridDimension.y());

	for (int row = 0; row < rows; ++row)
	{
		for (int col = 0; col < columns; ++col)
		{
			const int tilePos = row * columns + col;
			if (tilePos < 0 || tilePos >= (int)mTiles.size())
				continue;

			auto& tile = mTiles.at(tilePos);
			const int index = logicalIndexForCell(row, col);

			// Rebinding is atomic from the visual point of view: old entry state
			// is destroyed before new image/frame/selection state is applied.
			tile->cancelAllAnimations();
			tile->setSelected(false, false, nullptr, true);
			tile->setVisible(false);
			tile->reset();

			if (index < 0 || index >= size())
				continue;

			const std::string imagePath = mEntries.at(index).data.texturePath;
			if (!imagePath.empty() && ResourceManager::getInstance()->fileExists(imagePath))
			{
				tile->setImage(imagePath);
			}
			else if (mEntries.at(index).object->getType() == 2)
			{
				tile->setImage(mDefaultFolderTexture);
			}
			else
			{
				tile->setImage(mDefaultGameTexture);
			}

			tile->setSelected(index == mCursor, false, nullptr, true);
			tile->setVisible(true);
		}
	}
}

template<typename T>
int ModernGridComponent<T>::logicalIndexForCell(int localRow, int localCol) const
{
	const int columns = std::max(1, mGridDimension.x());
	const int rows = std::max(1, mGridDimension.y());

	if (isVertical())
	{
		const int absoluteRow = mFirstVisiblePrimary + localRow;
		return absoluteRow * columns + localCol;
	}

	const int absoluteCol = mFirstVisiblePrimary + localCol;
	return absoluteCol * rows + localRow;
}

template<typename T>
int ModernGridComponent<T>::visiblePrimaryCount() const
{
	return isVertical() ?
		std::max(1, mGridDimension.y()) : std::max(1, mGridDimension.x());
}

template<typename T>
int ModernGridComponent<T>::visibleSecondaryCount() const
{
	return isVertical() ?
		std::max(1, mGridDimension.x()) : std::max(1, mGridDimension.y());
}

template<typename T>
int ModernGridComponent<T>::totalPrimaryCount() const
{
	if (mEntries.empty())
		return 0;

	const int secondary = visibleSecondaryCount();
	return (size() + secondary - 1) / secondary;
}

template<typename T>
bool ModernGridComponent<T>::canLoop() const
{
	if (!mScrollLoop)
		return false;

	return size() >= (visiblePrimaryCount() * visibleSecondaryCount());
}

#endif // ES_CORE_COMPONENTS_MODERN_GRID_COMPONENT_H
