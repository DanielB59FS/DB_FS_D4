// The Bullet system is responsible for inflicting damage and cleaning up bullets
#ifndef BULLETLOGIC_H
#define BULLETLOGIC_H

// Contains our global game settings
#include "../GameConfig.h"
#include "../Entities/BulletData.h"

namespace TeamYellow
{
	struct Bullet;
	class BulletLogic
	{
		flecs::query<Bullet> bulletQuery;
		// shared connection to the main ECS engine
		std::shared_ptr<flecs::world> game;
		// non-ownership handle to configuration settings
		std::weak_ptr<const GameConfig> gameConfig;
	public:
		// attach the required logic to the ECS 
		bool Init(std::shared_ptr<flecs::world> _game,
			std::weak_ptr<const GameConfig> _gameConfig);
		// control if the system is actively running
		bool Activate(bool runSystem);
		// release any resources allocated by the system
		bool Shutdown();
		void Clear();
	};

};

#endif