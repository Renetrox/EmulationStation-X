#include "Gamelist.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sys/stat.h>

#include "utils/FileSystemUtil.h"
#include "FileData.h"
#include "FileFilterIndex.h"
#include "Log.h"
#include "Settings.h"
#include "SystemData.h"
#include <pugixml.hpp>

FileData* findOrCreateFile(SystemData* system, const std::string& path, FileType type)
{
	FileData* root = system->getRootFolder();
	bool contains = false;
	const std::string systemPath = root->getPath();

	// first, verify that path is within the system's root folder
	std::string relative = Utils::FileSystem::removeCommonPath(path, systemPath, contains, true);
	if(!contains)
	{
		LOG(LogError) << "File path \"" << path << "\" is outside system path \"" << system->getStartPath() << "\"";
		return NULL;
	}

	Utils::FileSystem::stringList pathList = Utils::FileSystem::getPathList(relative);

	auto path_it = pathList.begin();
	FileData* treeNode = root;
	bool found = false;

	// iterate over all subpaths below the provided path
	while(path_it != pathList.end())
	{
		const std::unordered_map<std::string, FileData*>& children = treeNode->getChildrenByFilename();

		std::string pathSegment = *path_it;
		auto candidate = children.find(pathSegment);
		found = candidate != children.cend();
		if (found) {
			treeNode = candidate->second;
		}

		// this is the end
		if(path_it == --pathList.end())
		{
			if(found)
				return treeNode;

			// --- FIX: allow creating <folder> entries from gamelist if they exist on filesystem ---
			// ES-dev historically refused to create folders coming from gamelist.xml if they were
			// not already present in the in-memory filesystem tree.
			//
			// That causes "Error finding/creating FileData..." for folders that exist on disk but
			// were not created during scanning (e.g. filtered/hidden earlier).
			//
			// Safe rule:
			// - If type == FOLDER and the folder exists on disk, create it and attach it.
			// - If it does not exist, keep old behavior (warn + return NULL).
			if(type == FOLDER)
			{
				// Only create if it actually exists in the filesystem
				if(Utils::FileSystem::exists(path))
				{
					// Ensure parent is a folder
					if (treeNode->getType() != FOLDER)
					{
						LOG(LogWarning) << "gameList: cannot add folder '" << path
							<< "' because parent is not a folder (parent path: " << treeNode->getPath() << ")";
						return NULL;
					}

					FileData* folder = new FileData(FOLDER, path, system->getSystemEnvData(), system);
					treeNode->addChild(folder);
					return folder;
				}

				LOG(LogWarning) << "gameList: folder doesn't exist on filesystem, won't create: " << path;
				return NULL;
			}

			FileData* file = new FileData(type, path, system->getSystemEnvData(), system);

			// skipping arcade assets from gamelist and add only to filesystem
			// (fs) folders, i.e. entriess in gamelist with <folder/> and not to
			// fs-folders which are marked as <game/> in gamelist. NB:
			// treeNode's type (=parent) is determined by the element in the
			// gamelist and not by the fs-type.
			if(!file->isArcadeAsset() && treeNode->getType() == FOLDER)
			{
				treeNode->addChild(file);
			}
			return file;
		}

		if(!found)
		{
			// don't create folders unless it's leading up to a game
			// if type is a folder it's gonna be empty, so don't bother
			if(type == FOLDER)
			{
				std::string absFolder = Utils::FileSystem::getAbsolutePath(pathSegment, systemPath);
				LOG(LogWarning) << "gameList: folder " << absFolder << " absent on fs, no FileData object created. Do remove leftover in gamelist.xml to remediate this warning.";
				return NULL;
			}
			// discard constellations like scummvm/game.svm/game.svm as
			// scummvm/game.svm/ is a GAME and not a FOLDER
			if (treeNode->getType() == GAME)
			{
				std::string absFolder = Utils::FileSystem::getAbsolutePath(pathSegment, systemPath);
				LOG(LogWarning) << "gameList: trying to add game '" << absFolder << "' to a parent <game/> entry is invalid, no FileData object created. Do remove nested <game/> in gamelist.xml to remediate this warning.";
				return NULL;
			}
			// create folder filedata object
			std::string absPath = Utils::FileSystem::resolveRelativePath(treeNode->getPath() + "/" + pathSegment, systemPath, false, true);
			FileData* folder = new FileData(FOLDER, absPath, system->getSystemEnvData(), system);
			LOG(LogDebug) << "folder not found as FileData, adding: " << folder->getPath();

			treeNode->addChild(folder);
			treeNode = folder;
		}

		path_it++;
	}

	return NULL;
}


