#include "views/gamelist/GridGameListView.h"

#include "ThemeData.h"
#include "Log.h"
#include "Window.h"
#include "animations/LambdaAnimation.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "Settings.h"
#include "SystemData.h"
#include "utils/FileSystemUtil.h"

#ifdef _OMX_
#include "components/VideoPlayerComponent.h"
#endif
#include "components/VideoVlcComponent.h"

#include "LocaleES.h"

#include <algorithm>
#include <cassert>

namespace
{
	static bool themeElemExists(const std::shared_ptr<ThemeData>& theme,
		const std::string& view, const std::string& elem, const std::string& type)
	{
		if (!theme)
			return false;

		return theme->getElement(view, elem, type) != nullptr;
	}

	static void applyIfExistsOrHide(GuiComponent& c,
		const std::shared_ptr<ThemeData>& theme,
		const std::string& view,
		const std::string& elem,
		const std::string& type,
		unsigned int flags)
	{
		if (themeElemExists(theme, view, elem, type))
		{
			c.setVisible(true);
			c.applyTheme(theme, view, elem, flags);
		}
		else
		{
			c.setVisible(false);
		}
	}
}

GridGameListView::GridGameListView(Window* window, FileData* root) :
	ISimpleGameListView(window, root),
	mGrid(window),
	mBackground(window),
	mLblRating(window), mLblReleaseDate(window), mLblDeveloper(window), mLblPublisher(window),
	mLblGenre(window), mLblPlayers(window), mLblLastPlayed(window), mLblPlayCount(window),
	mMarquee(window),
	mVideo(nullptr),
	mVideoPlaying(false),
	mImage(window),
	mPlayersIcon(window),
	mRating(window),
	mReleaseDate(window),
	mDeveloper(window),
	mPublisher(window),
	mGenre(window),
	mPlayers(window),
	mLastPlayed(window),
	mPlayCount(window),
	mName(window),
	mDescContainer(window, DESCRIPTION_SCROLL_DELAY),
	mDescription(window)
{
	LocaleES& loc = LocaleES::getInstance();

#ifdef _OMX_
	if (Settings::getInstance()->getBool("VideoOmxPlayer"))
		mVideo = new VideoPlayerComponent(window, "");
	else
		mVideo = new VideoVlcComponent(window, getTitlePath());
#else
	mVideo = new VideoVlcComponent(window, getTitlePath());
#endif

	// Fondo dinámico por juego: se comporta como fondo normal
	mBackground.setOrigin(0.0f, 0.0f);
	mBackground.setPosition(0.0f, 0.0f);
	mBackground.setResize(mSize.x(), mSize.y());
	mBackground.setDefaultZIndex(1);
	mBackground.setVisible(false);
	addChild(&mBackground);

	// Grid
	mGrid.setPosition(mSize.x() * 0.05f, mSize.y() * 0.18f);
	mGrid.setSize(mSize.x() * 0.55f, mSize.y() * 0.60f);
	mGrid.setDefaultZIndex(20);
	mGrid.setCursorChangedCallback([&](const CursorState& /*state*/) { updateInfoPanel(); });
	addChild(&mGrid);

	populateList(root->getChildrenListToDisplay());

	// Labels
	mLblRating.setText(loc.translate("RATING") + ": ");
	addChild(&mLblRating);
	addChild(&mRating);

	mLblReleaseDate.setText(loc.translate("RELEASE DATE") + ": ");
	addChild(&mLblReleaseDate);
	addChild(&mReleaseDate);

	mLblDeveloper.setText(loc.translate("DEVELOPER") + ": ");
	addChild(&mLblDeveloper);
	addChild(&mDeveloper);

	mLblPublisher.setText(loc.translate("PUBLISHER") + ": ");
	addChild(&mLblPublisher);
	addChild(&mPublisher);

	mLblGenre.setText(loc.translate("GENRE") + ": ");
	addChild(&mLblGenre);
	addChild(&mGenre);

	mLblPlayers.setText(loc.translate("PLAYERS") + ": ");
	addChild(&mLblPlayers);
	addChild(&mPlayers);

	mLblLastPlayed.setText(loc.translate("LAST PLAYED") + ": ");
	addChild(&mLblLastPlayed);
	mLastPlayed.setDisplayRelative(true);
	addChild(&mLastPlayed);

	mLblPlayCount.setText(loc.translate("PLAY COUNT") + ": ");
	addChild(&mLblPlayCount);
	addChild(&mPlayCount);

	// Nombre
	mName.setPosition(mSize.x() * 0.63f, mSize.y() * 0.18f);
	mName.setSize(mSize.x() * 0.32f, 0);
	mName.setDefaultZIndex(40);
	mName.setColor(0xAAAAAAFF);
	mName.setFont(Font::get(FONT_SIZE_MEDIUM));
	mName.setHorizontalAlignment(ALIGN_CENTER);
	addChild(&mName);

	// Descripción
	mDescContainer.setPosition(mSize.x() * 0.63f, mSize.y() * 0.44f);
	mDescContainer.setSize(mSize.x() * 0.32f, mSize.y() - mDescContainer.getPosition().y());
	mDescContainer.setAutoScroll(true);
	mDescContainer.setDefaultZIndex(40);
	addChild(&mDescContainer);

	mDescription.setFont(Font::get(FONT_SIZE_SMALL));
	mDescription.setSize(mDescContainer.getSize().x(), 0);
	mDescContainer.addChild(&mDescription);

	// Imagen
	mImage.setOrigin(0.5f, 0.5f);
	mImage.setPosition(2.0f, 2.0f);
	mImage.setMaxSize(mSize.x(), mSize.y());
	mImage.setDefaultZIndex(30);
	mImage.setVisible(false);
	addChild(&mImage);

	// Metadata-controlled badge; invisible for themes that do not opt in.
	mPlayersIcon.setDefaultZIndex(105);
	addChild(&mPlayersIcon);

	// Video
	mVideo->setOrigin(0.5f, 0.5f);
	mVideo->setPosition(2.0f, 2.0f);
	mVideo->setSize(mSize.x(), mSize.y());
	mVideo->setDefaultZIndex(30);
	mVideo->setVisible(false);
	addChild(mVideo);

	// Marquee
	mMarquee.setOrigin(0.5f, 0.5f);
	mMarquee.setPosition(2.0f, 2.0f);
	mMarquee.setMaxSize(mSize.x(), mSize.y());
	mMarquee.setDefaultZIndex(35);
	mMarquee.setVisible(false);
	addChild(&mMarquee);

	for (auto* l : getMDLabels())
		l->setVisible(false);

	for (auto* v : getMDValues())
		v->setVisible(false);

	mDescContainer.setVisible(false);
	mDescription.setVisible(false);

	initMDLabels();
	initMDValues();
	updateInfoPanel();
}

