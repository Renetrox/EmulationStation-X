#include "components/HelpComponent.h"

#include "components/ComponentGrid.h"
#include "components/ImageComponent.h"
#include "components/TextComponent.h"

#include "resources/ResourceManager.h"
#include "resources/Font.h"

#include "utils/StringUtil.h"
#include "utils/FileSystemUtil.h"

#include "Log.h"
#include "Settings.h"
#include "../LocaleESHook.h"   // es_translate()

#include <algorithm>

#define ICON_TEXT_SPACING 8
#define ENTRY_SPACING 16

static std::string trimSlashes(std::string s)
{
	while (!s.empty() && (s.back() == '/' || s.back() == '\\'))
		s.pop_back();
	return s;
}

HelpComponent::HelpComponent(Window* window)
	: GuiComponent(window)
{
}

void HelpComponent::clearPrompts()
{
	mPrompts.clear();
	updateGrid();
}

void HelpComponent::setPrompts(const std::vector<HelpPrompt>& prompts)
{
	if (promptsEqual(mPrompts, prompts))
		return;

	mPrompts = prompts;
	updateGrid();
}

void HelpComponent::setStyle(const HelpStyle& style)
{
	if (styleEqual(mStyle, style))
		return;

	mStyle = style;
	updateGrid();
}

bool HelpComponent::promptsEqual(const std::vector<HelpPrompt>& a, const std::vector<HelpPrompt>& b) const
{
	if (a.size() != b.size())
		return false;

	for (size_t i = 0; i < a.size(); i++)
	{
		if (a[i].first != b[i].first)  return false;
		if (a[i].second != b[i].second) return false;
	}
	return true;
}

bool HelpComponent::styleEqual(const HelpStyle& a, const HelpStyle& b) const
{
	if (a.position != b.position) return false;
	if (a.iconColor != b.iconColor) return false;
	if (a.textColor != b.textColor) return false;
	if (a.font != b.font) return false;
	return true;
}

std::string HelpComponent::normalizeKey(const std::string& key)
{
	std::string k = Utils::String::toLower(key);

	if (k == "favorites" || k == "favs" || k == "favorite") return "FAVORITES";
	if (k == "back")  return "BACK";
	if (k == "select") return "SELECT";
	if (k == "menu")  return "MENU";
	if (k == "random") return "RANDOM";
	if (k == "choose") return "CHOOSE";
	if (k == "close") return "CLOSE";

	return Utils::String::toUpper(key);
}

std::string HelpComponent::getSettingSafe(const std::string& key, const std::string& def)
{
	std::string v = Settings::getInstance()->getString(key);
	return v.empty() ? def : v;
}

std::string HelpComponent::joinPath(const std::string& base, const std::string& rel)
{
	if (base.empty())
		return rel;

	if (base.rfind(":/", 0) == 0)
	{
		std::string b = trimSlashes(base);
		if (!rel.empty() && rel.front() == '/')
			return b + rel;
		return b + "/" + rel;
	}

	std::string b = trimSlashes(base);
	if (!rel.empty() && (rel.front() == '/' || rel.front() == '\\'))
		return b + rel;

	return b + "/" + rel;
}

bool HelpComponent::existsAny(const std::string& path)
{
	if (path.rfind(":/", 0) == 0)
		return ResourceManager::getInstance()->fileExists(path);

	return Utils::FileSystem::exists(path);
}

std::vector<std::string> HelpComponent::getSearchBases()
{
	std::vector<std::string> bases;

	const std::string set = getSettingSafe("HelpIconSet", "default");
	const std::string home = Utils::FileSystem::getHomePath();

	if (set != "default" && !set.empty())
	{
		bases.push_back(home + "/.emulationstation/help/" + set);
		bases.push_back(home + "/.emulationstation/help/iconsets/" + set);
	}

	bases.push_back(home + "/.emulationstation/help");
	bases.push_back(home + "/.emulationstation/help/iconsets");

	if (set != "default" && !set.empty())
		bases.push_back(std::string(":/help/") + set);

	bases.push_back(":/help");

	return bases;
}

std::vector<std::string> HelpComponent::buildCandidateRelativeFiles(const std::string& logicalName)
{
	std::vector<std::string> rel;

	auto svg = [&](const std::string& n) { rel.push_back(n + ".svg"); };
	auto png = [&](const std::string& n) { rel.push_back(n + ".png"); };

	if (logicalName == "up/down")
	{
		svg("dpad_updown"); svg("updown"); png("updown");
	}
	else if (logicalName == "left/right")
	{
		svg("dpad_leftright"); svg("leftright"); png("leftright");
	}
	else if (logicalName == "up/down/left/right")
	{
		svg("dpad_all"); svg("all"); png("all");
	}
	else if (logicalName == "a" || logicalName == "b" || logicalName == "x" || logicalName == "y")
	{
		svg(logicalName); png(logicalName);
		svg(std::string("button_") + logicalName);
		svg(std::string("mbuttons_") + logicalName);
	}
	else if (logicalName == "start")
	{
		svg("start"); png("start"); svg("button_start");
	}
	else if (logicalName == "select")
	{
		svg("select"); png("select"); svg("button_select");
	}
	else if (logicalName == "back")
	{
		svg("back"); png("back"); svg("button_back");
	}
	else if (logicalName == "l")
	{
		svg("l"); png("l"); svg("button_l");
	}
	else if (logicalName == "r")
	{
		svg("r"); png("r"); svg("button_r");
	}
	else if (logicalName == "lr")
	{
		svg("lr"); png("lr"); svg("button_lr");
	}
	else if (logicalName == "lt")
	{
		svg("lt"); png("lt"); svg("button_lt");
	}
	else if (logicalName == "rt")
	{
		svg("rt"); png("rt"); svg("button_rt");
	}
	else if (logicalName == "ltrt")
	{
		svg("ltrt"); png("ltrt"); svg("button_ltrt");
	}
	else
	{
		svg(logicalName); png(logicalName); svg(std::string("button_") + logicalName);
	}

	return rel;
}

