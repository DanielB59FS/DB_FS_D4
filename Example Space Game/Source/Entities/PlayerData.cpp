#include "PlayerData.h"
#include "Prefabs.h"

#include "../Components/Identification.h"
#include "../Components/Visuals.h"
#include "../Components/Physics.h"
#include "../Components/Gameplay.h"

#include "../Systems/RenderLogic.h"

using namespace TeamYellow;

bool PlayerData::Load(  std::shared_ptr<flecs::world> _game, 
                            std::weak_ptr<const GameConfig> _gameConfig)
{
	// Grab init settings for players
	std::shared_ptr<const GameConfig> readCfg = _gameConfig.lock();
	// start position
	float xstart = (*readCfg).at("Player1").at("xstart").as<float>();
	float ystart = (*readCfg).at("Player1").at("ystart").as<float>();
	float scale = (*readCfg).at("Player1").at("scale").as<float>();
    // NOTE: Not a fan of loading these one by one,
    // maybe storing the paths and batch loading is a better option?
    std::string meshPath = (*readCfg).at("Player1").at("meshpath").as<std::string>();
    std::string texturePath = (*readCfg).at("Player1").at("texturepath").as<std::string>();

	// Create Player One
	_game->entity("Player One")
	.set([&](Position& p, Orientation& o, Scale& s, StaticMeshComponent& sm, ControllerID& c, AlliedWith& ally) {
		c = { 0 };
		p = { xstart, ystart };
        sm ={
            RenderSystem::RegisterMesh(meshPath.c_str(), texturePath.c_str()),
        };
		o = { GW::MATH2D::GIdentityMatrix2F, GW::MATH2D::GIdentityMatrix2F };
		s = { scale, scale, scale, 0 };
		ally = { PLAYER };
	})
	.add<Health>()
	.add<Collidable>()
	.add<PlayerStats>()
	.add<Gameobject>()
	.add<Player>(); // Tag this entity as a Player

	return true;
}

bool PlayerData::Unload(std::shared_ptr<flecs::world> _game)
{
	// remove all players
	_game->defer_begin(); // required when removing while iterating!
		_game->each([](flecs::entity e, Player&) {
			e.destruct(); // destroy this entitiy (happens at frame end)
		});
	_game->defer_end(); // required when removing while iterating!

    return true;
}
