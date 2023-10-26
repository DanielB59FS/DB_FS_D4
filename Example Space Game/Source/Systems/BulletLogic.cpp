#include <random>
#include "BulletLogic.h"
#include "../Components/Identification.h"
#include "../Components/Physics.h"
#include "../Components/Gameplay.h"

using namespace TeamYellow;

// Connects logic to traverse any players and allow a controller to manipulate them
bool BulletLogic::Init(	std::shared_ptr<flecs::world> _game,
							std::weak_ptr<const GameConfig> _gameConfig)
{
	// save a handle to the ECS & game settings
	game = _game;
	gameConfig = _gameConfig;

	// destroy any bullets that have the CollidedWith relationship
	game->system<Collidable, Damage>("Bullet System")
		.each([](flecs::entity e, Collidable, Damage &d) {
		bool collided = false;
		// damage anything we come into contact with
		e.each<CollidedWith>([&](flecs::entity hit) {
			if (!hit.has<Bullet>() && e.get<AlliedWith>()->faction != hit.get<AlliedWith>()->faction) {
				collided = true;
				if (hit.has<Health>()) {
  					int current = hit.get<Health>()->value;
					hit.set<Health>({ current - d.value });
					// reduce the amount of hits but the charged shot
					if (e.has<ChargedShot>() && hit.get<Health>()->value <= 0) 
					{
						int md_count = e.get<ChargedShot>()->max_destroy;
						e.set<ChargedShot>({ md_count - 1 });
					}
				}
			}
		});
		// if you have collidedWith realtionship then be destroyed
		if (collided) {
			if (e.has<ChargedShot>()) {
			
				if(e.get<ChargedShot>()->max_destroy <= 0)
					e.destruct();
			}
			else {
				// play hit sound
				e.destruct();
			}
		}
	});
	bulletQuery = _game->query_builder<Bullet>().build();
	return true;
}

// Free any resources used to run this system
bool BulletLogic::Shutdown()
{
	game->entity("Bullet System").destruct();
	// invalidate the shared pointers
	game.reset();
	gameConfig.reset();
	return true;
}

void BulletLogic::Clear()
{
	bulletQuery.each([](flecs::entity e, Bullet) { e.destruct(); });
}

// Toggle if a system's Logic is actively running
bool BulletLogic::Activate(bool runSystem)
{
	if (runSystem) {
		game->entity("Bullet System").enable();
	}
	else {
		game->entity("Bullet System").disable();
	}
	return false;
}