namespace
{
	using ExplicitMetadataMap = std::map<std::string, std::set<std::string>>;
	using ImportedMetadataMap = std::map<std::string, std::map<std::string, unsigned long long>>;
	using GameInfoSourceMap = std::map<std::string, std::set<std::string>>;

	struct GameInfoSignature
	{
		long long size = 0;
		long long mtime = 0;
	};

	struct PendingGameInfo
	{
		std::string path;
		GameInfoSignature signature;
	};

	using GameInfoCache = std::map<std::string, GameInfoSignature>;

	struct GameInfoCacheData
	{
		GameInfoCache files;
		ImportedMetadataMap importedMetadata;
		GameInfoSourceMap sourceGames;
		std::set<std::string> launchers;
		bool hasLauncherSnapshot = false;
		bool hasImportedMetadataSnapshot = false;
		bool hasSourceGameSnapshot = false;
	};

	bool sameSignature(const GameInfoSignature& left, const GameInfoSignature& right)
	{
		return left.size == right.size && left.mtime == right.mtime;
	}

	unsigned long long metadataValueHash(const std::string& value)
	{
		// Stable FNV-1a hash used only to recognize values previously imported by
		// this code. It lets user-edited gamelist fields remain authoritative.
		unsigned long long hash = 1469598103934665603ULL;
		for(unsigned char byte : value)
		{
			hash ^= byte;
			hash *= 1099511628211ULL;
		}
		return hash;
	}

	bool getGameInfoSignature(const std::string& path, GameInfoSignature& signature)
	{
#ifdef WIN32
		struct _stat64 info;
		if(_stat64(path.c_str(), &info) != 0)
			return false;
#else
		struct stat info;
		if(stat(path.c_str(), &info) != 0)
			return false;
#endif

		signature.size = static_cast<long long>(info.st_size);
		signature.mtime = static_cast<long long>(info.st_mtime);
		return true;
	}

	std::string getPortMasterGameInfoCachePath()
	{
		return Utils::FileSystem::getHomePath() +
			"/.emulationstation/cache/portmaster-gameinfo.cache";
	}

	GameInfoCacheData loadPortMasterGameInfoCache()
	{
		GameInfoCacheData cache;
		std::ifstream stream(getPortMasterGameInfoCachePath().c_str());
		std::string line;

		while(std::getline(stream, line))
		{
			if(line == "# ES-X PortMaster gameinfo cache v4")
			{
				cache.hasImportedMetadataSnapshot = true;
				cache.hasSourceGameSnapshot = true;
				continue;
			}

			if(line == "# ES-X PortMaster gameinfo cache v3")
			{
				cache.hasImportedMetadataSnapshot = true;
				continue;
			}

			if(line.empty() || line[0] == '#')
				continue;

			// v2: launcher snapshot, used for a zero-scan fast path.
			if(line.compare(0, 2, "L\t") == 0)
			{
				cache.launchers.insert(line.substr(2));
				cache.hasLauncherSnapshot = true;
				continue;
			}

			// v2: cached gameinfo.xml signature.
			if(line.compare(0, 2, "G\t") == 0)
			{
				const size_t firstTab = line.find('\t', 2);
				const size_t secondTab = firstTab == std::string::npos ?
					std::string::npos : line.find('\t', firstTab + 1);

				if(firstTab == std::string::npos || secondTab == std::string::npos)
					continue;

				GameInfoSignature signature;
				signature.mtime = std::atoll(line.substr(2, firstTab - 2).c_str());
				signature.size = std::atoll(line.substr(firstTab + 1, secondTab - firstTab - 1).c_str());
				cache.files[line.substr(secondTab + 1)] = signature;
				continue;
			}

			// v3: hash of a metadata value last imported from PortMaster.
			if(line.compare(0, 2, "M\t") == 0)
			{
				const size_t firstTab = line.find('\t', 2);
				const size_t secondTab = firstTab == std::string::npos ?
					std::string::npos : line.find('\t', firstTab + 1);

				if(firstTab == std::string::npos || secondTab == std::string::npos)
					continue;

				const unsigned long long hash = std::strtoull(
					line.substr(2, firstTab - 2).c_str(), NULL, 10);
				const std::string path = line.substr(firstTab + 1, secondTab - firstTab - 1);
				const std::string key = line.substr(secondTab + 1);
				if(!path.empty() && !key.empty())
					cache.importedMetadata[path][key] = hash;
				continue;
			}

			// v4: game paths associated with each gameinfo.xml source. This lets a
			// changed metadata file withdraw an entire <game> record cleanly.
			if(line.compare(0, 2, "S\t") == 0)
			{
				const size_t tab = line.find('\t', 2);
				if(tab == std::string::npos)
					continue;

				const std::string source = line.substr(2, tab - 2);
				const std::string path = line.substr(tab + 1);
				if(!source.empty() && !path.empty())
					cache.sourceGames[source].insert(path);
				continue;
			}

			// Backward compatibility with the v1 cache format:
			// mtime<TAB>size<TAB>path
			const size_t firstTab = line.find('\t');
			const size_t secondTab = firstTab == std::string::npos ?
				std::string::npos : line.find('\t', firstTab + 1);

			if(firstTab == std::string::npos || secondTab == std::string::npos)
				continue;

			GameInfoSignature signature;
			signature.mtime = std::atoll(line.substr(0, firstTab).c_str());
			signature.size = std::atoll(line.substr(firstTab + 1, secondTab - firstTab - 1).c_str());
			cache.files[line.substr(secondTab + 1)] = signature;
		}

		return cache;
	}

