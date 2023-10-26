#include "BombData.h"
#include "../Components/Identification.h"
#include "../Components/Visuals.h"
#include "../Components/Physics.h"
#include "Prefabs.h"
#include "../Components/Gameplay.h"

using namespace TeamYellow;

bool ESG::BombData::Load(std::shared_ptr<flecs::world> _game,
						 std::weak_ptr<const GameConfig> _gameConfig,
						 GW::AUDIO::GAudio _audioEngine) {

	std::shared_ptr<const GameConfig> readCfg = _gameConfig.lock();

	float red = (*readCfg).at("Bomb").at("red").as<float>();
	float green = (*readCfg).at("Bomb").at("green").as<float>();
	float blue = (*readCfg).at("Bomb").at("blue").as<float>();

	float scale = (*readCfg).at("Bomb").at("scale").as<float>();
	int dmg = (*readCfg).at("Bomb").at("damage").as<int>();
	float frate = (*readCfg).at("Bomb").at("firerate").as<float>();
	std::string fireFX = (*readCfg).at("Bomb").at("fireFX").as<std::string>();

	GW::MATH2D::GMATRIX2F world;
	GW::MATH2D::GMatrix2D::Scale2F(GW::MATH2D::GIdentityMatrix2F,
								   GW::MATH2D::GVECTOR2F { scale, scale }, world);

	GW::AUDIO::GSound shoot;
	shoot.Create(fireFX.c_str(), _audioEngine, 0.15f);

	auto bombPrefab = _game->prefab()
		.set<Orientation>({ world })
		.set<Acceleration>({ 0, 0 })
		.set<GW::AUDIO::GSound>(shoot.Relinquish())
		.set<AlliedWith>({ PLAYER })
		.set_override<Damage>({ dmg })
		.override<Position>()
		.override<Bomb>();

	RegisterPrefab("Bomb", bombPrefab);

	return true;
}

bool ESG::BombData::Unload(std::shared_ptr<flecs::world> _game) {
	_game->defer_begin();
	_game->each([](flecs::entity e, Bomb&) {
		e.destruct();
				});
	_game->defer_end();

	UnregisterPrefab("Bomb");

	return true;
}
