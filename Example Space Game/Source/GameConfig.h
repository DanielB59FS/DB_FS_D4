#ifndef GAMECONFIG_H
#define GAMECONFIG_H

// Loads the existing configuration file, creates a default one if none exists
class GameConfig : public ini::IniFile // for const correctness use ".at()" on read
{ 
public:
	// constructor loads game settings, writes defaults if none exist
	GameConfig();
	// destructor saves current game settings between plays
	virtual ~GameConfig();
};

#endif
