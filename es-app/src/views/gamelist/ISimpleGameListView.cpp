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
#include <algorithm>
#include <cctype>

namespace {
std::string overlayKey(const std::string& name) {
	std::string key;
	for (unsigned char c : name)
		if (c < 128 && std::isalnum(c)) key += static_cast<char>(std::tolower(c));
	return key;
}

bool matchesOverlay(const std::string& title, const std::string& key) {
	if (key.empty() || key.find("auto") == 0) return false;
	if (key.size() < 4) return title == key;
	return title.find(key) != std::string::npos;
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
	mGameOverlays.clear();
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
	mGameOverlays.clear();
	mLastOverlayCursor = nullptr;

	mThemeExtras = ThemeData::makeExtrasWithMetadata(theme, getName(), mWindow);
	for (auto& extra : mThemeExtras)
	{
		if (extra.component)
			addChild(extra.component);
	}

	// Resolve opt-in extras once per theme load, not on every cursor change.
	for (const auto& extra : mThemeExtras) {
		const auto* element = theme ? theme->getElement(getName(), extra.name, "image") : nullptr;
		if (!element || !element->has("gameOverlayMatch") ||
		    !element->get<bool>("gameOverlayMatch") || !element->has("path"))
			continue;
		ImageComponent* image = dynamic_cast<ImageComponent*>(extra.component);
		if (!image) continue;
		GameOverlayEntry entry;
		entry.image = image;
		const std::string directory = Utils::FileSystem::getParent(element->get<std::string>("path"));
		for (const auto& file : Utils::FileSystem::getDirContent(directory)) {
			if (Utils::FileSystem::getExtension(file) != ".png") continue;
			const std::string key = overlayKey(Utils::FileSystem::getStem(file));
			if (!key.empty() && key.compare(0, 4, "auto") != 0)
				entry.files.emplace_back(key, file);
		}
		mGameOverlays.push_back(std::move(entry));
	}
	updateThemeExtrasVisibility();
	updateGameOverlays();

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


void ISimpleGameListView::updateGameOverlays()
{
	mLastOverlayCursor = getCursor();
	if (mGameOverlays.empty()) return;
	const FileData* file = mLastOverlayCursor;
	const std::string title = (file && !file->isPlaceHolder())
		? overlayKey(file->getName()) : "";
	const std::string rom = (file && !file->isPlaceHolder())
		? overlayKey(Utils::FileSystem::getStem(file->getPath())) : "";
	for (auto& entry : mGameOverlays) {
		std::string best;
		size_t bestLength = 0;
		for (const auto& candidate : entry.files) {
			if ((matchesOverlay(title, candidate.first) || matchesOverlay(rom, candidate.first))
			    && candidate.first.size() > bestLength) {
				bestLength = candidate.first.size();
				best = candidate.second;
			}
		}
		entry.image->setImage(best);
		entry.image->setVisible(!best.empty());
	}
}

void ISimpleGameListView::update(int deltaTime)
{
	IGameListView::update(deltaTime);
	if (getCursor() != mLastOverlayCursor)
		updateGameOverlays();
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

	for (auto& extra : mThemeExtras)
	{
		if (!extra.component)
			continue;

		extra.component->setVisible(!(single && extra.hideWhenSingleGame));
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