GridGameListView::~GridGameListView()
{
	delete mVideo;
}

void GridGameListView::onShow()
{
	GuiComponent::onShow();
	updateInfoPanel();
}

void GridGameListView::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	ISimpleGameListView::onThemeChanged(theme);

	using namespace ThemeFlags;

	mGrid.applyTheme(theme, getName(), "gamegrid", ALL);
	mName.applyTheme(theme, getName(), "md_name", ALL);
	mBackground.applyTheme(theme, getName(), "md_background", POSITION | SIZE | Z_INDEX | ROTATION | VISIBLE);
	mMarquee.applyTheme(theme, getName(), "md_marquee", POSITION | SIZE | Z_INDEX | ROTATION | VISIBLE);
	mImage.applyTheme(theme, getName(), "md_image", POSITION | SIZE | Z_INDEX | ROTATION | VISIBLE);
	mPlayersIcon.applyPlayersTheme(theme, getName());
	mVideo->applyTheme(theme, getName(), "md_video", POSITION | SIZE | DELAY | Z_INDEX | ROTATION | VISIBLE);

	initMDLabels();

	applyIfExistsOrHide(mLblRating,      theme, getName(), "md_lbl_rating",      "text", ALL);
	applyIfExistsOrHide(mLblReleaseDate, theme, getName(), "md_lbl_releasedate", "text", ALL);
	applyIfExistsOrHide(mLblDeveloper,   theme, getName(), "md_lbl_developer",   "text", ALL);
	applyIfExistsOrHide(mLblPublisher,   theme, getName(), "md_lbl_publisher",   "text", ALL);
	applyIfExistsOrHide(mLblGenre,       theme, getName(), "md_lbl_genre",       "text", ALL);
	applyIfExistsOrHide(mLblPlayers,     theme, getName(), "md_lbl_players",     "text", ALL);
	applyIfExistsOrHide(mLblLastPlayed,  theme, getName(), "md_lbl_lastplayed",  "text", ALL);
	applyIfExistsOrHide(mLblPlayCount,   theme, getName(), "md_lbl_playcount",   "text", ALL);

	initMDValues();

	applyIfExistsOrHide(mRating,      theme, getName(), "md_rating",      "rating",   ALL ^ ThemeFlags::TEXT);
	applyIfExistsOrHide(mReleaseDate, theme, getName(), "md_releasedate", "datetime", ALL ^ ThemeFlags::TEXT);
	applyIfExistsOrHide(mDeveloper,   theme, getName(), "md_developer",   "text",     ALL ^ ThemeFlags::TEXT);
	applyIfExistsOrHide(mPublisher,   theme, getName(), "md_publisher",   "text",     ALL ^ ThemeFlags::TEXT);
	applyIfExistsOrHide(mGenre,       theme, getName(), "md_genre",       "text",     ALL ^ ThemeFlags::TEXT);
	applyIfExistsOrHide(mPlayers,     theme, getName(), "md_players",     "text",     ALL ^ ThemeFlags::TEXT);
	applyIfExistsOrHide(mLastPlayed,  theme, getName(), "md_lastplayed",  "datetime", ALL ^ ThemeFlags::TEXT);
	applyIfExistsOrHide(mPlayCount,   theme, getName(), "md_playcount",   "text",     ALL ^ ThemeFlags::TEXT);

	applyIfExistsOrHide(mDescContainer, theme, getName(), "md_description", "text",
		POSITION | SIZE | Z_INDEX | VISIBLE);

	if (mDescContainer.isVisible())
	{
		mDescription.setVisible(true);
		mDescription.setSize(mDescContainer.getSize().x(), 0);
		mDescription.applyTheme(theme, getName(), "md_description",
			ALL ^ (POSITION | SIZE | ThemeFlags::ORIGIN | TEXT | ROTATION));
	}
	else
	{
		mDescription.setVisible(false);
	}

	FileData* file = mGrid.getSelected();
	populateList(mRoot->getChildrenListToDisplay());

	if (file && !file->isPlaceHolder())
		mGrid.setCursor(file);

	mPlayersIcon.setPlayers(mGrid.getSelected() && !mGrid.getSelected()->isPlaceHolder()
		? mGrid.getSelected()->metadata.get("players") : "");
	sortChildren();
}

