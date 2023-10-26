#ifndef BombData_H
#define BombData_H

#include "../GameConfig.h"
#include "../../flecs-3.1.4/flecs.h"
#include "../../gateware-main/Gateware.h"
#include <memory>

namespace ESG {
	class BombData {
	public:
		bool Load(std::shared_ptr<flecs::world> _game,
				  std::weak_ptr<const GameConfig> _gameConfig,
				  GW::AUDIO::GAudio _audioEngine);

		bool Unload(std::shared_ptr<flecs::world> _game);
	};

};

#endif
