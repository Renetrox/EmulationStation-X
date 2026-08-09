#include "FileData.h"

#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "utils/TimeUtil.h"
#include "AudioManager.h"
#include "CollectionSystemManager.h"
#include "FileFilterIndex.h"
#include "FileSorts.h"
#include "InputManager.h"
#include "Log.h"
#include "MameNames.h"
#include "platform.h"
#include "Scripting.h"
#include "Settings.h"
#include "SystemData.h"
#include "VolumeControl.h"
#include "Window.h"

#include <algorithm>
#include <assert.h>

static inline std::string getNormalizedSourceSetting(const std::string& settingName, const std::string& defaultValue = "auto")
{
	std::string value = Settings::getInstance()->getString(settingName);
	if (value.empty())
		value = defaultValue;

	value = Utils::String::toLower(value);

	if (value != "auto" &&
		value != "image" &&
		value != "thumbnail" &&
		value != "marquee" &&
		value != "cover" &&
		value != "boxart" &&
		value != "screenshot" &&
		value != "wheel" &&
		value != "texture" &&
		value != "fanart" &&
		value != "none")
	{
		value = defaultValue;
	}

	return value;
}

FileData::FileData(FileType type, const std::string& path, SystemEnvironmentData* envData, SystemData* system)
	: mType(type), mPath(path), mEnvData(envData), mSystem(system), mSourceFileData(NULL), mParent(NULL),
	  metadata(type == GAME ? GAME_METADATA : FOLDER_METADATA)
{
	// metadata needs at least a name field (since that's what getName() will return)
	if (metadata.get("name").empty())
		metadata.set("name", getDisplayName());

	mSystemName = system->getName();
	metadata.resetChangedFlag();
}

FileData::~FileData()
{
	if (mParent)
		mParent->removeChild(this);

	if (mType == GAME)
		mSystem->getIndex()->removeFromIndex(this);

	mChildren.clear();
}

std::string FileData::getDisplayName() const
{
	std::string stem = Utils::FileSystem::getStem(mPath);

	if (mSystem && (mSystem->hasPlatformId(PlatformIds::ARCADE) || mSystem->hasPlatformId(PlatformIds::NEOGEO)))
		stem = MameNames::getInstance()->getRealName(stem);

	return stem;
}

std::string FileData::getCleanName() const
{
	return Utils::String::removeParenthesis(this->getDisplayName());
}

const std::string& FileData::getName()
{
	return metadata.get("name");
}

const std::string& FileData::getSortName()
{
	if (metadata.get("sortname").empty())
		return metadata.get("name");
	else
		return metadata.get("sortname");
}

std::string FileData::getLocalArt(const std::string& suffix) const
{
	if (!Settings::getInstance()->getBool("LocalArt"))
		return "";

	const char* extList[2] = { ".png", ".jpg" };

	for (int i = 0; i < 2; i++)
	{
		std::string path = mEnvData->mStartPath + "/images/" + getDisplayName() + suffix + extList[i];
		if (Utils::FileSystem::exists(path))
			return path;
	}

	return "";
}

std::string FileData::getMetadataOrLocalArt(const std::string& key, const std::string& localSuffix) const
{
	std::string value = metadata.get(key);
	if (!value.empty())
		return value;

	return getLocalArt(localSuffix);
}

std::string FileData::getImageCandidate() const
{
	return getMetadataOrLocalArt("image", "-image");
}

std::string FileData::getThumbnailCandidate() const
{
	std::string thumbnail = metadata.get("thumbnail");

	// fallback legacy: thumbnail -> image -> local image
	if (thumbnail.empty())
	{
		thumbnail = metadata.get("image");

		if (thumbnail.empty() && Settings::getInstance()->getBool("LocalArt"))
			thumbnail = getLocalArt("-image");
	}

	return thumbnail;
}

std::string FileData::getMarqueeCandidate() const
{
	return getMetadataOrLocalArt("marquee", "-marquee");
}

std::string FileData::getCoverCandidate() const
{
	return getMetadataOrLocalArt("cover", "-cover");
}

std::string FileData::getBoxartCandidate() const
{
	return getMetadataOrLocalArt("boxart", "-boxart");
}

