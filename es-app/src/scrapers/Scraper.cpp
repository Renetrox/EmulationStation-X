#include "scrapers/Scraper.h"

#include "FileData.h"
#include "GamesDBJSONScraper.h"
#include "ScreenScraper.h"
#include "Log.h"
#include "Settings.h"
#include "SystemData.h"
#include <FreeImage.h>
#include <fstream>
#include <cstdio>

const std::map<std::string, generate_scraper_requests_func> scraper_request_funcs {
	{ "TheGamesDB", &thegamesdb_generate_json_scraper_requests },
	{ "ScreenScraper", &screenscraper_generate_scraper_requests }
};

std::unique_ptr<ScraperSearchHandle> startScraperSearch(const ScraperSearchParams& params)
{
	const std::string& name = Settings::getInstance()->getString("Scraper");
	std::unique_ptr<ScraperSearchHandle> handle(new ScraperSearchHandle());

	// Check if the Scraper in the settings still exists as a registered scraping source.
	if (scraper_request_funcs.find(name) == scraper_request_funcs.end())
	{
		LOG(LogWarning) << "Configured scraper (" << name << ") unavailable, scraping aborted.";
	}
	else
	{
		scraper_request_funcs.at(name)(params, handle->mRequestQueue, handle->mResults);
	}

	return handle;
}

std::vector<std::string> getScraperList()
{
	std::vector<std::string> list;
	for(auto it = scraper_request_funcs.cbegin(); it != scraper_request_funcs.cend(); it++)
	{
		list.push_back(it->first);
	}

	return list;
}

bool isValidConfiguredScraper()
{
	const std::string& name = Settings::getInstance()->getString("Scraper");
	return scraper_request_funcs.find(name) != scraper_request_funcs.end();
}

// ScraperSearchHandle
ScraperSearchHandle::ScraperSearchHandle()
{
	setStatus(ASYNC_IN_PROGRESS);
}

void ScraperSearchHandle::update()
{
	if(mStatus == ASYNC_DONE)
		return;

	if(!mRequestQueue.empty())
	{
		// a request can add more requests to the queue while running,
		// so be careful with references into the queue
		auto& req = *(mRequestQueue.front());
		AsyncHandleStatus status = req.status();

		if(status == ASYNC_ERROR)
		{
			// propegate error
			setError(req.getStatusString());

			// empty our queue
			while(!mRequestQueue.empty())
				mRequestQueue.pop();

			return;
		}

		// finished this one, see if we have any more
		if(status == ASYNC_DONE)
		{
			mRequestQueue.pop();
		}

		// status == ASYNC_IN_PROGRESS
	}

	// we finished without any errors!
	if(mRequestQueue.empty())
	{
		setStatus(ASYNC_DONE);
		return;
	}
}



// ScraperRequest
ScraperRequest::ScraperRequest(std::vector<ScraperSearchResult>& resultsWrite) : mResults(resultsWrite)
{
}


// ScraperHttpRequest
ScraperHttpRequest::ScraperHttpRequest(std::vector<ScraperSearchResult>& resultsWrite, const std::string& url)
	: ScraperRequest(resultsWrite)
{
	setStatus(ASYNC_IN_PROGRESS);
	mReq = std::unique_ptr<HttpReq>(new HttpReq(url));
}

void ScraperHttpRequest::update()
{
	HttpReq::Status status = mReq->status();
	if(status == HttpReq::REQ_SUCCESS)
	{
		setStatus(ASYNC_DONE); // if process() has an error, status will be changed to ASYNC_ERROR
		process(mReq, mResults);
		return;
	}

	// not ready yet
	if(status == HttpReq::REQ_IN_PROGRESS)
		return;

	// everything else is some sort of error
	LOG(LogError) << "ScraperHttpRequest network error (status: " << status << ") - " << mReq->getErrorMsg();
	setError(mReq->getErrorMsg());
}


// metadata resolving stuff

std::unique_ptr<MDResolveHandle> resolveMetaDataAssets(const ScraperSearchResult& result, const ScraperSearchParams& search)
{
	return std::unique_ptr<MDResolveHandle>(new MDResolveHandle(result, search));
}

