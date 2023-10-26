#ifndef PLAYEVENTS_H
#define PLAYEVENTS_H

namespace TeamYellow
{
	enum PLAY_EVENT {
		ENEMY_DESTROYED,
		PLAYER_DESTROYED,   // Triggered when the Player loses a life
		PLAYER_DEFEATED,    // Triggered when the Player loses all lives
		LEVEL_CLEARED,
		EVENT_COUNT,
		GAME_PAUSED,
		GAME_RESUME,
		GAME_STARTED,
		BOMB_USED
	};
	struct PLAY_EVENT_DATA {
		flecs::id entity_id; // which entity was affected?
	};
}

#endif