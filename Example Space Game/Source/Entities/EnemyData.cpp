#include "EnemyData.h"
#include "Prefabs.h"

#include "../Components/Identification.h"
#include "../Components/Visuals.h"
#include "../Components/Physics.h"
#include "../Components/Gameplay.h"

#include "../Systems/RenderLogic.h"

using namespace TeamYellow;

bool EnemyData::Load(	std::shared_ptr<flecs::world> _game,
							std::weak_ptr<const GameConfig> _gameConfig,
							GW::AUDIO::GAudio _audioEngine)
{
	// Grab init settings for players
	std::shared_ptr<const GameConfig> readCfg = _gameConfig.lock();

	// Create prefab for lazer weapon
	// other attributes
	float scale = (*readCfg).at("Enemy1").at("scale").as<float>();
	float angle = (*readCfg).at("Enemy1").at("angle").as<float>();
	int health = (*readCfg).at("Enemy1").at("health").as<int>();
	int pointValue = (*readCfg).at("Enemy1").at("pointValue").as<int>();
	float cooldown = (*readCfg).at("Enemy1").at("cooldown").as<float>();
	float startY = (*readCfg).at("Enemy1").at("ystart").as<float>();
	float accmax = (*readCfg).at("Enemy1").at("accmax").as<float>();
	float accmin = (*readCfg).at("Enemy1").at("accmin").as<float>();
	std::string explosionFX = (*readCfg).at("Enemy1").at("explosionFX").as<std::string>();
    // NOTE: PlayerData.cpp.l:25
    std::string meshPath = (*readCfg).at("Enemy1").at("meshpath").as<std::string>();
    std::string texturePath = (*readCfg).at("Enemy1").at("texturepath").as<std::string>();
	// default projectile orientation & scale
	GW::MATH2D::GMATRIX2F world;
	GW::MATH2D::GMatrix2D::Rotate2F(GW::MATH2D::GIdentityMatrix2F, 
	G_DEGREE_TO_RADIAN_F(angle), world);
	// Load sound effect used by this enemy prefab
	GW::AUDIO::GSound explosion;
	explosion.Create(explosionFX.c_str(), _audioEngine, 0.04f); // we need a global music & sfx volumes
	// add prefab to ECS
	auto enemyPrefab = _game->prefab("Enemy Type1")
		// .set<> in a prefab means components are shared (instanced)
	.set<StaticMeshComponent>({
		 RenderSystem::RegisterMesh(meshPath.c_str(), texturePath.c_str())
		})
	.set<Orientation>({ world, world })
	.set<Scale>({ GW::MATH::GVECTORF{ scale, scale, scale, 0 } })
	.set<GW::AUDIO::GSound>(explosion.Relinquish())
	.set<Points>({ pointValue })
	.set<EnemyStats>({ startY, accmin, accmax })
	.set<AlliedWith>({ ENEMY })
	// .override<> ensures a component is unique to each entity created from a prefab
	.set_override<Health>({ health })
	.override<Acceleration>()
	.override<Velocity>()
	.override<Position>()
	.override<Enemy>() // Tag this prefab as an enemy (for queries/systems)
	.override<Gameobject>()
	.override<Collidable>() // can be collided with
	.set_override<Cooldown>({ cooldown, cooldown })
	.set_override<Damage>({ 10 });

	// register this prefab by name so other systems can use it
	RegisterPrefab("Enemy Type1", enemyPrefab);

	return true;
}

bool EnemyData::Unload(std::shared_ptr<flecs::world> _game)
{
	// remove all bullets and their prefabs
	_game->defer_begin(); // required when removing while iterating!
	_game->each([](flecs::entity e, Enemy&) {
		e.destruct(); // destroy this entitiy (happens at frame end)
	});
	_game->defer_end(); // required when removing while iterating!

	return true;
}