	void savePortMasterGameInfoCache(const GameInfoCacheData& cache)
	{
		const std::string cachePath = getPortMasterGameInfoCachePath();
		Utils::FileSystem::createDirectory(Utils::FileSystem::getParent(cachePath));

		std::ofstream stream(cachePath.c_str(), std::ios::out | std::ios::trunc);
		if(!stream)
		{
			LOG(LogWarning) << "PortMaster gameinfo: unable to write cache \"" << cachePath << "\"";
			return;
		}

		stream << "# ES-X PortMaster gameinfo cache v4\n";
		for(const std::string& launcher : cache.launchers)
			stream << "L\t" << launcher << '\n';

		for(const auto& entry : cache.files)
			stream << "G\t" << entry.second.mtime << '\t' << entry.second.size << '\t'
				<< entry.first << '\n';

		for(const auto& fileEntry : cache.importedMetadata)
		{
			for(const auto& fieldEntry : fileEntry.second)
				stream << "M\t" << fieldEntry.second << '\t' << fileEntry.first << '\t'
					<< fieldEntry.first << '\n';
		}

		for(const auto& sourceEntry : cache.sourceGames)
		{
			for(const std::string& path : sourceEntry.second)
				stream << "S\t" << sourceEntry.first << '\t' << path << '\n';
		}
	}

	std::set<std::string> getCurrentPortLaunchers(SystemData* system)
	{
		std::set<std::string> launchers;
		FileData* root = system->getRootFolder();
		if(!root)
			return launchers;

		const std::vector<FileData*>& children = root->getChildren();
		for(FileData* child : children)
		{
			if(child->getType() != GAME)
				continue;

			if(Utils::FileSystem::getExtension(child->getPath()) == ".sh")
				launchers.insert(child->getPath());
		}

		return launchers;
	}

	const char* PORTMASTER_METADATA_KEYS[] = {
		"name",
		"desc",
		"image",
		"releasedate",
		"developer",
		"publisher",
		"genre",
		"players"
	};

