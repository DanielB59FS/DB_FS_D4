// define all ECS components related to identification
#ifndef IDENTIFICATION_H
#define IDENTIFICATION_H

namespace TeamYellow
{
    enum Faction {
        NEUTRAL,
        PLAYER,
        ENEMY
    };
	enum GameState {
		STATE_UNDEFINED,
		STATE_START,
		STATE_GAMEPLAY,
		STATE_PAUSE,
		STATE_LEVEL_COMPLETE,
		STATE_DESTROYED,
		STATE_GAMEOVER
	};
	enum LevelState {
		LEVEL_START,
		LEVEL_ONE,
		LEVEL_TWO,
		LEVEL_THREE,
		LEVEL_COUNT,
		LEVEL_ENDLESS_ONE,
		LEVEL_ENDLESS_TWO,
		LEVEL_ENDLESS_THREE
	};
	struct Bomb {};
	struct Player {};
	struct Bullet {};
	struct Enemy {};
	struct Lives {};
	struct AlliedWith { Faction faction; };
	struct GameStateManager { GameState state; GameState target;};
	struct StartGame{};
	struct ControllerID {
		unsigned index = 0;
	};
	struct Item {
		unsigned count = 0;
	};
};

#endif