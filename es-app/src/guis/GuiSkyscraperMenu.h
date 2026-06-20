#pragma once
#ifndef ES_APP_GUIS_GUI_SKYSCRAPER_MENU_H
#define ES_APP_GUIS_GUI_SKYSCRAPER_MENU_H

#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "HelpStyle.h"
#include "HelpPrompt.h"

#include <string>
#include <vector>

class GuiSkyscraperMenu : public GuiComponent
{
public:
	GuiSkyscraperMenu(Window* window);
	~GuiSkyscraperMenu() {}

	bool input(InputConfig* config, Input input) override;
	std::vector<HelpPrompt> getHelpPrompts() override;
	HelpStyle getHelpStyle() override;

private:
	MenuComponent mMenu;

	void runGather();
	void runGenerate();
	void openMaintenance();
	void openLog();

	std::string getCurrentSystem();
	std::string getCurrentLanguage();

	int launchScript(const std::string& action);
};

#endif
