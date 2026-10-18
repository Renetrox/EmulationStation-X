// es-app/src/guis/GuiThemeOptions.cpp

#include "guis/GuiThemeOptions.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiTextEditKeyboardPopup.h"

#include "LocaleES.h"
#include "Settings.h"
#include "utils/FileSystemUtil.h"
#include "resources/Font.h"
#include "views/ViewController.h"
#include "SystemData.h"
#include "Window.h"
#include "renderers/Renderer.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>
#include <cctype>
#include <vector>

// ✅ NUEVO: restart request (micro-reinicio)
#include "RestartRequest.h"
#include <SDL_events.h>

namespace
{
	inline std::string _(const std::string& key)
	{
		return LocaleES::getInstance().translate(key);
	}

	struct ThemeOption
	{
		std::string id;
		std::string type;
		std::string label;
		std::string path;
		std::string applyTo;
		std::string key;

		std::vector<std::pair<std::string, std::string>> values;
		std::string defaultValue;
	};

	static std::string trim(const std::string& s)
	{
		size_t start = s.find_first_not_of(" \t\r\n");
		if(start == std::string::npos) return "";
		size_t end = s.find_last_not_of(" \t\r\n");
		return s.substr(start, end - start + 1);
	}

	static std::string toUpper(const std::string& s)
	{
		std::string out = s;
		std::transform(out.begin(), out.end(), out.begin(),
			[](unsigned char c) { return (char)std::toupper(c); });
		return out;
	}

	static bool isAbsolutePathSimple(const std::string& p)
	{
		return !p.empty() && p[0] == '/';
	}

	static std::string joinPathSimple(const std::string& a, const std::string& b)
	{
		if(a.empty()) return b;
		if(b.empty()) return a;

		if(a.back() == '/' && b.front() == '/')
			return a + b.substr(1);
		if(a.back() != '/' && b.front() != '/')
			return a + "/" + b;
		return a + b;
	}

	static std::string normalizeRelative(const std::string& p)
	{
		if(p.rfind("./", 0) == 0)
			return p.substr(2);
		return p;
	}

	static std::string resolveThemePath(const std::string& themeDir, const std::string& maybeRelative)
	{
		if(maybeRelative.empty())
			return "";

		if(isAbsolutePathSimple(maybeRelative))
			return maybeRelative;

		return joinPathSimple(themeDir, normalizeRelative(maybeRelative));
	}

	// --- FIX: getDirContent puede devolver "nombre" o "/abs/ruta/nombre"
	static std::string baseNameSimple(const std::string& p)
	{
		auto pos = p.find_last_of('/');
		if(pos == std::string::npos) return p;
		return p.substr(pos + 1);
	}

	static std::string childPath(const std::string& parent, const std::string& entry)
	{
		if(entry.empty()) return "";
		if(isAbsolutePathSimple(entry)) return entry;
		return parent + "/" + entry;
	}

	// --- FIX: mkdir -p (createDirectory no siempre crea anidado)
	static void mkdirs(const std::string& dir)
	{
		if(dir.empty()) return;
		if(Utils::FileSystem::isDirectory(dir)) return;

		std::string cur;
		if(dir[0] == '/')
			cur = "/";

		std::stringstream ss(dir);
		std::string part;
		while(std::getline(ss, part, '/'))
		{
			if(part.empty()) continue;
			if(cur.size() > 1 && cur.back() != '/') cur += "/";
			cur += part;
			if(!Utils::FileSystem::isDirectory(cur))
				Utils::FileSystem::createDirectory(cur);
		}
	}

	static bool copyFileForce(const std::string& src, const std::string& dst)
	{
		std::ifstream in(src.c_str(), std::ios::binary);
		if(!in)
			return false;

		// Asegura carpeta destino (anidada)
		auto pos = dst.find_last_of('/');
		if(pos != std::string::npos)
		{
			std::string parent = dst.substr(0, pos);
			mkdirs(parent);
		}

		std::ofstream out(dst.c_str(), std::ios::binary | std::ios::trunc);
		if(!out)
			return false;

		out << in.rdbuf();
		return out.good();
	}