std::string FileData::getScreenshotCandidate() const
{
	return getMetadataOrLocalArt("screenshot", "-screenshot");
}

std::string FileData::getWheelCandidate() const
{
	return getMetadataOrLocalArt("wheel", "-wheel");
}

std::string FileData::getTextureCandidate() const
{
	return getMetadataOrLocalArt("texture", "-texture");
}

std::string FileData::getFanartCandidate() const
{
	std::string fanart = metadata.get("fanart");
	if (!fanart.empty())
		return fanart;

	if (mType != GAME)
		return "";

	const std::string romDir = Utils::FileSystem::getParent(mPath);
	if (romDir.empty())
		return "";

	const std::string fanartDir = romDir + "/media/fanart";
	const std::string romStem = Utils::FileSystem::getStem(mPath);

	const char* extList[3] = { ".png", ".jpg", ".jpeg" };

	for (int i = 0; i < 3; i++)
	{
		const std::string candidate = fanartDir + "/" + romStem + extList[i];
		if (Utils::FileSystem::exists(candidate))
			return candidate;
	}

	return "";
}

std::string FileData::getVideoCandidate() const
{
	std::string video = metadata.get("video");

	// legacy local video support
	if (video.empty() && Settings::getInstance()->getBool("LocalArt"))
	{
		std::string path = mEnvData->mStartPath + "/images/" + getDisplayName() + "-video.mp4";
		if (Utils::FileSystem::exists(path))
			video = path;
	}

	return video;
}

std::string FileData::getUniversalScreenshotCandidate() const
{
	if (mType != GAME)
		return "";

	const std::string romDir = Utils::FileSystem::getParent(mPath);
	if (romDir.empty())
		return "";

	const std::string screenshotsDir = romDir + "/media/screenshots";
	const std::string romStem = Utils::FileSystem::getStem(mPath);

	const char* extList[3] = { ".png", ".jpg", ".jpeg" };

	for (int i = 0; i < 3; i++)
	{
		const std::string candidate = screenshotsDir + "/" + romStem + extList[i];
		if (Utils::FileSystem::exists(candidate))
			return candidate;
	}

	return "";
}

std::string FileData::getBackgroundCandidate() const
{
	std::string fanart = getFanartCandidate();
	if (!fanart.empty())
		return fanart;

	std::string screenshot = getScreenshotCandidate();
	if (!screenshot.empty())
		return screenshot;

	return getUniversalScreenshotCandidate();
}

std::string FileData::getArtBySource(const std::string& source) const
{
	if (source == "image")
		return getImageCandidate();
	else if (source == "thumbnail")
		return getThumbnailCandidate();
	else if (source == "marquee")
		return getMarqueeCandidate();
	else if (source == "cover")
	{
		std::string cover = getCoverCandidate();
		if (!cover.empty())
			return cover;

		return getBoxartCandidate();
	}
	else if (source == "boxart")
	{
		std::string boxart = getBoxartCandidate();
		if (!boxart.empty())
			return boxart;

		return getCoverCandidate();
	}
	else if (source == "screenshot")
	{
		std::string screenshot = getScreenshotCandidate();
		if (!screenshot.empty())
			return screenshot;

		return getUniversalScreenshotCandidate();
	}
	else if (source == "wheel")
	{
		std::string wheel = getWheelCandidate();
		if (!wheel.empty())
			return wheel;

		return getMarqueeCandidate();
	}
	else if (source == "texture")
		return getTextureCandidate();
	else if (source == "fanart")
		return getFanartCandidate();
	else if (source == "none")
		return "";

	return "";
}