	ExplicitMetadataMap getExplicitGamelistMetadata(SystemData* system,
		const ImportedMetadataMap& importedMetadata)
	{
		ExplicitMetadataMap explicitMetadata;
		const std::string xmlpath = system->getGamelistPath(false);

		if(!Utils::FileSystem::exists(xmlpath))
			return explicitMetadata;

		pugi::xml_document doc;
		if(!doc.load_file(xmlpath.c_str()))
			return explicitMetadata;

		pugi::xml_node root = doc.child("gameList");
		if(!root)
			return explicitMetadata;

		const std::string relativeTo = system->getStartPath();

		for(pugi::xml_node fileNode = root.child("game"); fileNode; fileNode = fileNode.next_sibling("game"))
		{
			pugi::xml_node pathNode = fileNode.child("path");
			if(!pathNode)
				continue;

			const std::string path =
				Utils::FileSystem::resolveRelativePath(pathNode.text().get(), relativeTo, false, true);

			MetaDataList current = MetaDataList::createFromXML(GAME_METADATA, fileNode, relativeTo);
			for(const char* key : PORTMASTER_METADATA_KEYS)
			{
				if(!fileNode.child(key))
					continue;

				auto importedFile = importedMetadata.find(path);
				if(importedFile != importedMetadata.end())
				{
					auto importedField = importedFile->second.find(key);
					if(importedField != importedFile->second.end() &&
						importedField->second == metadataValueHash(current.get(key)))
						continue;
				}

				explicitMetadata[path].insert(key);
			}
		}

		return explicitMetadata;
	}

	std::string getDefaultMetadataValue(const MetaDataList& metadata, const std::string& key)
	{
		const std::vector<MetaDataDecl>& declarations = getMDDByType(metadata.getType());
		for(const MetaDataDecl& declaration : declarations)
		{
			if(declaration.key == key)
				return declaration.defaultValue;
		}

		return std::string();
	}

	bool isDefaultMetadataValue(const MetaDataList& metadata, const std::string& key)
	{
		const std::string& value = metadata.get(key);
		if(value.empty())
			return true;

		return value == getDefaultMetadataValue(metadata, key);
	}

	bool isImportedMetadataValue(FileData* file,
		const ImportedMetadataMap& importedMetadata,
		const std::string& key)
	{
		auto importedFile = importedMetadata.find(file->getPath());
		if(importedFile == importedMetadata.end())
			return false;

		auto importedField = importedFile->second.find(key);
		return importedField != importedFile->second.end() &&
			importedField->second == metadataValueHash(file->metadata.get(key));
	}

	FileData* findExistingPortGame(SystemData* system, const std::string& path)
	{
		FileData* root = system->getRootFolder();
		if(!root)
			return NULL;

		for(FileData* child : root->getChildren())
		{
			if(child->getType() == GAME && child->getPath() == path)
				return child;
		}

		return NULL;
	}

	bool shouldUseSupplementalMetadata(FileData* file,
		const ExplicitMetadataMap& explicitMetadata,
		const std::string& key)
	{
		auto fileEntry = explicitMetadata.find(file->getPath());
		if(fileEntry != explicitMetadata.end() && fileEntry->second.count(key) != 0)
			return false;

		if(key == "name")
			return file->metadata.get("name").empty() ||
				file->metadata.get("name") == file->getDisplayName();

		return isDefaultMetadataValue(file->metadata, key);
	}
}

static bool updateGamelistInternal(SystemData* system);