	static void parseValues(const std::string& str, std::vector<std::pair<std::string, std::string>>& out)
	{
		out.clear();
		std::stringstream ss(str);
		std::string item;

		while(std::getline(ss, item, ','))
		{
			item = trim(item);
			if(item.empty())
				continue;

			auto pos = item.find('|');
			if(pos == std::string::npos)
				out.emplace_back(item, item);
			else
			{
				std::string internal = trim(item.substr(0, pos));
				std::string label    = trim(item.substr(pos + 1));
				if(!internal.empty() && !label.empty())
					out.emplace_back(internal, label);
			}
		}
	}

	static std::map<std::string, std::map<std::string, std::string>>
	loadIniSections(const std::string& iniPath)
	{
		std::map<std::string, std::map<std::string, std::string>> data;

		std::ifstream file(iniPath.c_str());
		if(!file)
			return data;

		std::string line;
		std::string currentSection;

		while(std::getline(file, line))
		{
			line = trim(line);
			if(line.empty() || line[0] == ';' || line[0] == '#')
				continue;

			if(line.front() == '[' && line.back() == ']')
			{
				currentSection = trim(line.substr(1, line.size() - 2));
				continue;
			}

			auto pos = line.find('=');
			if(pos == std::string::npos)
				continue;

			std::string key = trim(line.substr(0, pos));
			std::string val = trim(line.substr(pos + 1));

			if(!currentSection.empty() && !key.empty())
				data[currentSection][key] = val;
		}

		return data;
	}

	static std::string readTopIniValue(const std::string& iniPath, const std::string& key)
	{
		std::ifstream in(iniPath.c_str());
		if(!in)
			return "";

		std::string line;

		while(std::getline(in, line))
		{
			std::string t = trim(line);

			if(!t.empty() && t.front() == '[')
				break;

			if(t.empty() || t[0] == ';' || t[0] == '#')
				continue;

			auto posEq = t.find('=');
			if(posEq == std::string::npos)
				continue;

			std::string thisKey = trim(t.substr(0, posEq));
			if(thisKey == key)
				return trim(t.substr(posEq + 1));
		}

		return "";
	}

	static void updateThemeIniValue(const std::string& iniPath, const std::string& key, const std::string& value)
	{
		std::ifstream in(iniPath.c_str());
		if(!in)
			return;

		std::vector<std::string> lines;
		std::string line;
		bool updatedAny = false;
		bool inTop = true;

		while(std::getline(in, line))
		{
			std::string t = trim(line);

			if(inTop && !t.empty() && t.front() == '[')
				inTop = false;

			if(inTop)
			{
				auto posEq = t.find('=');
				if(posEq != std::string::npos)
				{
					std::string thisKey = trim(t.substr(0, posEq));
					if(thisKey == key)
					{
						line = key + "=" + value;
						updatedAny = true;
					}
				}
			}

			lines.push_back(line);
		}
		in.close();

		if(!updatedAny)
		{
			size_t insertPos = lines.size();
			for(size_t i = 0; i < lines.size(); ++i)
			{
				std::string t = trim(lines[i]);
				if(!t.empty() && t.front() == '[')
				{
					insertPos = i;
					break;
				}
			}
			lines.insert(lines.begin() + insertPos, key + "=" + value);
		}

		std::ofstream out(iniPath.c_str(), std::ios::trunc);
		if(!out)
			return;

		for(const auto& l : lines)
			out << l << "\n";
	}

	static void ensureLayoutOptionIfLayoutFolderExists(const std::string& themeDir, std::vector<ThemeOption>& options)
	{
		const std::string layoutDir = themeDir + "/layout";
		if(!Utils::FileSystem::isDirectory(layoutDir))
			return;

		for(const auto& o : options)
			if(o.id == "layout")
				return;

		Utils::FileSystem::stringList lst = Utils::FileSystem::getDirContent(layoutDir);

		std::vector<std::string> layouts;
		for(const auto& entry : lst)
		{
			if(entry.empty()) continue;
			const std::string name = baseNameSimple(entry);
			if(name.empty() || name[0] == '.') continue;

			const std::string full = childPath(layoutDir, name);
			if(Utils::FileSystem::isDirectory(full))
				layouts.push_back(name);
		}

		if(layouts.empty())
			return;

		std::sort(layouts.begin(), layouts.end());

		ThemeOption layoutOpt;
		layoutOpt.id = "layout";
		layoutOpt.type = "select";
		layoutOpt.applyTo = "layout";
		layoutOpt.label = _("THEME LAYOUT");

		for(const auto& l : layouts)
			layoutOpt.values.emplace_back(l, l);

		std::string current = Settings::getInstance()->getString("ThemeLayout");
		if(current.empty())
		{
			const std::string iniPath = themeDir + "/theme.ini";
			current = readTopIniValue(iniPath, "LAYOUT");
		}

		layoutOpt.defaultValue = !current.empty() ? current : layouts.front();
		options.insert(options.begin(), layoutOpt);
	}