std::vector<std::string> FileData::getFallbackOrderForSlot(ArtSlot slot, const std::string& preferred) const
{
	std::vector<std::string> order;

	auto appendUnique = [&order](const std::string& value)
	{
		if (value.empty())
			return;

		if (std::find(order.begin(), order.end(), value) == order.end())
			order.push_back(value);
	};

	if (preferred == "none")
	{
		appendUnique("none");
		return order;
	}

	switch (slot)
	{
	case ArtSlot::Image:
		appendUnique(preferred);
		appendUnique("image");
		appendUnique("thumbnail");
		appendUnique("cover");
		appendUnique("boxart");
		appendUnique("screenshot");
		appendUnique("texture");
		appendUnique("marquee");
		appendUnique("wheel");
		break;

	case ArtSlot::Thumbnail:
		appendUnique(preferred);
		appendUnique("thumbnail");
		appendUnique("image");
		appendUnique("cover");
		appendUnique("boxart");
		appendUnique("screenshot");
		appendUnique("marquee");
		appendUnique("wheel");
		appendUnique("texture");
		break;

	case ArtSlot::Marquee:
		if (preferred == "auto")
		{
			appendUnique("marquee");
			appendUnique("wheel");
		}
		else
		{
			appendUnique(preferred);
			appendUnique("marquee");
			appendUnique("wheel");
		}
		break;

		case ArtSlot::Grid:
		appendUnique(preferred);
		appendUnique("image");
		appendUnique("thumbnail");
		appendUnique("cover");
		appendUnique("boxart");
		appendUnique("screenshot");
		appendUnique("texture");
		appendUnique("marquee");
		appendUnique("wheel");
		break;

		case ArtSlot::Carousel:
		// ES-X:
		// Carrusel fake de gamelist. Por defecto prioriza capturas/gameplay,
		// porque visualmente funciona mejor como fila tipo consola.
		appendUnique(preferred);
		appendUnique("screenshot");
		appendUnique("thumbnail");
		appendUnique("image");
		appendUnique("boxart");
		appendUnique("cover");
		appendUnique("marquee");
		appendUnique("wheel");
		appendUnique("texture");
		appendUnique("fanart");
		break;;

	case ArtSlot::VideoFallback:
		appendUnique(preferred);
		appendUnique("image");
		appendUnique("thumbnail");
		appendUnique("cover");
		appendUnique("boxart");
		appendUnique("screenshot");
		appendUnique("texture");
		appendUnique("marquee");
		appendUnique("wheel");
		break;
	}

	return order;
}

std::string FileData::getArtPathForSlot(ArtSlot slot) const
{
	std::string preferred = "auto";

	switch (slot)
	{
	case ArtSlot::Image:
		preferred = getNormalizedSourceSetting("GameImageSource", "auto");
		break;

	case ArtSlot::Thumbnail:
		preferred = getNormalizedSourceSetting("GameThumbnailSource", "auto");
		break;

	case ArtSlot::Marquee:
		preferred = getNormalizedSourceSetting("GameMarqueeSource", "auto");
		break;

	case ArtSlot::Grid:
		preferred = getNormalizedSourceSetting("GridImageSource", "auto");
		break;

	case ArtSlot::Carousel:
		preferred = "auto";
		break;

	case ArtSlot::VideoFallback:
		preferred = getNormalizedSourceSetting("VideoFallbackSource", "auto");
		break;
	}

	std::vector<std::string> order = getFallbackOrderForSlot(slot, preferred);

	for (auto it = order.cbegin(); it != order.cend(); ++it)
	{
		if (*it == "auto")
			continue;

		std::string out = getArtBySource(*it);
		if (!out.empty())
			return out;
	}

	return "";
}

const std::string FileData::getThumbnailPath() const
{
	return getArtPathForSlot(ArtSlot::Thumbnail);
}

const std::string FileData::getVideoPath() const
{
	return getVideoCandidate();
}

const std::string FileData::getMarqueePath() const
{
	return getArtPathForSlot(ArtSlot::Marquee);
}

const std::string FileData::getImagePath() const
{
	return getArtPathForSlot(ArtSlot::Image);
}

const std::string FileData::getGridImagePath() const
{
	return getArtPathForSlot(ArtSlot::Grid);
}

const std::string FileData::getCarouselImagePath() const
{
	return getCarouselImagePath("auto");
}

const std::string FileData::getCarouselImagePath(const std::string& preferredType) const
{
	std::string preferred = Utils::String::toLower(preferredType);

	if (preferred.empty())
		preferred = "auto";

	if (preferred == "cover")
		preferred = "boxart";

	if (preferred != "auto" &&
		preferred != "image" &&
		preferred != "thumbnail" &&
		preferred != "marquee" &&
		preferred != "boxart" &&
		preferred != "screenshot" &&
		preferred != "wheel" &&
		preferred != "texture" &&
		preferred != "fanart" &&
		preferred != "none")
	{
		LOG(LogWarning) << "Unknown carousel image source \"" << preferred << "\". Using auto.";
		preferred = "auto";
	}

	std::vector<std::string> order = getFallbackOrderForSlot(ArtSlot::Carousel, preferred);

	for (auto it = order.cbegin(); it != order.cend(); ++it)
	{
		if (*it == "auto")
			continue;

		std::string out = getArtBySource(*it);
		if (!out.empty())
			return out;
	}

	return "";
}

