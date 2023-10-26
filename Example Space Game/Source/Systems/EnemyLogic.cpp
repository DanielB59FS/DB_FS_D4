#include <random>
#include "EnemyLogic.h"
#include "../Components/Identification.h"
#include "../Components/Physics.h"
#include "../Components/Gameplay.h"
#include "../Events/Playevents.h"
#include "../Helper/AudioHelper.h"
#include "../Entities/Prefabs.h"

using namespace TeamYellow;

// Connects logic to traverse any players and allow a controller to manipulate them
bool EnemyLogic::Init(std::shared_ptr<flecs::world> _game,
	std::weak_ptr<const GameConfig> _gameConfig,
	GW::CORE::GEventGenerator _eventPusher)
{
	// save a handle to the ECS & game settings
	game = _game;
	gameConfig = _gameConfig;
	eventPusher = _eventPusher;
	
	// destroy any bullets that have the CollidedWith relationship
	enemySystem = game->system<Enemy, Health>("Enemy System")
		.no_readonly()
		.each([this](flecs::entity e, Enemy, Health& h) {
			// if you have no health left be destroyed
			if (e.get<Health>()->value <= 0) {
				e.destruct();
				PLAY_EVENT_DATA x;
				x.entity_id = e;
				GW::GEvent explode;
				explode.Write(PLAY_EVENT::ENEMY_DESTROYED, x);
				PlaySound("Enemy Type1");
 				eventPusher.Push(explode);
				return;
			}
			float value = e.get<Cooldown>()->value - e.delta_time();
			float initial = e.get<Cooldown>()->initial;
			if (0 >= value) {
				Position offset = *e.get<Position>();
				offset.value.y += 0.05f;
				FireLasers(e.world(), offset, e.get<Orientation>());
				value = initial;
			}
			e.set<Cooldown>({ value, initial });
	});
	enemyQuery = _game->query_builder<Enemy>().build();
	return true;
}

// Free any resources used to run this system
bool EnemyLogic::Shutdown()
{
	game->entity("Enemy System").destruct();
	// invalidate the shared pointers
	game.reset();
	gameConfig.reset();
	return true;
}

// Toggle if a system's Logic is actively running
bool EnemyLogic::Activate(bool runSystem)
{
	if (runSystem) {
		enemySystem.enable();
	}
	else {
		enemySystem.disable();
	}
	return false;
}

void UpdateBullet(flecs::entity& b) {
	auto velocity = b.get<Velocity>()->value;
	GW::MATH2D::GVector2D::Scale2F(velocity, -1, velocity);
	b.set<Velocity>({ velocity });
}

// play sound and launch two laser rounds
bool EnemyLogic::FireLasers(flecs::world& stage, Position origin, const Orientation* orient) {
	// Grab the prefab for a laser round
	flecs::entity bullet;
	RetreivePrefab("Lazer Bullet", bullet);
	Velocity v = *bullet.get<Velocity>();
	v.value.y *= orient->target.data[0] * -1;

	stage.defer_suspend();
	origin.value.x -= 2.f;
	auto laserLeft = stage.entity().is_a(bullet)
		.set<Position>(origin)
		.set<Velocity>(v)
		.set<AlliedWith>({ ENEMY });
	origin.value.x += 2.f;
	auto laserRight = stage.entity().is_a(bullet)
		.set<Position>(origin)
		.set<Velocity>(v)
		.set<AlliedWith>({ ENEMY });
	stage.defer_resume();

	UpdateBullet(laserLeft);
	UpdateBullet(laserRight);

	// play the sound of the Lazer prefab
	GW::AUDIO::GSound shoot = *bullet.get<GW::AUDIO::GSound>();
	shoot.Play();

	return true;
}

void EnemyLogic::Clear()
{
	enemyQuery.each([](flecs::entity e, Enemy) { e.destruct(); });
}
