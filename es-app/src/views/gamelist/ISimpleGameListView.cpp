#include "views/gamelist/ISimpleGameListView.h"

#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "Scripting.h"
#include "Settings.h"
#include "Sound.h"
#include "SystemData.h"
#include "LocaleES.h"
#include "NavigationSounds.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include <algorithm>
#include <cctype>

namespace
{
	bool isCollectionOverlayExtra(const std::string& name)
	{
		return Utils::String::startsWith(name, "overlay-art");
	}

	bool isOverlayImageFile(const std::string& path)
	{
		const std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(path));
		return ext == ".png" || ext == ".jpg" || ext == ".jpeg";
	}

	std::string normalizeOverlayKey(const std::string& value)
	{
		const std::string lower = Utils::String::toLower(Utils::String::removeParenthesis(value));
		std::string normalized;
		normalized.reserve(lower.size());

		for(char c : lower)
		{
			const unsigned char uc = static_cast<unsigned char>(c);
			if(std::isalnum(uc))
				normalized.push_back(static_cast<char>(uc));
		}

		return normalized;
	}

	void appendUnique(std::vector<std::string>& values, const std::string& value)
	{
		if(!value.empty() && std::find(values.cbegin(), values.cend(), value) == values.cend())
			values.push_back(value);
	}

	void appendOverlayAliasCandidates(std::vector<std::string>& values, const std::string& key)
	{
		if(key.find("teenagemutantninjaturtles") != std::string::npos || key.find("tmnt") != std::string::npos)
			appendUnique(values, "ninjaturtles");

		if(key.find("dragonballz") != std::string::npos || key.find("dbz") != std::string::npos)
			appendUnique(values, "dragonball");

		if(key.find("kingoffighters") != std::string::npos || key.find("kof") != std::string::npos)
			appendUnique(values, "kingoffighters");

		if(key.find("metalgearsolid") != std::string::npos)
			appendUnique(values, "metalgear");
	}

	int getOverlayMatchScore(const std::vector<std::string>& gameKeys, const std::string& overlayKey)
	{
		if(overlayKey.empty())
			return 0;

		if(overlayKey.size() < 3 && overlayKey != "ys" && overlayKey != "007")
			return 0;

		int bestScore = 0;

		for(const std::string& gameKey : gameKeys)
		{
			if(gameKey.empty())
				continue;

			if(gameKey == overlayKey)
				bestScore = std::max(bestScore, 1000 + static_cast<int>(overlayKey.size()));
			else if(gameKey.find(overlayKey) != std::string::npos)
				bestScore = std::max(bestScore, 700 + static_cast<int>(overlayKey.size()));
			else if(gameKey.size() >= 6 && overlayKey.find(gameKey) != std::string::npos)
				bestScore = std::max(bestScore, 450 + static_cast<int>(gameKey.size()));
		}

		return bestScore;
	}
}


ISimpleGameListView::ISimpleGameListView(Window* window, FileData* root)
	: IGameListView(window, root),
	mHeaderText(window), mHeaderImage(window), mBackground(window)
{
	LocaleES& loc = LocaleES::getInstance();
	loc.loadFromSettings();

	mHeaderText.setText(loc.translate("LOGO TEXT"));
	mHeaderText.setSize(mSize.x(), 0);
	mHeaderText.setPosition(0, 0);
	mHeaderText.setHorizontalAlignment(ALIGN_CENTER);
	mHeaderText.setDefaultZIndex(50);

	mHeaderImage.setResize(0, mSize.y() * 0.185f);
	mHeaderImage.setOrigin(0.5f, 0.0f);
	mHeaderImage.setPosition(mSize.x() / 2, 0);
	mHeaderImage.setDefaultZIndex(50);

	mBackground.setResize(mSize.x(), mSize.y());
	mBackground.setDefaultZIndex(0);

	addChild(&mHeaderText);
	addChild(&mBackground);
}

ISimpleGameListView::~ISimpleGameListView()
{
	for (auto& extra : mThemeExtras)
	{
		if (extra.component)
		{
			removeChild(extra.component);
			delete extra.component;
		}
	}

	mThemeExtras.clear();
}

void ISimpleGameListView::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	using namespace ThemeFlags;

	mBackground.applyTheme(theme, getName(), "background", ALL);
	mHeaderImage.applyTheme(theme, getName(), "logo", ALL);
	mHeaderText.applyTheme(theme, getName(), "logoText", ALL);

	for (auto& extra : mThemeExtras)
	{
		if (extra.component)
		{
			removeChild(extra.component);
			delete extra.component;
		}
	}
	mThemeExtras.clear();

	mThemeExtras = ThemeData::makeExtrasWithMetadata(theme, getName(), mWindow);
	for (auto& extra : mThemeExtras)
	{
		if (extra.component)
			addChild(extra.component);
	}

	updateThemeExtrasVisibility();

	if (mHeaderImage.hasImage())
	{
		removeChild(&mHeaderText);
		addChild(&mHeaderImage);
	}
	else
	{
		addChild(&mHeaderText);
		removeChild(&mHeaderImage);
	}
}