	static bool hasImageExt(const std::string& f)
	{
		auto lower = f;
		std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

		return (lower.size() >= 4 && lower.rfind(".png") == lower.size() - 4) ||
		       (lower.size() >= 4 && lower.rfind(".jpg") == lower.size() - 4) ||
		       (lower.size() >= 5 && lower.rfind(".jpeg") == lower.size() - 5);
	}

	static std::string stripExt(const std::string& f)
	{
		auto pos = f.find_last_of('.');
		if(pos == std::string::npos) return f;
		return f.substr(0, pos);
	}

	static void ensureAvatarOptionIfAvatarFolderExists(const std::string& themeDir, std::vector<ThemeOption>& options)
	{
		for(const auto& o : options)
			if(o.id == "avatar")
				return;

		const std::string avatarDir = themeDir + "/_inc/avatars";
		if(!Utils::FileSystem::isDirectory(avatarDir))
			return;

		Utils::FileSystem::stringList lst = Utils::FileSystem::getDirContent(avatarDir);

		std::vector<std::string> imgs;
		for(const auto& entry : lst)
		{
			if(entry.empty()) continue;
			const std::string name = baseNameSimple(entry);
			if(name.empty() || name[0] == '.') continue;

			const std::string full = childPath(avatarDir, name);
			if(Utils::FileSystem::isRegularFile(full) && hasImageExt(name))
				imgs.push_back(name);
		}

		if(imgs.empty())
			return;

		std::sort(imgs.begin(), imgs.end());

		ThemeOption opt;
		opt.id = "avatar";
		opt.type = "select";
		opt.applyTo = "theme_ini";
		opt.key = "AVATAR_PATH";
		opt.label = _("AVATAR");

		for(const auto& f : imgs)
		{
			const std::string relPath = "./_inc/avatars/" + f;
			const std::string nice    = stripExt(f);
			opt.values.emplace_back(relPath, nice);
		}

		const std::string iniPath = themeDir + "/theme.ini";
		std::string current = readTopIniValue(iniPath, opt.key);
		opt.defaultValue = !current.empty() ? current : opt.values.front().first;

		options.push_back(opt);
	}

	static std::vector<ThemeOption> loadThemeOptionsFromIni(const std::string& themeDir)
	{
		std::vector<ThemeOption> options;
		const std::string iniPath = themeDir + "/theme.ini";

		if(Utils::FileSystem::isRegularFile(iniPath))
		{
			std::ifstream file(iniPath.c_str());
			if(file)
			{
				ThemeOption current;
				std::string line;

				auto pushIfValid = [&options](const ThemeOption& opt)
				{
					if(opt.id.empty() || opt.type.empty())
						return;

					std::string t = opt.type;
					std::transform(t.begin(), t.end(), t.begin(), ::tolower);

					if(t == "select")
					{
						if(!opt.values.empty())
							options.push_back(opt);
					}
					else if(t == "input")
					{
						options.push_back(opt);
					}
				};

				while(std::getline(file, line))
				{
					line = trim(line);
					if(line.empty() || line[0] == ';' || line[0] == '#')
						continue;

					if(line.front() == '[' && line.back() == ']')
					{
						pushIfValid(current);
						current = ThemeOption();
						current.id = trim(line.substr(1, line.size() - 2));
						continue;
					}

					auto pos = line.find('=');
					if(pos != std::string::npos)
					{
						std::string key   = trim(line.substr(0, pos));
						std::string value = trim(line.substr(pos + 1));

						if(key == "label") current.label = value;
						else if(key == "type") current.type = value;
						else if(key == "path") current.path = value;
						else if(key == "apply_to" || key == "applyTo") current.applyTo = value;
						else if(key == "values") parseValues(value, current.values);
						else if(key == "default") current.defaultValue = value;
						else if(key == "key") current.key = value;
					}
				}

				pushIfValid(current);
			}
		}

		ensureLayoutOptionIfLayoutFolderExists(themeDir, options);
		ensureAvatarOptionIfAvatarFolderExists(themeDir, options);
		return options;
	}