const std::string FileData::getVideoFallbackPath() const
{
	return getArtPathForSlot(ArtSlot::VideoFallback);
}

const std::string FileData::getBackgroundPath() const
{
	return getBackgroundCandidate();
}

const std::string FileData::getCoverPath() const
{
	return getCoverCandidate();
}

const std::string FileData::getScreenshotPath() const
{
	return getScreenshotCandidate();
}

const std::string FileData::getWheelPath() const
{
	return getWheelCandidate();
}

const std::string FileData::getTexturePath() const
{
	return getTextureCandidate();
}

const std::string FileData::getFanartPath() const
{
	return getFanartCandidate();
}

const std::vector<FileData*>& FileData::getChildrenListToDisplay()
{
	FileFilterIndex* idx = CollectionSystemManager::get()->getSystemToView(mSystem)->getIndex();

	if (idx->isFiltered())
	{
		mFilteredChildren.clear();

		for (auto it = mChildren.cbegin(); it != mChildren.cend(); it++)
		{
			if (idx->showFile((*it)))
				mFilteredChildren.push_back(*it);
		}

		return mFilteredChildren;
	}
	else
	{
		return mChildren;
	}
}

std::vector<FileData*> FileData::getFilesRecursive(unsigned int typeMask, bool displayedOnly) const
{
	std::vector<FileData*> out;
	FileFilterIndex* idx = mSystem->getIndex();

	for (auto it = mChildren.cbegin(); it != mChildren.cend(); it++)
	{
		if ((*it)->getType() & typeMask)
		{
			if (!displayedOnly || !idx->isFiltered() || idx->showFile(*it))
				out.push_back(*it);
		}

		if ((*it)->getChildren().size() > 0)
		{
			std::vector<FileData*> subchildren = (*it)->getFilesRecursive(typeMask, displayedOnly);
			out.insert(out.cend(), subchildren.cbegin(), subchildren.cend());
		}
	}

	return out;
}

std::string FileData::getKey()
{
	return getFileName();
}

const bool FileData::isArcadeAsset()
{
	const std::string stem = Utils::FileSystem::getStem(mPath);

	return (
		(mSystem && (mSystem->hasPlatformId(PlatformIds::ARCADE) || mSystem->hasPlatformId(PlatformIds::NEOGEO))) &&
		(MameNames::getInstance()->isBios(stem) || MameNames::getInstance()->isDevice(stem))
	);
}

FileData* FileData::getSourceFileData()
{
	return this;
}

void FileData::addChild(FileData* file)
{
	assert(mType == FOLDER);
	assert(file->getParent() == NULL);

	const std::string key = file->getKey();
	if (mChildrenByFilename.find(key) == mChildrenByFilename.cend())
	{
		mChildrenByFilename[key] = file;
		mChildren.push_back(file);
		file->mParent = this;
	}
}

void FileData::removeChild(FileData* file)
{
	assert(mType == FOLDER);
	assert(file->getParent() == this);

	mChildrenByFilename.erase(file->getKey());

	for (auto it = mChildren.cbegin(); it != mChildren.cend(); it++)
	{
		if (*it == file)
		{
			file->mParent = NULL;
			mChildren.erase(it);
			return;
		}
	}

	// File somehow wasn't in our children.
	assert(false);
}

void FileData::sort(ComparisonFunction& comparator, bool ascending)
{
	if (ascending)
	{
		std::stable_sort(mChildren.begin(), mChildren.end(), comparator);

		for (auto it = mChildren.cbegin(); it != mChildren.cend(); it++)
		{
			if ((*it)->getChildren().size() > 0)
				(*it)->sort(comparator, ascending);
		}
	}
	else
	{
		std::stable_sort(mChildren.rbegin(), mChildren.rend(), comparator);

		for (auto it = mChildren.rbegin(); it != mChildren.rend(); it++)
		{
			if ((*it)->getChildren().size() > 0)
				(*it)->sort(comparator, ascending);
		}
	}
}