std::string ISimpleGameListView::resolveCollectionOverlay(FileData* game) const
{
	if(!game || game->getType() != GAME || !mRoot || !mRoot->getSystem())
		return "";

	const std::string themeXml = ThemeData::getThemeFromCurrentSet(mRoot->getSystem()->getName());
	if(themeXml.empty())
		return "";

	const std::string systemThemeDir = Utils::FileSystem::getParent(themeXml);
	const std::string themeRootDir = Utils::FileSystem::getParent(systemThemeDir);

	std::vector<std::string> overlayDirs;
	appendUnique(overlayDirs, themeRootDir + "/_inc/collection-overlays");
	appendUnique(overlayDirs, themeRootDir + "/_theme_inc/collection-overlays");
	appendUnique(overlayDirs, systemThemeDir + "/_inc/collection-overlays");
	appendUnique(overlayDirs, systemThemeDir + "/_theme_inc/collection-overlays");

	std::vector<std::string> gameKeys;
	appendUnique(gameKeys, normalizeOverlayKey(game->getName()));
	appendUnique(gameKeys, normalizeOverlayKey(game->getCleanName()));
	appendUnique(gameKeys, normalizeOverlayKey(game->getDisplayName()));
	appendUnique(gameKeys, normalizeOverlayKey(Utils::FileSystem::getStem(game->getPath())));

	const size_t baseKeyCount = gameKeys.size();
	for(size_t i = 0; i < baseKeyCount; i++)
		appendOverlayAliasCandidates(gameKeys, gameKeys[i]);

	int bestScore = 0;
	std::string bestPath;

	for(const std::string& dir : overlayDirs)
	{
		if(!Utils::FileSystem::isDirectory(dir))
			continue;

		const Utils::FileSystem::stringList files = Utils::FileSystem::getDirContent(dir);
		for(auto it = files.cbegin(); it != files.cend(); it++)
		{
			if(!Utils::FileSystem::isRegularFile(*it) || !isOverlayImageFile(*it))
				continue;

			const std::string overlayKey = normalizeOverlayKey(Utils::FileSystem::getStem(*it));
			const int score = getOverlayMatchScore(gameKeys, overlayKey);

			if(score > bestScore)
			{
				bestScore = score;
				bestPath = *it;
			}
		}
	}

	return bestPath;
}

bool ISimpleGameListView::isSingleGameList()
{
	FileData* cursor = getCursor();

	if (!cursor || cursor->isPlaceHolder())
		return false;

	FileData* parent = cursor->getParent();
	if (!parent)
		return false;

	const std::vector<FileData*>& list = parent->getChildrenListToDisplay();

	return list.size() <= 1;
}

void ISimpleGameListView::updateThemeExtrasVisibility()
{
	const bool single = isSingleGameList();
	const std::string collectionOverlay = resolveCollectionOverlay(getCursor());
	bool overlayApplied = false;

	for (auto& extra : mThemeExtras)
	{
		if (!extra.component)
			continue;

		bool visible = !(single && extra.hideWhenSingleGame);

		if (isCollectionOverlayExtra(extra.name))
		{
			ImageComponent* image = dynamic_cast<ImageComponent*>(extra.component);
			const bool useThisOverlay = image && !overlayApplied && !collectionOverlay.empty();

			if (useThisOverlay)
			{
				image->setImage(collectionOverlay);
				overlayApplied = true;
			}

			visible = visible && useThisOverlay;
		}

		extra.component->setVisible(visible);
	}
}

void ISimpleGameListView::onFileChanged(FileData* /*file*/, FileChangeType /*change*/)
{
	FileData* cursor = getCursor();

	if (!cursor)
	{
		updateThemeExtrasVisibility();
		return;
	}

	if (!cursor->isPlaceHolder())
	{
		populateList(cursor->getParent()->getChildrenListToDisplay());
		setCursor(cursor);
	}
	else
	{
		populateList(mRoot->getChildrenListToDisplay());
		setCursor(cursor);
	}

	updateThemeExtrasVisibility();
}