FileData* GridGameListView::getCursor()
{
	return mGrid.getSelected();
}

void GridGameListView::setCursor(FileData* file, bool refreshListCursorPos)
{
	if (!file)
		return;

	if (!mGrid.setCursor(file) && !file->isPlaceHolder())
	{
		populateList(file->getParent()->getChildrenListToDisplay());
		mGrid.setCursor(file);
	}
}

bool GridGameListView::input(InputConfig* config, Input input)
{
	if (config->isMappedLike("left", input) || config->isMappedLike("right", input))
		return GuiComponent::input(config, input);

	return ISimpleGameListView::input(config, input);
}

std::vector<HelpPrompt> GridGameListView::getHelpPrompts()
{
	LocaleES& loc = LocaleES::getInstance();
	std::vector<HelpPrompt> prompts;

	if (Settings::getInstance()->getBool("QuickSystemSelect"))
		prompts.push_back(HelpPrompt("lr", loc.translate("SYSTEM")));

	prompts.push_back(HelpPrompt("up/down/left/right", loc.translate("CHOOSE")));
	prompts.push_back(HelpPrompt("a", loc.translate("START")));
	prompts.push_back(HelpPrompt("b", loc.translate("BACK")));

	if (!UIModeController::getInstance()->isUIModeKid())
		prompts.push_back(HelpPrompt("select", loc.translate("OPTIONS")));

	if (mRoot->getSystem()->isGameSystem())
		prompts.push_back(HelpPrompt("x", loc.translate("RANDOM")));

	if (mRoot->getSystem()->isGameSystem() && !UIModeController::getInstance()->isUIModeKid())
	{
		std::string prompt = CollectionSystemManager::get()->getEditingCollection();
		prompts.push_back(HelpPrompt("y", prompt));
	}

	return prompts;
}

