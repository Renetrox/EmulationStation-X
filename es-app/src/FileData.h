#pragma once
#ifndef ES_APP_FILE_DATA_H
#define ES_APP_FILE_DATA_H

#include "utils/FileSystemUtil.h"
#include "MetaData.h"

#include <string>
#include <unordered_map>
#include <vector>

class SystemData;
class Window;
struct SystemEnvironmentData;

enum FileType
{
	GAME = 1,   // Cannot have children.
	FOLDER = 2,
	PLACEHOLDER = 3
};

enum FileChangeType
{
	FILE_ADDED,
	FILE_METADATA_CHANGED,
	FILE_REMOVED,
	FILE_SORTED
};

// Used for loading/saving gamelist.xml.
const char* fileTypeToString(FileType type);
FileType stringToFileType(const char* str);

// A tree node that holds information for a file.
class FileData
{
public:
	enum class ArtSlot
	{
		Image,
		Thumbnail,
		Marquee,
		Grid,
		Carousel,
		VideoFallback
	};

	FileData(FileType type, const std::string& path, SystemEnvironmentData* envData, SystemData* system);
	virtual ~FileData();

	virtual const std::string& getName();
	virtual const std::string& getSortName();

	inline FileType getType() const { return mType; }
	inline const std::string& getPath() const { return mPath; }
	inline FileData* getParent() const { return mParent; }
	inline const std::unordered_map<std::string, FileData*>& getChildrenByFilename() const { return mChildrenByFilename; }
	inline const std::vector<FileData*>& getChildren() const { return mChildren; }
	inline SystemData* getSystem() const { return mSystem; }
	inline SystemEnvironmentData* getSystemEnvData() const { return mEnvData; }

	virtual const std::string getThumbnailPath() const;
	virtual const std::string getVideoPath() const;
	virtual const std::string getMarqueePath() const;
	virtual const std::string getImagePath() const;
	virtual const std::string getGridImagePath() const;

	// ES-X: imagen para carrusel fake de gamelist.
	// Sin parámetro mantiene compatibilidad con el comportamiento anterior.
	// Con parámetro permite que el tema use <carouselImageType>.
	virtual const std::string getCarouselImagePath() const;
	virtual const std::string getCarouselImagePath(const std::string& preferredType) const;
	// Preferred media first, without substituting a different type when missing.
	// Used only by opt-in carouselFallbackName; legacy art fallback is unchanged.
	virtual const std::string getCarouselPreferredImagePath(const std::string& preferredType) const;

	virtual const std::string getVideoFallbackPath() const;
	virtual const std::string getBackgroundPath() const;

	// ES-X extended media slots.
	virtual const std::string getCoverPath() const;
	virtual const std::string getScreenshotPath() const;
	virtual const std::string getWheelPath() const;
	virtual const std::string getTexturePath() const;
	virtual const std::string getFanartPath() const;

	const std::vector<FileData*>& getChildrenListToDisplay();
	std::vector<FileData*> getFilesRecursive(unsigned int typeMask, bool displayedOnly = false) const;

	void addChild(FileData* file); // Error if mType != FOLDER.
	void removeChild(FileData* file); // Error if mType != FOLDER.

	inline bool isPlaceHolder() { return mType == PLACEHOLDER; }

	virtual inline void refreshMetadata() { return; }

	virtual std::string getKey();
	const bool isArcadeAsset();

	inline std::string getFullPath() { return getPath(); }
	inline std::string getFileName() { return Utils::FileSystem::getFileName(getPath()); }

	virtual FileData* getSourceFileData();

	inline std::string getSystemName() const { return mSystemName; }

	// Returns our best guess at the "real" name for this file.
	// Will attempt to perform MAME name translation.
	std::string getDisplayName() const;

	// As above, but also remove parenthesis.
	std::string getCleanName() const;

	void launchGame(Window* window);

	typedef bool ComparisonFunction(const FileData* a, const FileData* b);

	struct SortType
	{
		ComparisonFunction* comparisonFunction;
		bool ascending;
		std::string description;

		SortType(ComparisonFunction* sortFunction, bool sortAscending, const std::string& sortDescription)
			: comparisonFunction(sortFunction),
			  ascending(sortAscending),
			  description(sortDescription)
		{
		}
	};

	void sort(const SortType& type);
	std::string getSortDescription() { return mSortDesc; }

	MetaDataList metadata;

protected:
	FileData* mSourceFileData;
	FileData* mParent;
	std::string mSystemName;

	// Art helpers.
	std::string getArtPathForSlot(ArtSlot slot) const;
	std::string getArtBySource(const std::string& source) const;
	std::vector<std::string> getFallbackOrderForSlot(ArtSlot slot, const std::string& preferred) const;

	std::string getLocalArt(const std::string& suffix) const;
	std::string getMetadataOrLocalArt(const std::string& key, const std::string& localSuffix) const;

	std::string getImageCandidate() const;
	std::string getThumbnailCandidate() const;
	std::string getMarqueeCandidate() const;
	std::string getCoverCandidate() const;
	std::string getBoxartCandidate() const;
	std::string getScreenshotCandidate() const;
	std::string getWheelCandidate() const;
	std::string getTextureCandidate() const;
	std::string getFanartCandidate() const;
	std::string getVideoCandidate() const;

	std::string getUniversalScreenshotCandidate() const;
	std::string getBackgroundCandidate() const;

private:
	void sort(ComparisonFunction& comparator, bool ascending = true);

	FileType mType;
	std::string mPath;
	SystemEnvironmentData* mEnvData;
	SystemData* mSystem;
	std::unordered_map<std::string, FileData*> mChildrenByFilename;
	std::vector<FileData*> mChildren;
	std::vector<FileData*> mFilteredChildren;
	std::string mSortDesc;
};

class CollectionFileData : public FileData
{
public:
	CollectionFileData(FileData* file, SystemData* system);
	~CollectionFileData();

	const std::string& getName();
	void refreshMetadata();
	FileData* getSourceFileData();
	std::string getKey();

private:
	// Needs to be updated when metadata changes.
	std::string mCollectionFileName;
	bool mDirty;
};

FileData::SortType getSortTypeFromString(std::string desc);

#endif // ES_APP_FILE_DATA_H
