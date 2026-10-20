#include <exception>
#include <map>

#include "scrapers/GamesDBJSONScraper.h"
#include "scrapers/GamesDBJSONScraperResources.h"

#include "FileData.h"
#include "Log.h"
#include "PlatformId.h"
#include "Settings.h"
#include "SystemData.h"
#include "utils/TimeUtil.h"
#include <pugixml.hpp>

/* When raspbian will get an up to date version of rapidjson we'll be
   able to have it throw in case of error with the following:
#ifndef RAPIDJSON_ASSERT
#define RAPIDJSON_ASSERT(x)                                                    \
  if (!(x)) {                                                                  \
	throw std::runtime_error("rapidjson internal assertion failure: " #x);     \
  }
#endif // RAPIDJSON_ASSERT
*/

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

using namespace PlatformIds;
using namespace rapidjson;

namespace
{
TheGamesDBJSONRequestResources resources;
}

const std::map<PlatformId, std::string> gamesdb_new_platformid_map{
	{ THREEDO, "25" },
	{ AMIGA, "4911" },
	{ AMSTRAD_CPC, "4914" },
	{ APPLE_II, "4942" },
	{ ARCADE, "23" },
	{ ATARI_800, "4943" },
	{ ATARI_2600, "22" },
	{ ATARI_5200, "26" },
	{ ATARI_7800, "27" },
	{ ATARI_JAGUAR, "28" },
	{ ATARI_JAGUAR_CD, "29" },
	{ ATARI_LYNX, "4924" },
	{ ATARI_ST, "4937" },
	{ ATARI_XE, "30" },
	{ COLECOVISION, "31" },
	{ COMMODORE_64, "40" },
	{ DAPHNE, "23" },
	{ INTELLIVISION, "32" },
	{ MAC_OS, "37" },
	{ XBOX, "14" },
	{ XBOX_360, "15" },
	{ MSX, "4929" },
	{ NEOGEO, "24" },
	{ NEOGEO_CD, "4956" },
	{ NEOGEO_POCKET, "4922" },
	{ NEOGEO_POCKET_COLOR, "4923" },
	{ NINTENDO_3DS, "4912" },
	{ NINTENDO_64, "3" },
	{ NINTENDO_DS, "8" },
	{ FAMICOM_DISK_SYSTEM, "4936" },
	{ NINTENDO_ENTERTAINMENT_SYSTEM, "7" },
	{ NINTENDO_POKEMON_MINI, "4957" },
	{ GAME_BOY, "4" },
	{ GAME_BOY_ADVANCE, "5" },
	{ GAME_BOY_COLOR, "41" },
	{ NINTENDO_GAMECUBE, "2" },
	{ NINTENDO_WII, "9" },
	{ NINTENDO_WII_U, "38" },
	{ NINTENDO_VIRTUAL_BOY, "4918" },
	{ NINTENDO_GAME_AND_WATCH, "4950" },
	{ NINTENDO_SWITCH, "4971" },
	{ PC, "1" },
	{ SEGA_32X, "33" },
	{ SEGA_CD, "21" },
	{ SEGA_DREAMCAST, "16" },
	{ SEGA_GAME_GEAR, "20" },
	{ SEGA_GENESIS, "18" },
	{ SEGA_MASTER_SYSTEM, "35" },
	{ SEGA_MEGA_DRIVE, "36" },
	{ SEGA_SATURN, "17" },
	{ SEGA_SG1000, "4949" },
	{ SAM_COUPE, "4979" },
	{ PLAYSTATION, "10" },
	{ PLAYSTATION_2, "11" },
	{ PLAYSTATION_3, "12" },
	{ PLAYSTATION_4, "4919" },
	{ PLAYSTATION_VITA, "39" },
	{ PLAYSTATION_PORTABLE, "13" },
	{ SUPER_NINTENDO, "6" },
	{ SHARP_X1, "4977" },
	{ SHARP_X6800, "4931"},
	{ NEC_PC_8801, "4933"},
	{ NEC_PC_9801, "4934"},
	{ TURBOGRAFX_16, "34" },   // HuCards only
	{ TURBOGRAFX_CD, "4955" }, // CD-ROMs only
	{ NEC_PCFX, "4930" },
	{ WONDERSWAN, "4925" },
	{ WONDERSWAN_COLOR, "4926" },
	{ ZX_SPECTRUM, "4913" },
	{ ZX81_SINCLAR, "5010" },
	{ VIDEOPAC_ODYSSEY2, "4927" },
	{ VECTREX, "4939" },
	{ TRS80_COLOR_COMPUTER, "4941" },
	{ TI_99, "4953" },
	{ TANDY, "4941" },
	{ FMTOWNS, "4932" },
};