void GridGameListView::launch(FileData* game)
{
	float screenWidth = (float)Renderer::getScreenWidth();
	float screenHeight = (float)Renderer::getScreenHeight();

	Vector3f target(screenWidth / 2.0f, screenHeight / 2.0f, 0);

	if (mMarquee.hasImage() &&
		(mMarquee.getPosition().x() < screenWidth && mMarquee.getPosition().x() > 0.0f &&
		 mMarquee.getPosition().y() < screenHeight && mMarquee.getPosition().y() > 0.0f))
	{
		target = Vector3f(mMarquee.getCenter().x(), mMarquee.getCenter().y(), 0);
	}
	else if (mImage.hasImage() &&
		(mImage.getPosition().x() < screenWidth && mImage.getPosition().x() > 2.0f &&
		 mImage.getPosition().y() < screenHeight && mImage.getPosition().y() > 2.0f))
	{
		target = Vector3f(mImage.getCenter().x(), mImage.getCenter().y(), 0);
	}
	else if (mVideo->getPosition().x() < screenWidth && mVideo->getPosition().x() > 0.0f &&
		mVideo->getPosition().y() < screenHeight && mVideo->getPosition().y() > 0.0f)
	{
		target = Vector3f(mVideo->getCenter().x(), mVideo->getCenter().y(), 0);
	}

	ViewController::get()->launch(game, target);
}

void GridGameListView::onFocusLost(void)
{
	mDescContainer.reset();
}

void GridGameListView::update(int deltaTime)
{
	ISimpleGameListView::update(deltaTime);
	mVideo->update(deltaTime);
}

std::string GridGameListView::getQuickSystemSelectRightButton()
{
	return "rightshoulder";
}

std::string GridGameListView::getQuickSystemSelectLeftButton()
{
	return "leftshoulder";
}

void GridGameListView::populateList(const std::vector<FileData*>& files)
{
	mGrid.clear();
	mHeaderText.setText(mRoot->getSystem()->getFullName());

	if (!files.empty())
	{
		for (auto it = files.cbegin(); it != files.cend(); ++it)
			mGrid.add((*it)->getName(), getImagePath(*it), *it);
	}
	else
	{
		addPlaceholder();
	}
}

void GridGameListView::remove(FileData* game, bool deleteFile, bool refreshView)
{
	if (deleteFile)
		Utils::FileSystem::removeFile(game->getPath());

	FileData* parent = game->getParent();
	if (getCursor() == game)
	{
		std::vector<FileData*> siblings = parent->getChildrenListToDisplay();
		auto gameIter = std::find(siblings.cbegin(), siblings.cend(), game);
		int gamePos = (int)std::distance(siblings.cbegin(), gameIter);

		if (gameIter != siblings.cend())
		{
			if ((gamePos + 1) < (int)siblings.size())
				setCursor(siblings.at(gamePos + 1));
			else if ((gamePos - 1) >= 0)
				setCursor(siblings.at(gamePos - 1));
		}
	}

	mGrid.remove(game);
	if (mGrid.size() == 0)
		addPlaceholder();

	delete game;

	if (refreshView)
		onFileChanged(parent, FILE_REMOVED);
}

