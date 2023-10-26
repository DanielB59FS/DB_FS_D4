#include <random>
#include "BombLogic.h"
#include "../Components/Identification.h"
#include "../Components/Gameplay.h"

using namespace TeamYellow;

bool ESG::BombLogic::Init(std::shared_ptr<flecs::world> _game,
						  std::weak_ptr<const GameConfig> _gameConfig) {
	game = _game;
	gameConfig = _gameConfig;

	game->system<Bullet, Damage>("Bomb System");

	return true;
}

// Free any resources used to run this system
bool ESG::BombLogic::Shutdown() {
	game->entity("Bomb System").destruct();
	game.reset();
	gameConfig.reset();
	return true;
}

// Toggle if a system's Logic is actively running
bool ESG::BombLogic::Activate(bool runSystem) {
	if (runSystem) {
		game->entity("Bomb System").enable();
	}
	else {
		game->entity("Bomb System").disable();
	}
	return false;
}
