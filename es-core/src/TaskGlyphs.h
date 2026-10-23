#pragma once
#ifndef ES_CORE_TASK_GLYPHS_H
#define ES_CORE_TASK_GLYPHS_H

// Shared lightweight Font Awesome 4 glyphs for ES-X task notifications
// and compact Theme Browser / X-TRAS rows.
// Render with Font::get(size, TaskGlyphs::FONT_PATH). No PNG required.
namespace TaskGlyphs
{
	static const char* const FONT_PATH = ":/fonts/fontawesome-webfont.ttf";

	static const char* const SEARCH = u8"\uf002";   // search: native metadata scraper
	static const char* const GATHER = u8"\uf0ed";   // cloud-download: Skyscraper gather
	static const char* const GAMELIST = u8"\uf0ca"; // list-ul: generate gamelist
	static const char* const DOWNLOAD = u8"\uf019";// download: themes/ports
	static const char* const INSTALL = u8"\uf013"; // cog: install
	static const char* const UPDATE = u8"\uf021";  // refresh: update
	static const char* const DONE = u8"\uf058";    // check-circle: success
	static const char* const ERROR = u8"\uf071";   // exclamation-triangle: failure
	static const char* const CANCEL = u8"\uf057";  // times-circle: cancelled
}

#endif // ES_CORE_TASK_GLYPHS_H