void GridGameListView::addPlaceholder()
{
	LocaleES& loc = LocaleES::getInstance();

	std::string placeholderName = loc.translate("NO ENTRIES FOUND");
	if (placeholderName == "NO ENTRIES FOUND")
		placeholderName = "<No Entries Found>";

	FileData* placeholder = new FileData(
		PLACEHOLDER,
		placeholderName,
		this->mRoot->getSystem()->getSystemEnvData(),
		this->mRoot->getSystem());

	mGrid.add(placeholder->getName(), "", placeholder);
}

void GridGameListView::updateInfoPanel()
{
	FileData* file = (mGrid.size() == 0 || mGrid.isScrolling()) ? nullptr : mGrid.getSelected();

	bool fadingOut;
	if (file == nullptr)
	{
		mPlayersIcon.setPlayers("");
		mBackground.setImage("");
		mBackground.setVisible(false);

		mVideo->setVideo("");
		mVideo->setImage("");
		mVideoPlaying = false;
		fadingOut = true;
	}
	else
	{
		const std::string bgPath = file->getBackgroundPath();
		if (!bgPath.empty())
		{
			mBackground.setImage(bgPath);
			mBackground.setVisible(true);
		}
		else
		{
			mBackground.setImage("");
			mBackground.setVisible(false);
		}

		if (!mVideo->setVideo(file->getVideoPath()))
			mVideo->setDefaultVideo();

		mVideoPlaying = true;

		mVideo->setImage(file->getThumbnailPath());
		mMarquee.setImage(file->getMarqueePath());
		mImage.setImage(file->getImagePath());

		if (mDescription.isVisible())
		{
			mDescription.setText(file->metadata.get("desc"));
			mDescContainer.reset();
		}

		if (mRating.isVisible())      mRating.setValue(file->metadata.get("rating"));
		if (mReleaseDate.isVisible()) mReleaseDate.setValue(file->metadata.get("releasedate"));
		if (mDeveloper.isVisible())   mDeveloper.setValue(file->metadata.get("developer"));
		if (mPublisher.isVisible())   mPublisher.setValue(file->metadata.get("publisher"));
		if (mGenre.isVisible())       mGenre.setValue(file->metadata.get("genre"));
		if (mPlayers.isVisible())     mPlayers.setValue(file->metadata.get("players"));
		mPlayersIcon.setPlayers(file->metadata.get("players"));

		std::string displayName = file->metadata.get("name");
		if (displayName.empty())
			displayName = file->getName();
		mName.setValue(displayName);

		if (file->getType() == GAME)
		{
			if (mLastPlayed.isVisible())
				mLastPlayed.setValue(file->metadata.get("lastplayed"));
			if (mPlayCount.isVisible())
				mPlayCount.setValue(file->metadata.get("playcount"));
		}

		fadingOut = false;
	}

	// El fondo no entra en el fade del panel
	std::vector<GuiComponent*> comps = getMDValues();
	comps.push_back(&mPlayersIcon);
	comps.push_back(&mDescription);
	comps.push_back(&mName);
	comps.push_back(&mMarquee);
	comps.push_back(mVideo);
	comps.push_back(&mImage);

	std::vector<TextComponent*> labels = getMDLabels();
	comps.insert(comps.cend(), labels.cbegin(), labels.cend());

	for (auto it = comps.cbegin(); it != comps.cend(); ++it)
	{
		GuiComponent* comp = *it;

		if (!comp->isVisible())
			continue;

		if ((comp->isAnimationPlaying(0) && comp->isAnimationReversed(0) != fadingOut) ||
			(!comp->isAnimationPlaying(0) && comp->getOpacity() != (fadingOut ? 0 : 255)))
		{
			auto func = [comp](float t)
			{
				comp->setOpacity((unsigned char)(Math::lerp(0.0f, 1.0f, t) * 255));
			};
			comp->setAnimation(new LambdaAnimation(func, 150), 0, nullptr, fadingOut);
		}
	}
}

const std::string GridGameListView::getImagePath(FileData* file)
{
	if (!file)
		return "";

	ImageSource src = mGrid.getImageSource();

	if (src == ImageSource::MARQUEE)
	{
		std::string marquee = file->getMarqueePath();
		if (!marquee.empty())
			return marquee;
	}

	std::string gridImage = file->getGridImagePath();
	if (!gridImage.empty())
		return gridImage;

	if (src == ImageSource::IMAGE)
	{
		std::string image = file->getImagePath();
		if (!image.empty())
			return image;
	}

	std::string thumb = file->getThumbnailPath();
	if (!thumb.empty())
		return thumb;

	return file->getImagePath();
}

