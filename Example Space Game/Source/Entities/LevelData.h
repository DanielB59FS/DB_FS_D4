#ifndef LEVELDATA_H
#define LEVELDATA_H

#include "../GameConfig.h"
#include "../Components/Identification.h"

namespace TeamYellow
{
	class LevelData
	{
		struct LevelReadOnlyData
		{
			float       spawnDelay;
			uint32_t    spawnCount; // NOTE: We'll change this to an array with an element per Enemy Type
			float       environmentObjectScale;
			float       halfWidth;
			uint32_t    floorMeshID;
			uint32_t    skyBoxID;
			uint32_t    meshIDs[5];
		};
		LevelReadOnlyData levels[LEVEL_COUNT];

	public:
		GW::MATH2D::GMatrix2D matrixMath;
		bool Init(std::shared_ptr<flecs::world> _game,
			std::weak_ptr<const GameConfig> _gameConfig);
		// Load required entities and/or prefabs into the ECS 
		bool Load(std::shared_ptr<flecs::world> _game,
			LevelState _level);
		// Unload the entities/prefabs from the ECS
		bool Unload(std::shared_ptr<flecs::world> _game);
	private:
		void InitLevelReadOnlyData(std::shared_ptr<flecs::world> _game,
			const ini::IniSection& _section,
			LevelReadOnlyData& _data);
	};
};




#endif