bool ISimpleGameListView::input(InputConfig* config, Input input)
{
	bool horizontalGameList = false;
	const std::shared_ptr<ThemeData>& activeTheme = getTheme();

	if (activeTheme)
	{
		const ThemeData::ThemeElement* gamelistElem = activeTheme->getElement(getName(), "gamelist", "textlist");
		if (gamelistElem && gamelistElem->has("orientation"))
		{
			const std::string orientation = gamelistElem->get<std::string>("orientation");
			horizontalGameList = (orientation == "horizontal");
		}
	}

	if (input.value != 0)
	{
		if (config->isMappedTo("a", input))
		{
			FileData* cursor = getCursor();

			if (!cursor)
			{
				updateThemeExtrasVisibility();
				return true;
			}

			if (cursor->getType() == GAME)
			{
				std::shared_ptr<Sound> launchSnd;

				SystemData* sys = mRoot ? mRoot->getSystem() : nullptr;
				if (sys != nullptr)
				{
					const std::shared_ptr<ThemeData>& theme = sys->getTheme();
					if (theme)
						launchSnd = NavigationSounds::getFromTheme(theme, "launch");
				}

				if (!launchSnd)
					launchSnd = Sound::getFromTheme(getTheme(), getName(), "launch");

				if (launchSnd)
					launchSnd->play();

				launch(cursor);
			}
			else
			{
				if (cursor->getChildren().size() > 0)
				{
					mCursorStack.push(cursor);
					populateList(cursor->getChildrenListToDisplay());
					setCursor(getCursor());
					updateThemeExtrasVisibility();
				}
			}

			updateThemeExtrasVisibility();
			return true;
		}
		else if (config->isMappedTo("b", input))
		{
			std::shared_ptr<Sound> backSnd;
			SystemData* sys = mRoot ? mRoot->getSystem() : nullptr;

			if (sys != nullptr)
			{
				const std::shared_ptr<ThemeData>& theme = sys->getTheme();
				if (theme)
					backSnd = NavigationSounds::getFromTheme(theme, "back");
			}

			if (!backSnd)
				backSnd = Sound::getFromTheme(getTheme(), getName(), "back");

			if (backSnd)
				backSnd->play();

			if (mCursorStack.size())
			{
				populateList(mCursorStack.top()->getParent()->getChildrenListToDisplay());
				setCursor(mCursorStack.top());
				mCursorStack.pop();
				updateThemeExtrasVisibility();
			}
			else
			{
				onFocusLost();

				FileData* cursor = getCursor();
				if (!cursor)
				{
					updateThemeExtrasVisibility();
					return true;
				}

				SystemData* systemToView = cursor->getSystem();
				if (systemToView->isCollection())
					systemToView = CollectionSystemManager::get()->getSystemToView(systemToView);

				ViewController::get()->goToSystemView(systemToView);
			}

			updateThemeExtrasVisibility();
			return true;
		}
		else if (config->isMappedTo("x", input))
		{
			if (mRoot->getSystem()->isGameSystem())
			{
				FileData* cursor = getCursor();
				FileData* randomGame = cursor ? cursor->getSystem()->getRandomGame() : nullptr;

				if (randomGame)
					setCursor(randomGame);

				updateThemeExtrasVisibility();
				return true;
			}
		}
		else if (config->isMappedTo("y", input) && !UIModeController::getInstance()->isUIModeKid())
		{
			if (mRoot->getSystem()->isGameSystem())
			{
				if (CollectionSystemManager::get()->toggleGameInCollection(getCursor()))
				{
					SystemData* sys = mRoot ? mRoot->getSystem() : nullptr;
					if (sys != nullptr)
					{
						const std::shared_ptr<ThemeData>& theme = sys->getTheme();
						if (theme)
						{
							auto favSnd = NavigationSounds::getFromTheme(theme, "favorite");
							if (!favSnd)
								favSnd = NavigationSounds::getFromTheme(theme, "select");
							if (favSnd)
								favSnd->play();
						}
					}

					updateThemeExtrasVisibility();
					return true;
				}
			}
		}
		else
		{
			// Quick system select
			if (Settings::getInstance()->getBool("QuickSystemSelect"))
			{
				bool nextSystem = false;
				bool prevSystem = false;

				if (horizontalGameList)
				{
					// En gamelist horizontal: L/R cambian sistemas
					nextSystem = config->isMappedLike("rightshoulder", input);
					prevSystem = config->isMappedLike("leftshoulder", input);
				}
				else
				{
					// Comportamiento normal
					nextSystem = config->isMappedLike(getQuickSystemSelectRightButton(), input);
					prevSystem = config->isMappedLike(getQuickSystemSelectLeftButton(), input);
				}

				if (nextSystem || prevSystem)
				{
					SystemData* sys = mRoot ? mRoot->getSystem() : nullptr;
					if (sys != nullptr)
					{
						const std::shared_ptr<ThemeData>& theme = sys->getTheme();
						if (theme)
						{
							auto snd = NavigationSounds::getFromTheme(theme, "quicksysselect");
							if (!snd)
								snd = NavigationSounds::getFromTheme(theme, "systembrowse");
							if (snd)
								snd->play();
						}
					}

					onFocusLost();

					if (nextSystem)
						ViewController::get()->goToNextGameList();
					else
						ViewController::get()->goToPrevGameList();

					updateThemeExtrasVisibility();
					return true;
				}
			}
		}
	}

	FileData* cursor = getCursor();
	SystemData* currentSystem = this->mRoot->getSystem();

	if (cursor && currentSystem != NULL)
	{
		Scripting::fireEvent("game-select", currentSystem->getName(), cursor->getPath(), cursor->getName(), "input");
	}
	else
	{
		Scripting::fireEvent("game-select", "NULL", "NULL", "NULL", "input");
	}

	bool handled = IGameListView::input(config, input);
	updateThemeExtrasVisibility();
	return handled;
}