void GridGameListView::initMDLabels()
{
	std::vector<TextComponent*> components = getMDLabels();

	const unsigned int colCount = 2;
	const unsigned int rowCount = (unsigned int)(components.size() / 2);

	Vector3f start(mSize.x() * 0.63f, mSize.y() * 0.26f, 0.0f);

	const float colSize = (mSize.x() * 0.35f) / colCount;
	const float rowPadding = 0.01f * mSize.y();

	for (unsigned int i = 0; i < components.size(); i++)
	{
		const unsigned int row = i % rowCount;
		Vector3f pos(0.0f, 0.0f, 0.0f);

		if (row == 0)
		{
			pos = start + Vector3f(colSize * (i / rowCount), 0, 0);
		}
		else
		{
			GuiComponent* lc = components[i - 1];
			pos = lc->getPosition() + Vector3f(0, lc->getSize().y() + rowPadding, 0);
		}

		components[i]->setFont(Font::get(FONT_SIZE_SMALL));
		components[i]->setPosition(pos);
		components[i]->setDefaultZIndex(40);
	}
}

void GridGameListView::initMDValues()
{
	std::vector<TextComponent*> labels = getMDLabels();
	std::vector<GuiComponent*> values = getMDValues();

	std::shared_ptr<Font> defaultFont = Font::get(FONT_SIZE_SMALL);
	mRating.setSize(defaultFont->getHeight() * 5.0f, (float)defaultFont->getHeight());
	mReleaseDate.setFont(defaultFont);
	mDeveloper.setFont(defaultFont);
	mPublisher.setFont(defaultFont);
	mGenre.setFont(defaultFont);
	mPlayers.setFont(defaultFont);
	mLastPlayed.setFont(defaultFont);
	mPlayCount.setFont(defaultFont);

	float bottom = 0.0f;
	const float colSize = (mSize.x() * 0.48f) / 2;

	for (unsigned int i = 0; i < labels.size(); i++)
	{
		const float heightDiff = (labels[i]->getSize().y() - values[i]->getSize().y()) / 2;
		values[i]->setPosition(labels[i]->getPosition() + Vector3f(labels[i]->getSize().x(), heightDiff, 0));
		values[i]->setSize(colSize - labels[i]->getSize().x(), values[i]->getSize().y());
		values[i]->setDefaultZIndex(40);

		float testBot = values[i]->getPosition().y() + values[i]->getSize().y();
		if (testBot > bottom)
			bottom = testBot;
	}

	mDescContainer.setPosition(mDescContainer.getPosition().x(), bottom + mSize.y() * 0.01f);

	float newHeight = mSize.y() - mDescContainer.getPosition().y();
	if (newHeight < 0.0f)
		newHeight = 0.0f;

	mDescContainer.setSize(mDescContainer.getSize().x(), newHeight);
}

std::vector<TextComponent*> GridGameListView::getMDLabels()
{
	std::vector<TextComponent*> ret;
	ret.push_back(&mLblRating);
	ret.push_back(&mLblReleaseDate);
	ret.push_back(&mLblDeveloper);
	ret.push_back(&mLblPublisher);
	ret.push_back(&mLblGenre);
	ret.push_back(&mLblPlayers);
	ret.push_back(&mLblLastPlayed);
	ret.push_back(&mLblPlayCount);
	return ret;
}

std::vector<GuiComponent*> GridGameListView::getMDValues()
{
	std::vector<GuiComponent*> ret;
	ret.push_back(&mRating);
	ret.push_back(&mReleaseDate);
	ret.push_back(&mDeveloper);
	ret.push_back(&mPublisher);
	ret.push_back(&mGenre);
	ret.push_back(&mPlayers);
	ret.push_back(&mLastPlayed);
	ret.push_back(&mPlayCount);
	return ret;
}
