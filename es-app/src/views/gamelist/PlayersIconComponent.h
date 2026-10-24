#pragma once
#ifndef ES_APP_VIEWS_GAME_LIST_PLAYERS_ICON_COMPONENT_H
#define ES_APP_VIEWS_GAME_LIST_PLAYERS_ICON_COMPONENT_H

#include "components/ImageComponent.h"
#include "ThemeData.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

// Optional game-metadata badge. Only {game:players} is expanded,
// so unrelated theme images retain their original behavior.
class PlayersIconComponent : public ImageComponent
{
public:
	explicit PlayersIconComponent(Window* window) : ImageComponent(window)
	{
		setVisible(false);
	}

	void applyPlayersTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view)
	{
		mTemplatePath.clear();
		mCurrentPath.clear();
		setDefaultImage("");
		setImage("");
		setVisible(false);

		const ThemeData::ThemeElement* elem =
			theme ? theme->getElement(view, "md_players_icon", "image") : nullptr;

		if (!elem || !elem->has("path"))
			return;

		const std::string path = elem->get<std::string>("path");
		if (path.find("{game:players}") == std::string::npos)
			return;

		mTemplatePath = path;
		// Apply sizing/placement/default/zIndex but not the unresolved path.
		ImageComponent::applyTheme(theme, view, "md_players_icon",
			ThemeFlags::ALL & ~ThemeFlags::PATH);
		setVisible(!elem->has("visible") || elem->get<bool>("visible"));
	}

	void setPlayers(const std::string& rawPlayers)
	{
		if (mTemplatePath.empty())
			return;

		const std::string count = normalizePlayers(rawPlayers);
		std::string path;
		if (!count.empty())
		{
			path = mTemplatePath;
			const size_t pos = path.find("{game:players}");
			path.replace(pos, sizeof("{game:players}") - 1, count);
		}

		if (path != mCurrentPath)
		{
			mCurrentPath = path;
			// Unavailable/unknown counts fall back to the theme's <default>.
			setImage(path);
		}
	}

private:
	// Gamelists can store ranges, e.g. 1-2. Show the maximum player count.
	// Values that are not simple counts/ranges retain the generic icon.
	static std::string normalizePlayers(const std::string& raw)
	{
		std::string count;
		for (unsigned char ch : raw)
			if (!std::isspace(ch))
				count += static_cast<char>(ch);

		const auto validCount = [](const std::string& value) {
			return !value.empty() && value.size() <= 2 &&
				std::all_of(value.begin(), value.end(), [](unsigned char ch) {
					return std::isdigit(ch) != 0;
				}) && std::atoi(value.c_str()) > 0;
		};

		const size_t dash = count.find('-');
		if (dash == std::string::npos)
			return validCount(count) ? std::to_string(std::atoi(count.c_str())) : "";

		const std::string minimum = count.substr(0, dash);
		const std::string maximum = count.substr(dash + 1);
		if (!validCount(minimum) || !validCount(maximum) ||
			std::atoi(minimum.c_str()) > std::atoi(maximum.c_str()))
			return "";

		return std::to_string(std::atoi(maximum.c_str()));
	}

	std::string mTemplatePath;
	std::string mCurrentPath;
};

#endif // ES_APP_VIEWS_GAME_LIST_PLAYERS_ICON_COMPONENT_H
