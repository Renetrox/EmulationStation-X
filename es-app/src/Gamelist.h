#pragma once
#ifndef ES_APP_GAME_LIST_H
#define ES_APP_GAME_LIST_H

class SystemData;

// Loads gamelist.xml data into a SystemData.
void parseGamelist(SystemData* system);

// Loads supplemental PortMaster gameinfo.xml metadata without overriding
// fields explicitly present in gamelist.xml.
void parsePortMasterGameInfo(SystemData* system);

// Writes currently loaded metadata for a SystemData to gamelist.xml.
void updateGamelist(SystemData* system);

#endif // ES_APP_GAME_LIST_H