	static bool endsWithXml(const std::string& f)
	{
		std::string lower = f;
		std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
		return (lower.size() >= 4 && lower.rfind(".xml") == lower.size() - 4);
	}

	static void listXmlRecursiveAbs(const std::string& baseDir, std::vector<std::string>& out)
	{
		if(!Utils::FileSystem::isDirectory(baseDir))
			return;

		Utils::FileSystem::stringList lst = Utils::FileSystem::getDirContent(baseDir);
		for(const auto& entry : lst)
		{
			if(entry.empty()) continue;
			const std::string name = baseNameSimple(entry);
			if(name.empty() || name[0] == '.') continue;

			const std::string abs = childPath(baseDir, name);

			if(Utils::FileSystem::isDirectory(abs))
			{
				listXmlRecursiveAbs(abs, out);
				continue;
			}

			if(Utils::FileSystem::isRegularFile(abs) && endsWithXml(name))
				out.push_back(abs);
		}
	}

	static std::string makeDstFromLayoutRel(const std::string& layoutAbsDir,
	                                       const std::string& themeDir,
	                                       const std::string& xmlAbs)
	{
		std::string rel = xmlAbs;

		if(rel.rfind(layoutAbsDir, 0) == 0)
			rel = rel.substr(layoutAbsDir.size());

		while(!rel.empty() && rel.front() == '/')
			rel.erase(rel.begin());

		return themeDir + "/" + rel;
	}

	static bool applyLayoutByCopy(Window* win, const std::string& themeDir, const std::string& layoutId)
	{
		const std::string iniPath = themeDir + "/theme.ini";
		std::map<std::string, std::map<std::string, std::string>> ini;
		if(Utils::FileSystem::isRegularFile(iniPath))
			ini = loadIniSections(iniPath);

		auto defPath = [&](const std::string& fileName) -> std::string
		{
			return "./layout/" + layoutId + "/" + fileName;
		};

		const std::string section = "layout_" + layoutId;

		std::string srcTheme  = defPath("theme.xml");
		std::string srcSwatch = defPath("Swatch.xml");
		std::string srcAvatar = defPath("avatar.xml");
		std::string srcUser   = defPath("user.xml");
		std::string srcStart  = defPath("start.xml");

		if(!ini.empty() && ini.count(section))
		{
			auto& s = ini[section];
			if(s.count("theme"))  srcTheme  = s["theme"];
			if(s.count("swatch")) srcSwatch = s["swatch"];
			if(s.count("avatar")) srcAvatar = s["avatar"];
			if(s.count("user"))   srcUser   = s["user"];
			if(s.count("start"))  srcStart  = s["start"];
		}

		const std::string absTheme  = resolveThemePath(themeDir, srcTheme);
		const std::string absSwatch = resolveThemePath(themeDir, srcSwatch);
		const std::string absAvatar = resolveThemePath(themeDir, srcAvatar);
		const std::string absUser   = resolveThemePath(themeDir, srcUser);
		const std::string absStart  = resolveThemePath(themeDir, srcStart);

		bool okAny = false;
		bool okAll = true;

		auto doCopy = [&](const std::string& srcAbs, const std::string& dstName)
		{
			if(srcAbs.empty())
				return;

			if(!Utils::FileSystem::isRegularFile(srcAbs))
				return;

			const std::string dstAbs = themeDir + "/" + dstName;
			if(!copyFileForce(srcAbs, dstAbs))
				okAll = false;
			else
				okAny = true;
		};

		// 1) Copia clásica
		doCopy(absTheme,  "theme.xml");
		doCopy(absSwatch, "Swatch.xml");
		doCopy(absAvatar, "avatar.xml");
		doCopy(absUser,   "user.xml");
		doCopy(absStart,  "start.xml");

		// 2) Copia genérica recursiva
		const std::string layoutAbsDir = themeDir + "/layout/" + layoutId;
		std::vector<std::string> xmls;
		listXmlRecursiveAbs(layoutAbsDir, xmls);

		if(!xmls.empty())
		{
			std::sort(xmls.begin(), xmls.end());
			for(const auto& xmlAbs : xmls)
			{
				const std::string dstAbs = makeDstFromLayoutRel(layoutAbsDir, themeDir, xmlAbs);
				if(copyFileForce(xmlAbs, dstAbs))
					okAny = true;
				else
					okAll = false;
			}
		}

		if(!okAny)
		{
			if(win)
				win->pushGui(new GuiMsgBox(win, _("NO XML FILES FOUND TO COPY FOR THIS LAYOUT."), _("BACK")));
			return false;
		}

		if(!okAll)
		{
			if(win)
				win->pushGui(new GuiMsgBox(win, _("SOME FILES COULD NOT BE COPIED. CHECK PERMISSIONS AND PATHS."), _("BACK")));
		}

		return true;
	}

