// This class creates all types of enemy prefabs
#ifndef ENEMYDATA_H
#define ENEMYDATA_H

// Contains our global game settings
#include "../GameConfig.h"

namespace TeamYellow
{
	class EnemyData
	{
	public:
		// Load required entities and/or prefabs into the ECS 
		bool Load(	std::shared_ptr<flecs::world> _game,
					std::weak_ptr<const GameConfig> _gameConfig,
					GW::AUDIO::GAudio _audioEngine);
		// Unload the entities/prefabs from the ECS
		bool Unload(std::shared_ptr<flecs::world> _game);
	};
	
};

#endif