void parsePortMasterGameInfo(SystemData* system)
{
	// PortMaster installs launchers in the root of the "ports" system and keeps
	// supplemental metadata in first-level port directories as gameinfo.xml.
	if(system->getName() != "ports")
		return;

	const std::string relativeTo = system->getStartPath();
	if(!Utils::FileSystem::isDirectory(relativeTo))
		return;

	GameInfoCacheData cache = loadPortMasterGameInfoCache();
	const bool gamelistExists = Utils::FileSystem::exists(system->getGamelistPath(false));
	const std::set<std::string> currentLaunchers = getCurrentPortLaunchers(system);

	// Validate every cached metadata source before taking the launcher fast path.
	// A PortMaster update can replace gameinfo.xml without changing its .sh launcher.
	bool cachedSignaturesMatch =
		cache.hasImportedMetadataSnapshot && cache.hasSourceGameSnapshot;
	if(cachedSignaturesMatch)
	{
		for(const auto& entry : cache.files)
		{
			GameInfoSignature currentSignature;
			if(!getGameInfoSignature(entry.first, currentSignature) ||
				!sameSignature(entry.second, currentSignature))
			{
				cachedSignaturesMatch = false;
				break;
			}
		}
	}

	if(gamelistExists && cache.hasLauncherSnapshot &&
		currentLaunchers == cache.launchers && cachedSignaturesMatch)
		return;

	const Utils::FileSystem::stringList entries = Utils::FileSystem::getDirContent(relativeTo);
	std::vector<PendingGameInfo> pending;

	// Fast path: only stat gameinfo.xml files. If size + mtime match the cache,
	// do not open the XML and do not re-read gamelist.xml.
	for(const std::string& portDir : entries)
	{
		if(!Utils::FileSystem::isDirectory(portDir))
			continue;

		const std::string xmlpath = portDir + "/gameinfo.xml";
		GameInfoSignature signature;
		if(!getGameInfoSignature(xmlpath, signature))
			continue;

		if(gamelistExists && cache.hasImportedMetadataSnapshot && cache.hasSourceGameSnapshot)
		{
			auto cached = cache.files.find(xmlpath);
			if(cached != cache.files.end() && sameSignature(cached->second, signature))
				continue;
		}

		PendingGameInfo item;
		item.path = xmlpath;
		item.signature = signature;
		pending.push_back(item);
	}

	if(pending.empty())
		return;

	// Only parse gamelist.xml when at least one PortMaster metadata file is new
	// or changed. Explicit gamelist fields always win over supplemental data.
	const ExplicitMetadataMap explicitMetadata =
		getExplicitGamelistMetadata(system, cache.importedMetadata);
	std::set<FileData*> importedFiles;
	GameInfoCache processedFiles;
	bool cacheChanged = !cache.hasImportedMetadataSnapshot;

	for(const PendingGameInfo& pendingInfo : pending)
	{
		const std::string& xmlpath = pendingInfo.path;
		bool cacheable = true;
		std::set<std::string> currentSourceGames;

		pugi::xml_document doc;
		pugi::xml_parse_result result = doc.load_file(xmlpath.c_str());
		if(!result)
		{
			LOG(LogWarning) << "PortMaster gameinfo: error parsing \"" << xmlpath
				<< "\": " << result.description();
			continue;
		}

		pugi::xml_node root = doc.child("gameList");
		if(!root)
		{
			LOG(LogWarning) << "PortMaster gameinfo: no <gameList> node in \"" << xmlpath << "\"";
			continue;
		}

		for(pugi::xml_node gameNode = root.child("game"); gameNode; gameNode = gameNode.next_sibling("game"))
		{
			pugi::xml_node pathNode = gameNode.child("path");
			if(!pathNode)
			{
				cacheable = false;
				continue;
			}

			const std::string path =
				Utils::FileSystem::resolveRelativePath(pathNode.text().get(), relativeTo, false, true);

			if(!Utils::FileSystem::exists(path))
			{
				cacheable = false;
				continue;
			}

			FileData* file = findOrCreateFile(system, path, GAME);
			if(!file || file->isArcadeAsset())
			{
				cacheable = false;
				continue;
			}

			currentSourceGames.insert(path);

			MetaDataList incoming =
				MetaDataList::createFromXML(GAME_METADATA, gameNode, relativeTo);

			bool merged = false;
			for(const char* key : PORTMASTER_METADATA_KEYS)
			{
				const bool importedValue =
					isImportedMetadataValue(file, cache.importedMetadata, key);
				pugi::xml_node sourceNode = gameNode.child(key);

				// A withdrawn source field is no longer owned by PortMaster. Always
				// drop its ownership hash; only reset the visible value when the
				// current metadata still matches the value ES-X imported previously.
				if(!sourceNode || sourceNode.text().get()[0] == '\0')
				{
					auto importedFile = cache.importedMetadata.find(path);
					if(importedFile != cache.importedMetadata.end() &&
						importedFile->second.find(key) != importedFile->second.end())
					{
						if(importedValue)
						{
							file->metadata.set(key, key == "name" ?
								file->getDisplayName() :
								getDefaultMetadataValue(file->metadata, key));
							merged = true;
						}

						importedFile->second.erase(key);
						if(importedFile->second.empty())
							cache.importedMetadata.erase(importedFile);
						cacheChanged = true;
					}
					continue;
				}

				const std::string& incomingValue = incoming.get(key);

				// A hash-matched value is still PortMaster-owned, so a changed
				// gameinfo.xml value must replace it even though the persisted
				// gamelist value is non-default.
				if(!importedValue &&
					!shouldUseSupplementalMetadata(file, explicitMetadata, key))
				{
					// Migrate values imported by v2 when the persisted value still
					// matches its source. Differing values remain user-owned.
					if(!cache.hasImportedMetadataSnapshot &&
						cache.files.find(xmlpath) != cache.files.end() &&
						file->metadata.get(key) == incomingValue)
					{
						cache.importedMetadata[path][key] = metadataValueHash(incomingValue);
						cacheChanged = true;
					}
					else
					{
						auto importedFile = cache.importedMetadata.find(path);
						if(importedFile != cache.importedMetadata.end() &&
							importedFile->second.erase(key) != 0)
						{
							if(importedFile->second.empty())
								cache.importedMetadata.erase(importedFile);
							cacheChanged = true;
						}
					}
					continue;
				}

				file->metadata.set(key, incomingValue);
				cache.importedMetadata[path][key] = metadataValueHash(incomingValue);
				cacheChanged = true;
				merged = true;
			}

			if(merged)
			{
				importedFiles.insert(file);
				LOG(LogDebug) << "PortMaster gameinfo: imported metadata for \"" << path << "\"";
			}
		}

		if(cacheable)
		{
			// If a changed gameinfo.xml removed an entire <game> record, clear
			// metadata only where the cached hash still proves PortMaster ownership.
			// User/scraper edits survive, but their obsolete ownership hashes do not.
			if(cache.hasSourceGameSnapshot)
			{
				auto previousSource = cache.sourceGames.find(xmlpath);
				if(previousSource != cache.sourceGames.end())
				{
					for(const std::string& oldPath : previousSource->second)
					{
						if(currentSourceGames.count(oldPath) != 0)
							continue;

						auto importedFile = cache.importedMetadata.find(oldPath);
						if(importedFile == cache.importedMetadata.end())
							continue;

						FileData* oldFile = findExistingPortGame(system, oldPath);
						if(oldFile)
						{
							for(const auto& fieldEntry : importedFile->second)
							{
								if(fieldEntry.second !=
									metadataValueHash(oldFile->metadata.get(fieldEntry.first)))
									continue;

								oldFile->metadata.set(fieldEntry.first,
									fieldEntry.first == "name" ?
										oldFile->getDisplayName() :
										getDefaultMetadataValue(oldFile->metadata, fieldEntry.first));
								importedFiles.insert(oldFile);
							}
						}

						cache.importedMetadata.erase(importedFile);
						cacheChanged = true;
					}
				}
			}

			auto previousSource = cache.sourceGames.find(xmlpath);
			if(previousSource == cache.sourceGames.end() ||
				previousSource->second != currentSourceGames)
			{
				if(currentSourceGames.empty())
					cache.sourceGames.erase(xmlpath);
				else
					cache.sourceGames[xmlpath] = currentSourceGames;
				cacheChanged = true;
			}

			// Cache valid, fully-resolved files even when there was nothing new to
			// merge. On subsequent boots they only cost a stat(), not an XML parse.
			processedFiles[xmlpath] = pendingInfo.signature;
		}
	}

	// Persist imported metadata immediately. Afterwards gamelist.xml is the
	// authoritative source and gameinfo.xml is only revisited if it changes.
	if(!importedFiles.empty())
	{
		if(!updateGamelistInternal(system))
		{
			LOG(LogWarning) << "PortMaster gameinfo: gamelist write failed; import cache was not advanced";
			return;
		}

		for(FileData* file : importedFiles)
			file->metadata.resetChangedFlag();

		LOG(LogInfo) << "PortMaster gameinfo: persisted metadata for "
			<< importedFiles.size() << " game(s) into gamelist.xml";
	}

	for(const auto& entry : processedFiles)
	{
		auto cached = cache.files.find(entry.first);
		if(cached == cache.files.end() || !sameSignature(cached->second, entry.second))
		{
			cache.files[entry.first] = entry.second;
			cacheChanged = true;
		}
	}

	cache.hasImportedMetadataSnapshot = true;
	cache.hasSourceGameSnapshot = true;

	if(!cache.hasLauncherSnapshot || cache.launchers != currentLaunchers)
	{
		cache.launchers = currentLaunchers;
		cache.hasLauncherSnapshot = true;
		cacheChanged = true;
	}

	if(cacheChanged)
		savePortMasterGameInfoCache(cache);
}