void FileData::sort(const SortType& type)
{
	sort(*type.comparisonFunction, type.ascending);
	mSortDesc = type.description;
}

void FileData::launchGame(Window* window)
{
	LOG(LogInfo) << "Attempting to launch game...";

	AudioManager::getInstance()->deinit();
	VolumeControl::getInstance()->deinit();
	InputManager::getInstance()->deinit();
	window->deinit();

	std::string command = mEnvData->mLaunchCommand;

	const std::string rom      = Utils::FileSystem::getEscapedPath(getPath());
	const std::string basename = Utils::FileSystem::getStem(getPath());
	const std::string rom_raw  = Utils::FileSystem::getPreferredPath(getPath());
	const std::string name     = getName();

	command = Utils::String::replace(command, "%ROM%", rom);
	command = Utils::String::replace(command, "%BASENAME%", basename);
	command = Utils::String::replace(command, "%ROM_RAW%", rom_raw);

	Scripting::fireEvent("game-start", rom, basename, name);

	LOG(LogInfo) << "\t" << command;
	int exitCode = runSystemCommand(command);

	if (exitCode != 0)
		LOG(LogWarning) << "...launch terminated with nonzero exit code " << exitCode << "!";

	Scripting::fireEvent("game-end");

	window->init();
	InputManager::getInstance()->init();
	VolumeControl::getInstance()->init();
	window->normalizeNextUpdate();

	// update number of times the game has been launched
	FileData* gameToUpdate = getSourceFileData();

	int timesPlayed = gameToUpdate->metadata.getInt("playcount") + 1;
	gameToUpdate->metadata.set("playcount", std::to_string(static_cast<long long>(timesPlayed)));

	// update last played time
	gameToUpdate->metadata.set("lastplayed", Utils::Time::DateTime(Utils::Time::now()));
	CollectionSystemManager::get()->refreshCollectionSystems(gameToUpdate);

	gameToUpdate->mSystem->onMetaDataSavePoint();
}

CollectionFileData::CollectionFileData(FileData* file, SystemData* system)
	: FileData(file->getSourceFileData()->getType(), file->getSourceFileData()->getPath(),
		file->getSourceFileData()->getSystemEnvData(), system)
{
	// we use this constructor to create a clone of the filedata, and change its system
	mSourceFileData = file->getSourceFileData();
	refreshMetadata();
	mParent = NULL;
	metadata = mSourceFileData->metadata;
	mSystemName = mSourceFileData->getSystem()->getName();
}

CollectionFileData::~CollectionFileData()
{
	// need to remove collection file data at the collection object destructor
	if (mParent)
		mParent->removeChild(this);

	mParent = NULL;
}

std::string CollectionFileData::getKey()
{
	return getFullPath();
}

FileData* CollectionFileData::getSourceFileData()
{
	return mSourceFileData;
}

void CollectionFileData::refreshMetadata()
{
	metadata = mSourceFileData->metadata;
	mDirty = true;
}

const std::string& CollectionFileData::getName()
{
	if (mDirty)
	{
		mCollectionFileName = Utils::String::removeParenthesis(mSourceFileData->metadata.get("name"));
		mCollectionFileName += " [" + Utils::String::toUpper(mSourceFileData->getSystem()->getName()) + "]";
		mDirty = false;
	}

	if (Settings::getInstance()->getBool("CollectionShowSystemInfo"))
		return mCollectionFileName;

	return mSourceFileData->metadata.get("name");
}

// returns Sort Type based on a string description
FileData::SortType getSortTypeFromString(std::string desc)
{
	std::vector<FileData::SortType> SortTypes = FileSorts::SortTypes;

	// find it
	for (unsigned int i = 0; i < FileSorts::SortTypes.size(); i++)
	{
		const FileData::SortType& sort = FileSorts::SortTypes.at(i);
		if (sort.description == desc)
			return sort;
	}

	// if not found default to "name, ascending"
	return FileSorts::SortTypes.at(0);
}
