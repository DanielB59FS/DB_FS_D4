// define all ECS components related to gameplay
#ifndef GAMEPLAY_H
#define GAMEPLAY_H

namespace TeamYellow
{
	struct Damage           { int   value; };
	struct Health           { int   value; };
	struct Firerate         { float value; };
	struct Cooldown         { float value; float initial; };
	struct Points           { int   value; };
	struct GameplayStats    { int   score; };
	struct PlayerStats      {
		int lives;
		int specials;
		int scoreToNextLifeReward;
		int scoreToNextSpecialReward;
	};
	struct EnemyStats      {
		float startY;
		float accMin;
		float accMax;
	};
	struct LevelStats       {
		float       spawnDelay;
		uint32_t    startingSpawnCount;
		float       halfWidth;
	};
	// gameplay tags (states)
	struct Firing {};
	struct Charging {};

	// powerups
	struct ChargedShot { int max_destroy; };
};

#endif