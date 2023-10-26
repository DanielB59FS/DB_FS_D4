#ifndef BombLOGIC_H
#define BombLOGIC_H

#include "../GameConfig.h"
#include "../Entities/BombData.h"

namespace ESG {
	class BombLogic {
		std::shared_ptr<flecs::world> game;
		std::weak_ptr<const GameConfig> gameConfig;
	public:
		bool Init(std::shared_ptr<flecs::world> _game,
				  std::weak_ptr<const GameConfig> _gameConfig);
		bool Activate(bool runSystem);
		bool Shutdown();
	};

};

#endif