	static std::string getOutputKeyForOption(const ThemeOption& opt)
	{
		if(!opt.key.empty())
			return opt.key;

		if(!opt.applyTo.empty() && opt.applyTo != "theme_ini")
			return opt.applyTo;

		return opt.id;
	}

	static void applyThemeOption(Window* win, const std::string& themeDir, const ThemeOption& opt, const std::string& value)
	{
		if(opt.id.empty() || value.empty())
			return;

		const std::string iniPath = themeDir + "/theme.ini";

		if(opt.applyTo == "layout" || opt.id == "layout")
		{
			Settings::getInstance()->setString("ThemeLayout", value);
			Settings::getInstance()->saveFile();

			updateThemeIniValue(iniPath, "LAYOUT", value);
			applyLayoutByCopy(win, themeDir, value);

			// ✅ Mostrar “splash” simple y reiniciar el proceso (limpia VRAM/caches)
			if (win)
				win->renderLoadingScreen("Applying theme...");

			requestRestart("theme layout change");

			SDL_Event ev;
			ev.type = SDL_QUIT;
			SDL_PushEvent(&ev);
			return; // IMPORTANT: no reloadAll()
		}
		else
		{
			const std::string outKey = getOutputKeyForOption(opt);

			updateThemeIniValue(iniPath, outKey, value);

			const std::string upperKey = toUpper(outKey);
			if(upperKey != outKey)
				updateThemeIniValue(iniPath, upperKey, value);
		}

		auto vc = ViewController::get();
		if(vc != nullptr)
			vc->reloadAll();
	}

	class GuiThemeOptionSelect : public GuiComponent
	{
	public:
		GuiThemeOptionSelect(Window* window,
		                     const std::string& themeDir,
		                     const ThemeOption& opt,
		                     const std::string& title)
			: GuiComponent(window)
			, mMenu(window, title.c_str())
			, mThemeDir(themeDir)
			, mOption(opt)
		{
			for(const auto& v : mOption.values)
			{
				ComponentListRow row;

				auto text = std::make_shared<TextComponent>(
					mWindow,
					v.second,
					Font::get(FONT_SIZE_MEDIUM),
					0x777777FF
				);

				text->setColor(0x777777FF);
				row.addElement(text, true);

				row.makeAcceptInputHandler([this, v]()
				{
					applyThemeOption(mWindow, mThemeDir, mOption, v.first);
					delete this;
				});

				mMenu.addRow(row);
			}

			addChild(&mMenu);

			setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
			mMenu.setPosition(
				(mSize.x() - mMenu.getSize().x()) / 2.0f,
				Renderer::getScreenHeight() * 0.15f);
		}

		bool input(InputConfig* config, Input input) override
		{
			if(GuiComponent::input(config, input))
				return true;

			if(config->isMappedTo("b", input) && input.value != 0)
			{
				delete this;
				return true;
			}
			return false;
		}