void parseGamelist(SystemData* system)
{
	bool trustGamelist = Settings::getInstance()->getBool("ParseGamelistOnly");
	std::string xmlpath = system->getGamelistPath(false);
	const std::vector<std::string> allowedExtensions = system->getExtensions();

	if(!Utils::FileSystem::exists(xmlpath))
		return;

	LOG(LogInfo) << "Parsing XML file \"" << xmlpath << "\"...";

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(xmlpath.c_str());

	if(!result)
	{
		LOG(LogError) << "Error parsing XML file \"" << xmlpath << "\"!\n	" << result.description();
		return;
	}

	pugi::xml_node root = doc.child("gameList");
	if(!root)
	{
		LOG(LogError) << "Could not find <gameList> node in gamelist \"" << xmlpath << "\"!";
		return;
	}

	std::string relativeTo = system->getStartPath();

	const char* tagList[2] = { "game", "folder" };
	FileType typeList[2] = { GAME, FOLDER };
	for(int i = 0; i < 2; i++)
	{
		const char* tag = tagList[i];
		FileType type = typeList[i];
		for(pugi::xml_node fileNode = root.child(tag); fileNode; fileNode = fileNode.next_sibling(tag))
		{
			std::string path = fileNode.child("path").text().get();
			path = Utils::FileSystem::resolveRelativePath(path, relativeTo, false, true);

			if(!trustGamelist && !Utils::FileSystem::exists(path))
			{
				LOG(LogWarning) << "File \"" << path << "\" does not exist! Ignoring.";
				continue;
			}

			// Check whether the file's extension is allowed in the system
			if (i == 0 /*game*/ && std::find(allowedExtensions.cbegin(), allowedExtensions.cend(), Utils::FileSystem::getExtension(path)) == allowedExtensions.cend())
			{
				LOG(LogDebug) << "file " << path << " found in gamelist, but has unregistered extension";
				continue;
			}

			FileData* file = findOrCreateFile(system, path, type);
			if(!file)
			{
				// This is often a gamelist inconsistency or a folder that was filtered during scan.
				// Avoid marking it as a hard error.
				LOG(LogWarning) << "Error finding/creating FileData for \"" << path << "\", skipping.";
				continue;
			}
			else if(!file->isArcadeAsset())
			{
				std::string defaultName = file->metadata.get("name");
				file->metadata = MetaDataList::createFromXML(file->getType() == GAME ? GAME_METADATA : FOLDER_METADATA, fileNode, relativeTo);

				//make sure name gets set if one didn't exist
				if(file->metadata.get("name").empty())
					file->metadata.set("name", defaultName);

				file->metadata.resetChangedFlag();
			}
		}
	}
}

