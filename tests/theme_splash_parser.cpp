// Native splash-view parser regression check.
#include "ThemeData.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <cstdio>

static void writeFile(const std::string& path, const std::string& text)
{
	std::ofstream file(path.c_str());
	file << text;
}

int main(int argc, char** argv)
{
	assert(argc == 2 || argc == 3);
	const std::string path = std::string(argv[1]) + "/splash-test.xml";

	writeFile(path,
		"<theme><formatVersion>7</formatVersion>"
		"<view name='splash'>"
		"<splash name='splash'><pos>0 0</pos><origin>0 0</origin><size>1 1</size><minSize>1 1</minSize><backgroundColor>030303</backgroundColor></splash>"
		"<image name='background'><path>./missing-is-allowed-in-parser.png</path></image>"
		"<image name='progressbar'><pos>0.05 0.86</pos><size>0.9 0.016</size>"
		"<color>6060607F</color></image>"
		"<image name='progressbar:active'><tile>true</tile><color>4a8d35</color></image>"
		"<text name='label' extra='true'><fontSize>0.023</fontSize>"
		"<color>000000</color><glowColor>222222</glowColor><glowSize>1</glowSize><glowOffset>1</glowOffset><verticalAlignment>center</verticalAlignment><forceUppercase>true</forceUppercase></text>"
		"</view></theme>");

	ThemeData theme;
	theme.loadFile({}, path);
	assert(theme.hasView("splash"));

	auto splash = theme.getElement("splash", "splash", "splash");
	assert(splash && splash->get<unsigned int>("backgroundColor") == 0x030303FF);

	auto progress = theme.getElement("splash", "progressbar", "image");
	assert(progress);
	assert(progress->get<Vector2f>("pos") == Vector2f(0.05f, 0.86f));
	assert(progress->get<Vector2f>("size") == Vector2f(0.9f, 0.016f));

	auto active = theme.getElement("splash", "progressbar:active", "image");
	assert(active && active->get<bool>("tile"));

	auto label = theme.getElement("splash", "label", "text");
	assert(label && label->extra && label->get<bool>("forceUppercase"));
	assert(label->get<std::string>("verticalAlignment") == "center");
	assert(label->get<unsigned int>("glowColor") == 0x222222FF);

	// PlayStation-X style regression: subset variables from theme.ini,
	// selected colorset variables, ifSubset filtering, scalar maxSize,
	// unresolved system includes, and PNG/SVG path fallback.
	const std::string dir = std::string(argv[1]);
	const std::string psxTheme = dir + "/psx-theme.xml";
	const std::string psxSplash = dir + "/psx-splash.xml";
	const std::string blackVars = dir + "/black.xml";
	const std::string blueVars = dir + "/blue.xml";
	const std::string iniPath = dir + "/theme.ini";
	const std::string backgroundPath = dir + "/blue-background.jpg";
	const std::string logoPng = dir + "/EmulationStation.png";
	const std::string tipPng = dir + "/tip.png";

	writeFile(blackVars,
		"<theme><formatVersion>7</formatVersion><variables>"
		"<splash.labelColor>111111</splash.labelColor>"
		"</variables></theme>");
	writeFile(blueVars,
		"<theme><formatVersion>7</formatVersion><variables>"
		"<splash.labelColor>ffffff</splash.labelColor>"
		"</variables></theme>");
	writeFile(psxTheme,
		"<theme><formatVersion>7</formatVersion>"
		"<subset name='colorset'>"
		"<include name='black'>./black.xml</include>"
		"<include name='blue'>./blue.xml</include>"
		"</subset>"
		"<subset name='frontend'>"
		"<include name='Batocera'/>"
		"<include name='EmulationStation'/>"
		"</subset>"
		"<subset name='system-avatar'>"
		"<include if='{global.cheevos} == true' name='cheevos'/>"
		"<include name='psx-logo'/>"
		"</subset>"
		"<include>./views/${system.theme}.xml</include>"
		"<view name='system'><unknown name='should-not-be-parsed'/></view>"
		"</theme>");
	writeFile(iniPath,
		"subset.colorset=blue\n"
		"subset.frontend=EmulationStation\n");
	writeFile(backgroundPath, "fixture");
	writeFile(logoPng, "fixture");
	writeFile(tipPng, "fixture");

	writeFile(psxSplash,
		"<theme><formatVersion>7</formatVersion>"
		"<include>${themePath}/psx-theme.xml</include>"
		"<view name='splash'>"
		"<text name='label'><color>${splash.labelColor}</color></text>"
		"<image name='background'><linearSmooth>true</linearSmooth>"
		"<path>${themePath}/${subset.colorset}-background.jpg</path></image>"
		"<image name='logo' extra='true'>"
		"<path>${themePath}/${subset.frontend}.png</path>"
		"<path>${themePath}/${subset.frontend}.svg</path></image>"
		"<image name='tip' extra='static' ifSubset='colorset:blue'>"
		"<maxSize>0.35</maxSize><path>${themePath}/tip.png</path></image>"
		"<image name='hidden' extra='true' ifSubset='colorset:black'>"
		"<path>${themePath}/tip.png</path></image>"
		"</view></theme>");

	ThemeData psx;
	psx.loadFile({{"themePath", dir}}, psxSplash, "splash");
	assert(psx.hasView("splash"));

	auto psxLabel = psx.getElement("splash", "label", "text");
	assert(psxLabel && psxLabel->get<unsigned int>("color") == 0xFFFFFFFF);

	auto psxBackground = psx.getElement("splash", "background", "image");
	assert(psxBackground);
	assert(psxBackground->get<std::string>("path") == backgroundPath);
	assert(psxBackground->get<bool>("linearSmooth"));

	auto psxLogo = psx.getElement("splash", "logo", "image");
	assert(psxLogo && psxLogo->extra);
	assert(psxLogo->get<std::string>("path") == logoPng);

	auto psxTip = psx.getElement("splash", "tip", "image");
	assert(psxTip && psxTip->extra);
	assert(psxTip->get<Vector2f>("maxSize") == Vector2f(0.35f, 0.35f));
	assert(psx.getElement("splash", "hidden", "image") == nullptr);

	if (argc == 3)
	{
		ThemeData external;
		external.loadFile({}, argv[2]);
		assert(external.hasView("splash"));
		std::cout << "External splash parsed successfully\n";
	}

	std::remove(path.c_str());
	std::remove(psxTheme.c_str());
	std::remove(psxSplash.c_str());
	std::remove(blackVars.c_str());
	std::remove(blueVars.c_str());
	std::remove(iniPath.c_str());
	std::remove(backgroundPath.c_str());
	std::remove(logoPng.c_str());
	std::remove(tipPng.c_str());
	std::cout << "Native splash ThemeData checks passed\n";
}