void thegamesdb_generate_json_scraper_requests(const ScraperSearchParams& params,
	std::queue<std::unique_ptr<ScraperRequest>>& requests, std::vector<ScraperSearchResult>& results)
{
	resources.prepare();
	std::string path = "https://api.thegamesdb.net/v1";
	bool usingGameID = false;
	const std::string apiKey = std::string("apikey=") + resources.getApiKey();
	std::string cleanName = params.nameOverride;
	if (!cleanName.empty() && cleanName.substr(0, 3) == "id:")
	{
		std::string gameID = cleanName.substr(3);
		path += "/Games/ByGameID?" + apiKey +
				"&fields=players,publishers,genres,overview,last_updated,rating,"
				"platform,coop,youtube,os,processor,ram,hdd,video,sound,alternates&"
				"include=boxart&id=" +
				HttpReq::urlEncode(gameID);
		usingGameID = true;
	} else
	{
		if (cleanName.empty())
			cleanName = params.game->getCleanName();
		path += "/Games/ByGameName?" + apiKey +
				"&fields=players,publishers,genres,overview,last_updated,rating,"
				"platform,coop,youtube,os,processor,ram,hdd,video,sound,alternates&"
				"include=boxart&name=" +
				HttpReq::urlEncode(cleanName);
	}

	if (usingGameID)
	{
		// if we have the ID already, we don't need the GetGameList request
		requests.push(std::unique_ptr<ScraperRequest>(new TheGamesDBJSONRequest(results, path)));
	} else
	{
		std::string platformQueryParam;
		auto& platforms = params.system->getPlatformIds();
		if (!platforms.empty())
		{
			bool first = true;
			platformQueryParam += "&filter%5Bplatform%5D=";
			for (auto platformIt = platforms.cbegin(); platformIt != platforms.cend(); platformIt++)
			{
				auto mapIt = gamesdb_new_platformid_map.find(*platformIt);
				if (mapIt != gamesdb_new_platformid_map.cend())
				{
					if (!first)
					{
						platformQueryParam += ",";
					}
					platformQueryParam += HttpReq::urlEncode(mapIt->second);
					first = false;
				} else
				{
					LOG(LogWarning) << "TheGamesDB scraper warning - no support for platform "
									<< getPlatformName(*platformIt);
				}
			}
			path += platformQueryParam;
		}

		requests.push(std::unique_ptr<ScraperRequest>(new TheGamesDBJSONRequest(requests, results, path)));
	}
}