std::string HelpComponent::getIconPath(const char* logicalName)
{
	const std::string key(logicalName);

	const std::string currentSet = getSettingSafe("HelpIconSet", "default");
	if (mCachedIconSet != currentSet)
	{
		mIconPathCache.clear();
		mCachedIconSet = currentSet;
	}

	auto it = mIconPathCache.find(key);
	if (it != mIconPathCache.cend())
		return it->second;

	const std::vector<std::string> relFiles = buildCandidateRelativeFiles(key);
	const std::vector<std::string> bases = getSearchBases();

	for (const auto& base : bases)
	{
		for (const auto& rel : relFiles)
		{
			const std::string full = joinPath(base, rel);
			if (existsAny(full))
			{
				mIconPathCache[key] = full;
				return full;
			}
		}
	}

	LOG(LogWarning) << "HelpComponent: missing icon for \"" << key << "\"";
	mIconPathCache[key] = "";
	return "";
}

void HelpComponent::updateGrid()
{
	// Apagado global: ahí sí destruimos
	if (!Settings::getInstance()->getBool("ShowHelpPrompts"))
	{
		mGrid.reset();
		mLastBuiltPrompts.clear();
		return;
	}

	// Si no hay prompts: NO destruimos el grid (evita 1 frame vacío / parpadeo)
	if (mPrompts.empty())
	{
		mLastBuiltPrompts.clear();
		return;
	}

	// Si nada cambió respecto a lo ya construido, no toques nada.
	if (mGrid && promptsEqual(mLastBuiltPrompts, mPrompts) && styleEqual(mLastBuiltStyle, mStyle))
		return;

	std::shared_ptr<Font> font = mStyle.font;
	if (!font)
		font = Font::get(FONT_SIZE_SMALL);

	const float height = Math::round(font->getLetterHeight() * 1.25f);

	// Construimos un grid NUEVO (seguro) y al final hacemos swap
	auto newGrid = std::make_shared<ComponentGrid>(mWindow, Vector2i((int)mPrompts.size() * 4, 1));

	float width = 0.0f;

	std::vector<std::shared_ptr<ImageComponent>> icons;
	std::vector<std::shared_ptr<TextComponent>> labels;

	icons.reserve(mPrompts.size());
	labels.reserve(mPrompts.size());

	for (auto it = mPrompts.cbegin(); it != mPrompts.cend(); ++it)
	{
		auto icon = std::make_shared<ImageComponent>(mWindow);

		// Give ImageComponent the final target before resolving the image. This is
		// required by the ES-DE style SVG path: it reads SVG dimensions first and
		// then requests a resolution-specific cached texture.
		icon->setResize(0, height);
		icon->setImage(getIconPath(it->first.c_str()));
		icon->setColorShift(mStyle.iconColor);
		icons.push_back(icon);

		std::string normalizedKey = normalizeKey(it->second);
		std::string translated = es_translate(normalizedKey);

		auto lbl = std::make_shared<TextComponent>(
			mWindow,
			Utils::String::toUpper(translated),
			font,
			mStyle.textColor
		);
		labels.push_back(lbl);

		width += icon->getSize().x() + lbl->getSize().x() + ICON_TEXT_SPACING + ENTRY_SPACING;
	}

	if (width <= 0.0f)
		width = 1.0f;

	newGrid->setSize(width, height);

	for (unsigned int i = 0; i < icons.size(); i++)
	{
		const int col = (int)i * 4;

		const float iconW  = icons.at(i)->getSize().x();
		const float labelW = labels.at(i)->getSize().x();

		newGrid->setColWidthPerc(col,     iconW / width);
		newGrid->setColWidthPerc(col + 1, (float)ICON_TEXT_SPACING / width);
		newGrid->setColWidthPerc(col + 2, labelW / width);

		newGrid->setEntry(icons.at(i),  Vector2i(col, 0),     false, false);
		newGrid->setEntry(labels.at(i), Vector2i(col + 2, 0), false, false);
	}

	newGrid->setPosition(Vector3f(mStyle.position.x(), mStyle.position.y(), 0.0f));

	// ✅ Swap atómico: nunca queda “sin grid” a mitad
	mGrid = newGrid;
	mLastBuiltPrompts = mPrompts;
	mLastBuiltStyle   = mStyle;
}

void HelpComponent::setOpacity(unsigned char opacity)
{
	GuiComponent::setOpacity(opacity);

	if (!mGrid)
		return;

	for (unsigned int i = 0; i < mGrid->getChildCount(); i++)
		mGrid->getChild(i)->setOpacity(opacity);
}

void HelpComponent::render(const Transform4x4f& parent)
{
	if (!mGrid || mPrompts.empty())
		return;

	Transform4x4f trans = parent * getTransform();
	mGrid->render(trans);
}