void addFileDataNode(pugi::xml_node& parent, const FileData* file, const char* tag, SystemData* system)
{
	//create game and add to parent node
	pugi::xml_node newNode = parent.append_child(tag);

	//write metadata
	file->metadata.appendToXML(newNode, true, system->getStartPath());

	if(newNode.children().begin() == newNode.child("name") //first element is name
		&& ++newNode.children().begin() == newNode.children().end() //theres only one element
		&& newNode.child("name").text().get() == file->getDisplayName()) //the name is the default
	{
		//if the only info is the default name, don't bother with this node
		//delete it and ultimately do nothing
		parent.remove_child(newNode);
	}else{
		//there's something useful in there so we'll keep the node, add the path

		// try and make the path relative if we can so things still work if we change the rom folder location in the future
		std::string relPath = Utils::FileSystem::createRelativePath(file->getPath(), system->getStartPath(), false, true);
		newNode.prepend_child("path").text().set(relPath.c_str());
	}
}

static bool updateGamelistInternal(SystemData* system)
{
	//We do this by reading the XML again, adding changes and then writing it back,
	//because there might be information missing in our systemdata which would then miss in the new XML.
	//We have the complete information for every game though, so we can simply remove a game
	//we already have in the system from the XML, and then add it back from its GameData information...

	if(Settings::getInstance()->getBool("IgnoreGamelist"))
		return false;

	pugi::xml_document doc;
	pugi::xml_node root;
	std::string xmlReadPath = system->getGamelistPath(false);

	std::string relativeTo = system->getStartPath();

	if(Utils::FileSystem::exists(xmlReadPath))
	{
		//parse an existing file first
		pugi::xml_parse_result result = doc.load_file(xmlReadPath.c_str());

		if(!result)
		{
			LOG(LogError) << "Error parsing XML file \"" << xmlReadPath << "\"!\n	" << result.description();
			return false;
		}

		root = doc.child("gameList");
		if(!root)
		{
			LOG(LogError) << "Could not find <gameList> node in gamelist \"" << xmlReadPath << "\"!";
			return false;
		}
	}else{
		//set up an empty gamelist to append to
		root = doc.append_child("gameList");
	}

	std::vector<FileData*> changedGames;
	std::vector<FileData*> changedFolders;

	//now we have all the information from the XML. now iterate through all our games and add information from there
	FileData* rootFolder = system->getRootFolder();
	if (rootFolder != nullptr)
	{
		int numUpdated = 0;

		std::vector<FileData*> files = rootFolder->getFilesRecursive(GAME | FOLDER);

		// Stage 1: iterate through all files in memory, checking for changes
		for(std::vector<FileData*>::const_iterator fit = files.cbegin(); fit != files.cend(); ++fit)
		{

			// do not touch if it wasn't changed anyway
			if (!(*fit)->metadata.wasChanged())
				continue;

			// adding item to changed list
			if ((*fit)->getType() == GAME)
			{
				changedGames.push_back((*fit));
			}
			else
			{
				changedFolders.push_back((*fit));
			}
		}


		// Stage 2: iterate XML if needed, to remove and add changed items
		const char* tagList[2] = { "game", "folder" };
		FileType typeList[2] = { GAME, FOLDER };
		std::vector<FileData*> changedList[2] = { changedGames, changedFolders };

		for(int i = 0; i < 2; i++)
		{
			const char* tag = tagList[i];
			std::vector<FileData*> changes = changedList[i];

			// check for changed items of this type
			if (changes.size() > 0) {
				// check if the item already exists in the XML
				// if it does, remove all corresponding items before adding
				for(pugi::xml_node fileNode = root.child(tag); fileNode; )
				{
					pugi::xml_node pathNode = fileNode.child("path");

					// we need this as we were deleting the iterator and things would become inconsistent
					pugi::xml_node nextNode = fileNode.next_sibling(tag);

					if(!pathNode)
					{
						LOG(LogError) << "<" << tag << "> node contains no <path> child!";
						continue;
					}

					std::string xmlpath = pathNode.text().get();
					// apply the same transformation as in Gamelist::parseGamelist
					xmlpath = Utils::FileSystem::resolveRelativePath(xmlpath, relativeTo, false, true);

					for(std::vector<FileData*>::const_iterator cfit = changes.cbegin(); cfit != changes.cend(); ++cfit)
					{
						if(xmlpath == (*cfit)->getPath())
						{
							// found it
							root.remove_child(fileNode);
							break;
						}
					}
					fileNode = nextNode;

				}

				// add items to XML
				for(std::vector<FileData*>::const_iterator cfit = changes.cbegin(); cfit != changes.cend(); ++cfit)
				{
					// it was either removed or never existed to begin with; either way, we can add it now
					addFileDataNode(root, *cfit, tag, system);
					++numUpdated;
				}
			}
		}

		// now write the file

		if (numUpdated > 0) {
			const auto startTs = std::chrono::system_clock::now();

			//make sure the folders leading up to this path exist (or the write will fail)
			std::string xmlWritePath(system->getGamelistPath(true));
			Utils::FileSystem::createDirectory(Utils::FileSystem::getParent(xmlWritePath));

			LOG(LogInfo) << "Added/Updated " << numUpdated << " entities in '" << xmlReadPath << "'";

			if (!doc.save_file(xmlWritePath.c_str())) {
				LOG(LogError) << "Error saving gamelist.xml to \"" << xmlWritePath << "\" (for system " << system->getName() << ")!";
				return false;
			}

			const auto endTs = std::chrono::system_clock::now();
			LOG(LogInfo) << "Saved gamelist.xml for system \"" << system->getName() << "\" in " << std::chrono::duration_cast<std::chrono::milliseconds>(endTs - startTs).count() << " ms";
		}
	}else{
		LOG(LogError) << "Found no root folder for system \"" << system->getName() << "\"!";
		return false;
	}

	return true;
}

void updateGamelist(SystemData* system)
{
	updateGamelistInternal(system);
}