namespace
{

std::string getStringOrThrow(const Value& v, const std::string& key)
{
	if (!v.HasMember(key.c_str()) || !v[key.c_str()].IsString())
	{
		throw std::runtime_error("rapidjson internal assertion failure: missing or non string key:" + key);
	}
	return v[key.c_str()].GetString();
}

int getIntOrThrow(const Value& v, const std::string& key)
{
	if (!v.HasMember(key.c_str()) || !v[key.c_str()].IsInt())
	{
		throw std::runtime_error("rapidjson internal assertion failure: missing or non int key:" + key);
	}
	return v[key.c_str()].GetInt();
}

int getIntOrThrow(const Value& v)
{
	if (!v.IsInt())
	{
		throw std::runtime_error("rapidjson internal assertion failure: not an int");
	}
	return v.GetInt();
}

std::string getBoxartImage(const Value& v)
{
	if (!v.IsArray())
		return "";

	std::string firstValidImage;
	for (SizeType i = 0; i < v.Size(); ++i)
	{
		const Value& image = v[i];
		if (!image.IsObject() || !image.HasMember("filename") || !image["filename"].IsString())
			continue;

		const std::string filename = image["filename"].GetString();
		if (firstValidImage.empty())
			firstValidImage = filename;

		if (image.HasMember("type") && image["type"].IsString() &&
			image.HasMember("side") && image["side"].IsString() &&
			std::string(image["type"].GetString()) == "boxart" &&
			std::string(image["side"].GetString()) == "front")
			return filename;
	}
	return firstValidImage;
}

std::string getDeveloperString(const Value& v)
{
	if (!v.IsArray())
	{
		return "";
	}
	std::string out = "";
	bool first = true;
	for (int i = 0; i < (int)v.Size(); ++i)
	{
		auto mapIt = resources.gamesdb_new_developers_map.find(getIntOrThrow(v[i]));
		if (mapIt == resources.gamesdb_new_developers_map.cend())
		{
			continue;
		}
		if (!first)
		{
			out += ", ";
		}
		out += mapIt->second;
		first = false;
	}
	return out;
}

std::string getPublisherString(const Value& v)
{
	if (!v.IsArray())
	{
		return "";
	}
	std::string out = "";
	bool first = true;
	for (int i = 0; i < (int)v.Size(); ++i)
	{
		auto mapIt = resources.gamesdb_new_publishers_map.find(getIntOrThrow(v[i]));
		if (mapIt == resources.gamesdb_new_publishers_map.cend())
		{
			continue;
		}
		if (!first)
		{
			out += ", ";
		}
		out += mapIt->second;
		first = false;
	}
	return out;
}

std::string getGenreString(const Value& v)
{
	if (!v.IsArray())
	{
		return "";
	}
	std::string out = "";
	bool first = true;
	for (int i = 0; i < (int)v.Size(); ++i)
	{
		auto mapIt = resources.gamesdb_new_genres_map.find(getIntOrThrow(v[i]));
		if (mapIt == resources.gamesdb_new_genres_map.cend())
		{
			continue;
		}
		if (!first)
		{
			out += ", ";
		}
		out += mapIt->second;
		first = false;
	}
	return out;
}

void processGame(const Value& game, const Value* boxart, std::vector<ScraperSearchResult>& results)
{
	ScraperSearchResult result;

	result.setMetadata("name", getStringOrThrow(game, "game_title"));
	if (game.HasMember("overview") && game["overview"].IsString())
	{
		result.setMetadata("desc", game["overview"].GetString());
	}
	if (game.HasMember("release_date") && game["release_date"].IsString())
	{
		result.setMetadata(
			"releasedate", Utils::Time::DateTime(Utils::Time::stringToTime(game["release_date"].GetString(), "%Y-%m-%d")));
	}
	if (game.HasMember("developers") && game["developers"].IsArray())
	{
		result.setMetadata("developer", getDeveloperString(game["developers"]));
	}
	if (game.HasMember("publishers") && game["publishers"].IsArray())
	{
		result.setMetadata("publisher", getPublisherString(game["publishers"]));
	}
	if (game.HasMember("genres") && game["genres"].IsArray())
	{

		result.setMetadata("genre", getGenreString(game["genres"]));
	}
	if (game.HasMember("players") && game["players"].IsInt())
	{
		result.setMetadata("players", std::to_string(game["players"].GetInt()));
	}


	// Artwork is optional. Preserve game metadata even when TheGamesDB
	// omits the include=boxart payload or a particular game has no image.
	if (boxart && boxart->IsObject() && boxart->HasMember("base_url") &&
		(*boxart)["base_url"].IsObject() && boxart->HasMember("data") &&
		(*boxart)["data"].IsObject() && game.HasMember("id") && game["id"].IsInt())
	{
		const Value& baseUrl = (*boxart)["base_url"];
		const Value& data = (*boxart)["data"];
		const std::string id = std::to_string(game["id"].GetInt());
		if (data.HasMember(id.c_str()))
		{
			const std::string image = getBoxartImage(data[id.c_str()]);
			if (!image.empty())
			{
				if (baseUrl.HasMember("thumb") && baseUrl["thumb"].IsString())
					result.thumbnailUrl = std::string(baseUrl["thumb"].GetString()) + "/" + image;
				if (baseUrl.HasMember("large") && baseUrl["large"].IsString())
					result.imageUrl = std::string(baseUrl["large"].GetString()) + "/" + image;
			}
		}
	}

	results.push_back(result);
}
} // namespace

void TheGamesDBJSONRequest::process(const std::unique_ptr<HttpReq>& req, std::vector<ScraperSearchResult>& results)
{
	assert(req->status() == HttpReq::REQ_SUCCESS);

	Document doc;
	doc.Parse(req->getContent().c_str());

	if (doc.HasParseError())
	{
		std::string err =
			std::string("TheGamesDBJSONRequest - Error parsing JSON. \n\t") + GetParseError_En(doc.GetParseError());
		setError(err);
		LOG(LogError) << err;
		return;
	}

	if (!doc.IsObject() || !doc.HasMember("data") || !doc["data"].IsObject() ||
		!doc["data"].HasMember("games") || !doc["data"]["games"].IsArray())
	{
		std::string warn = "TheGamesDBJSONRequest - Response had no game data.\n";
		LOG(LogWarning) << warn;
		return;
	}
	const Value& games = doc["data"]["games"];

	const Value* boxart = nullptr;
	if (doc.HasMember("include") && doc["include"].IsObject() &&
		doc["include"].HasMember("boxart") && doc["include"]["boxart"].IsObject())
	{
		boxart = &doc["include"]["boxart"];
	}
	else
	{
		LOG(LogWarning) << "TheGamesDBJSONRequest - No boxart returned; importing text metadata.";
	}

	resources.ensureResources();


	for (int i = 0; i < (int)games.Size(); ++i)
	{
		const Value& v = games[i];
		if (!v.IsObject())
			continue;
		try
		{
			processGame(v, boxart, results);
		}
		catch (std::runtime_error& e)
		{
			LOG(LogError) << "Error while processing game: " << e.what();
		}
	}
}