		std::vector<HelpPrompt> getHelpPrompts() override
		{
			std::vector<HelpPrompt> prompts;
			prompts.push_back(HelpPrompt("a", _("SELECT")));
			prompts.push_back(HelpPrompt("b", _("BACK")));
			return prompts;
		}

		HelpStyle getHelpStyle() override
		{
			HelpStyle style;
			auto vc = ViewController::get();
			if(vc)
			{
				auto system = vc->getState().getSystem();
				if(system)
					style.applyTheme(system->getTheme(), "system");
			}
			return style;
		}

	private:
		MenuComponent mMenu;
		std::string   mThemeDir;
		ThemeOption   mOption;
	};

	static void openThemeInput(Window* win, const std::string& themeDir, const ThemeOption& opt, const std::string& title)
	{
		if(win == nullptr)
			return;

		const std::string iniPath = themeDir + "/theme.ini";
		const std::string outKey  = getOutputKeyForOption(opt);
		const std::string upperKey = toUpper(outKey);

		std::string current = readTopIniValue(iniPath, outKey);
		if(current.empty() && upperKey != outKey)
			current = readTopIniValue(iniPath, upperKey);

		if(current.empty())
			current = opt.defaultValue;

		win->pushGui(new GuiTextEditKeyboardPopup(
			win,
			title,
			current,
			[win, themeDir, opt](const std::string& newValue)
			{
				if(newValue.empty())
					return;

				applyThemeOption(win, themeDir, opt, newValue);
			},
			false
		));
	}
}

GuiThemeOptions::GuiThemeOptions(Window* window)
	: GuiComponent(window)
	, mMenu(window, _("THEME OPTIONS").c_str())
{
	std::string themeSet = Settings::getInstance()->getString("ThemeSet");
	std::string themeDir;

	if(!themeSet.empty())
		themeDir = Utils::FileSystem::getHomePath() + "/.emulationstation/themes/" + themeSet;

	auto options = loadThemeOptionsFromIni(themeDir);

	if(options.empty())
	{
		ComponentListRow row;
		row.addElement(
			std::make_shared<TextComponent>(
				mWindow,
				_("NO THEME OPTIONS AVAILABLE"),
				Font::get(FONT_SIZE_MEDIUM),
				0x777777FF),
			true);
		mMenu.addRow(row);
	}
	else
	{
		for(const auto& opt : options)
		{
			std::string entryLabel = !opt.label.empty() ? opt.label : opt.id;

			ComponentListRow row;

			auto text = std::make_shared<TextComponent>(
				mWindow,
				entryLabel,
				Font::get(FONT_SIZE_MEDIUM),
				0x777777FF
			);
			text->setColor(0x777777FF);

			row.addElement(text, true);

			row.makeAcceptInputHandler([this, themeDir, opt, entryLabel]
			{
				std::string t = opt.type;
				std::transform(t.begin(), t.end(), t.begin(), ::tolower);

				if(t == "select" && !opt.values.empty())
				{
					mWindow->pushGui(new GuiThemeOptionSelect(mWindow, themeDir, opt, entryLabel));
				}
				else if(t == "input")
				{
					openThemeInput(mWindow, themeDir, opt, entryLabel);
				}
				else
				{
					std::string msg = entryLabel + "\n\n" + _("(Feature not yet implemented)");
					mWindow->pushGui(new GuiMsgBox(mWindow, msg, _("BACK")));
				}
			});

			mMenu.addRow(row);
		}
	}

	addChild(&mMenu);

	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	mMenu.setPosition(
		(mSize.x() - mMenu.getSize().x()) / 2.0f,
		Renderer::getScreenHeight() * 0.15f);
}

bool GuiThemeOptions::input(InputConfig* config, Input input)
{
	if(GuiComponent::input(config, input))
		return true;

	if(config->isMappedTo("b", input) && input.value != 0)
	{
		delete this;
		return true;
	}
	return false;
}

std::vector<HelpPrompt> GuiThemeOptions::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt("b", _("BACK")));
	return prompts;
}

HelpStyle GuiThemeOptions::getHelpStyle()
{
	HelpStyle style;
	auto vc = ViewController::get();
	if(vc)
	{
		auto system = vc->getState().getSystem();
		if(system)
			style.applyTheme(system->getTheme(), "system");
	}
	return style;
}