bool shouldImportScrapedField(const ScraperSearchResult& result, const std::string& key, const std::string& current)
{
	if (!result.suppliedFields.count(key) || result.mdl.get(key).empty()) return false;
	if (key == "favorite" || key == "hidden" || key == "kidgame" || key == "sortname" ||
		key == "playcount" || key == "lastplayed") return false;
	if (current.empty()) return true;
	if (key == "name") return Settings::getInstance()->getBool("ScrapeOverwriteNames");
	if (key == "desc") return Settings::getInstance()->getBool("ScrapeOverwriteDescriptions");
	for (const auto& decl : result.mdl.getMDD())
		if (decl.key == key && decl.type == MD_PATH)
			return Settings::getInstance()->getBool("ScrapeOverwriteMedia");
	return true;
}

void importScrapedMetadata(MetaDataList& destination, const ScraperSearchResult& result)
{
	for (const auto& key : result.suppliedFields)
		if (shouldImportScrapedField(result, key, destination.get(key)))
			destination.set(key, result.mdl.get(key));
}

MDResolveHandle::MDResolveHandle(const ScraperSearchResult& result, const ScraperSearchParams& search) : mResult(result)
{
	auto media = result.mediaUrls;
	// Keep TheGamesDB's existing single-image interface working.
	if (media.count("image") == 0 && !result.imageUrl.empty())
	{
		std::string ext = result.imageType;
		if (ext.empty())
		{
			const std::string url = result.imageUrl.substr(0, result.imageUrl.find_first_of("?#"));
			const auto dot = url.find_last_of('.');
			if (dot != std::string::npos) ext = url.substr(dot);
		}
		media["image"] = {result.imageUrl, ext};
	}
	for (const auto& entry : media)
	{
		const std::string key = entry.first;
		if (!Settings::getInstance()->getBool("ScrapeOverwriteMedia") &&
			!search.game->metadata.get(key).empty()) continue;
		std::string ext = entry.second.extension;
		if (ext.empty() || ext[0] != '.' || ext.find_first_not_of(".abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") != std::string::npos)
			ext = key == "video" ? ".mp4" : ".png";
		const std::string path = getSaveAsPath(search, key, ext);
		std::unique_ptr<ImageDownloadHandle> download = key == "video" ?
			std::unique_ptr<ImageDownloadHandle>(new ImageDownloadHandle(entry.second.url, path, 0, 0)) :
			downloadImageAsync(entry.second.url, path);
		mFuncs.push_back(ResolvePair(std::move(download), [this, key, path]
		{
			mResult.setMetadata(key, path);
			if (key == "image") mResult.imageUrl.clear();
		}));
	}
}

void MDResolveHandle::update()
{
	if(mStatus == ASYNC_DONE || mStatus == ASYNC_ERROR)
		return;

	auto it = mFuncs.cbegin();
	while(it != mFuncs.cend())
	{
		if(it->first->status() == ASYNC_ERROR)
		{
			setError(it->first->getStatusString());
			return;
		}else if(it->first->status() == ASYNC_DONE)
		{
			it->second();
			it = mFuncs.erase(it);
			continue;
		}
		it++;
	}

	if(mFuncs.empty())
		setStatus(ASYNC_DONE);
}

std::unique_ptr<ImageDownloadHandle> downloadImageAsync(const std::string& url, const std::string& saveAs)
{
	return std::unique_ptr<ImageDownloadHandle>(new ImageDownloadHandle(url, saveAs,
		Settings::getInstance()->getInt("ScraperResizeWidth"), Settings::getInstance()->getInt("ScraperResizeHeight")));
}

ImageDownloadHandle::ImageDownloadHandle(const std::string& url, const std::string& path, int maxWidth, int maxHeight) :
	mReq(new HttpReq(url)), mSavePath(path), mMaxWidth(maxWidth), mMaxHeight(maxHeight)
{
}

