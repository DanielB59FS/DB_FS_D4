#include "GameConfig.h"
#include <filesystem>
using namespace std::chrono_literals;

GameConfig::GameConfig() : ini::IniFile() 
{
	// the default game config file is central to the game's data-driven behavior
	// its a bit extreme, but lets abort the program if we can't find it
	// a more reasonable solution would be to write out some default values here

	// this is really slick but it does require C++17
	const char* defaults = "../defaults.ini";
	const char* saved ="../saved.ini";
	// if they both exist choose the newest one
#if DEV_BUILD
	if (std::filesystem::exists(defaults)) {
#else
	if (std::filesystem::exists(defaults) && 
		std::filesystem::exists(saved)) {
		// Load the newer file
		auto dtime = std::filesystem::last_write_time(defaults);
		auto stime = std::filesystem::last_write_time(saved);
		if (dtime > stime)
			(*this).load("../defaults.ini"); // defaults were tweaked
		else
			(*this).load("../saved.ini"); // what happens most of the time
	}
	else if (std::filesystem::exists(defaults)) { // probably the first run after install
#endif
		(*this).load("../defaults.ini");
	}
	else { // the default file is missing or corrupted, this is bad
		std::abort(); // a more graceful approach would be to overwrite defaults.ini
	}
}

GameConfig::~GameConfig() 
{
	// Save current state of .ini to disk
	// Could be used for persisting user prefrences, highscores, savegames etc...
	(*this).save("../saved.ini");
}