void ImageDownloadHandle::update()
{
	if(mReq->status() == HttpReq::REQ_IN_PROGRESS)
		return;

	if(mReq->status() != HttpReq::REQ_SUCCESS)
	{
		std::stringstream ss;
		ss << "Network error: " << mReq->getErrorMsg();
		setError(ss.str());
		return;
	}

	// download is done, save it to disk
	const std::string temporaryPath = mSavePath + ".part" + mSavePath.substr(mSavePath.find_last_of('.'));
	std::ofstream stream(temporaryPath, std::ios_base::out | std::ios_base::binary);
	if(!stream)
	{
		setError("Failed to open image path to write. Permission error? Disk full?");
		return;
	}

	const std::string& content = mReq->getContent();
	if (content.empty())
	{
		stream.close();
		std::remove(temporaryPath.c_str());
		setError("Downloaded media is empty.");
		return;
	}
	stream.write(content.data(), content.length());
	stream.close();
	if(!stream)
	{
		std::remove(temporaryPath.c_str());
		setError("Failed to save image. Disk full?");
		return;
	}

	// resize it
	if(!resizeImage(temporaryPath, mMaxWidth, mMaxHeight))
	{
		std::remove(temporaryPath.c_str());
		setError("Error saving resized image. Out of memory? Disk full?");
		return;
	}

	if (std::rename(temporaryPath.c_str(), mSavePath.c_str()) != 0)
	{
		std::remove(temporaryPath.c_str());
		setError("Failed to install downloaded media.");
		return;
	}
	setStatus(ASYNC_DONE);
}

//you can pass 0 for width or height to keep aspect ratio
bool resizeImage(const std::string& path, int maxWidth, int maxHeight)
{
	// nothing to do
	if(maxWidth == 0 && maxHeight == 0)
		return true;

	FREE_IMAGE_FORMAT format = FIF_UNKNOWN;
	FIBITMAP* image = NULL;

	//detect the filetype
	format = FreeImage_GetFileType(path.c_str(), 0);
	if(format == FIF_UNKNOWN)
		format = FreeImage_GetFIFFromFilename(path.c_str());
	if(format == FIF_UNKNOWN)
	{
		LOG(LogError) << "Error - could not detect filetype for image \"" << path << "\"!";
		return false;
	}

	//make sure we can read this filetype first, then load it
	if(FreeImage_FIFSupportsReading(format))
	{
		image = FreeImage_Load(format, path.c_str());
	}else{
		LOG(LogError) << "Error - file format reading not supported for image \"" << path << "\"!";
		return false;
	}

	if (image == NULL) return false;
	float width = (float)FreeImage_GetWidth(image);
	float height = (float)FreeImage_GetHeight(image);

	if(maxWidth == 0)
	{
		maxWidth = (int)((maxHeight / height) * width);
	}else if(maxHeight == 0)
	{
		maxHeight = (int)((maxWidth / width) * height);
	}

	FIBITMAP* imageRescaled = FreeImage_Rescale(image, maxWidth, maxHeight, FILTER_BILINEAR);
	FreeImage_Unload(image);

	if(imageRescaled == NULL)
	{
		LOG(LogError) << "Could not resize image! (not enough memory? invalid bitdepth?)";
		return false;
	}

	bool saved = (FreeImage_Save(format, imageRescaled, path.c_str()) != 0);
	FreeImage_Unload(imageRescaled);

	if(!saved)
		LOG(LogError) << "Failed to save resized image!";

	return saved;
}

std::string getSaveAsPath(const ScraperSearchParams& params, const std::string& suffix, const std::string& extension)
{
	const std::string subdirectory = params.system->getName();
	const std::string name = Utils::FileSystem::getStem(params.game->getPath()) + "-" + suffix;

	std::string path = Utils::FileSystem::getHomePath() + "/.emulationstation/downloaded_images/";

	if(!Utils::FileSystem::exists(path))
		Utils::FileSystem::createDirectory(path);

	path += subdirectory + "/";

	if(!Utils::FileSystem::exists(path))
		Utils::FileSystem::createDirectory(path);


	path += name + extension;
	return path;
}
