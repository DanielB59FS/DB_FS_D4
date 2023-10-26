#include <random>
#include "LevelData.h"
#include "Prefabs.h"

#include "../Components/Gameplay.h"
#include "../Components/Identification.h"
#include "../Components/Visuals.h"
#include "../Components/Physics.h"

#include "../Systems/RenderLogic.h"

using namespace TeamYellow;

bool LevelData::Init(std::shared_ptr<flecs::world> _game,
	std::weak_ptr<const GameConfig> _gameConfig) {
	std::shared_ptr<const GameConfig> readCfg = _gameConfig.lock();
	levels[LEVEL_START].spawnDelay = 1;
	levels[LEVEL_START].spawnCount = 10;
	levels[LEVEL_START].halfWidth = 500;
	std::string skyBoxTexturePathStrings = (*readCfg).at("Common").at("skybox").as<std::string>();
	levels[LEVEL_START].skyBoxID = RenderSystem::RegisterSkybox(skyBoxTexturePathStrings.c_str());
	_game->set<Skybox>({ levels[LEVEL_START].skyBoxID, { 1, 0 }});
	std::string floorMeshPathString = (*readCfg).at("Common").at("floorMeshPath").as<std::string>();
	std::string floorTexturePathString = (*readCfg).at("Common").at("texture_floor").as<std::string>();
	uint32_t floorMeshID = RenderSystem::RegisterMesh(floorMeshPathString.c_str(), floorTexturePathString.c_str());
	levels[LEVEL_ONE].floorMeshID = floorMeshID;
	levels[LEVEL_TWO].floorMeshID = floorMeshID;
	levels[LEVEL_THREE].floorMeshID = floorMeshID;
	InitLevelReadOnlyData(_game, (*readCfg).at("Level1"), levels[LEVEL_ONE]);
	InitLevelReadOnlyData(_game, (*readCfg).at("Level2"), levels[LEVEL_TWO]);
	InitLevelReadOnlyData(_game, (*readCfg).at("Level3"), levels[LEVEL_THREE]);
	return true;
}

void LevelData::InitLevelReadOnlyData(std::shared_ptr<flecs::world> _game,
			const ini::IniSection& _section,
			LevelReadOnlyData& _data) {
	/*-----------------------------------------------------------------------*/
	/* Level Metrics                                                         */
	/*-----------------------------------------------------------------------*/
	_data.environmentObjectScale = _section.at("EnvironmentObjectScale").as<float>();
	_data.halfWidth = _section.at("LevelHalfWidth").as<float>();
	_data.spawnDelay = _section.at("spawndelay").as<float>();
	_data.spawnCount = _section.at("spawnCount").as<unsigned>();
	/*-----------------------------------------------------------------------*/
	/* Environment Object Mesh Paths                                         */
	/*-----------------------------------------------------------------------*/
	std::string meshPathStrings[5];
	// foreground buildings
	meshPathStrings[0] = _section.at("meshPath1").as<std::string>();
	meshPathStrings[1] = _section.at("meshPath2").as<std::string>();
	// background buildings
	meshPathStrings[2] = _section.at("meshPath3").as<std::string>();
	meshPathStrings[3] = _section.at("meshPath4").as<std::string>();
	meshPathStrings[4] = _section.at("meshPath5").as<std::string>();
	/*-----------------------------------------------------------------------*/
	/* Environment Object Texture Paths                                      */
	/*-----------------------------------------------------------------------*/
	std::string texturePathString = _section.at("texture").as<std::string>();
	/*-----------------------------------------------------------------------*/
	/* Render System Mesh IDs                                                */
	/*-----------------------------------------------------------------------*/
	for (int i = 0; i < 5;  ++i) {
		_data.meshIDs[i] = RenderSystem::RegisterMesh(
			meshPathStrings[i].c_str(), texturePathString.c_str());
	}
	/*-----------------------------------------------------------------------*/
	/* Skybox Texture IDs                                                    */
	/*-----------------------------------------------------------------------*/
	std::string skyBoxTexturePathStrings = _section.at("skybox").as<std::string>();
	_data.skyBoxID = RenderSystem::RegisterSkybox(skyBoxTexturePathStrings.c_str());
}

bool LevelData::Load(std::shared_ptr<flecs::world> _game,
	LevelState _level) {
	_level = (LevelState)(_level % LEVEL_COUNT);
	_game->set<Skybox>({ levels[_level].skyBoxID, { 1, 0 }});
	_game->set<LevelStats>({ levels[_level].spawnDelay, levels[_level].spawnCount, levels[_level].halfWidth });
	if(_level == LEVEL_START) return true;
	/*=======================================================================*/
	/* Populate Environment Entities                                         */
	/*=======================================================================*/
	GW::MATH2D::GMATRIX2F world = GW::MATH2D::GIdentityMatrix2F;
	std::random_device rd;  // Will be used to obtain a seed for the random number engine
	std::mt19937 gen(rd()); // Standard mersenne_twister_engine seeded with rd()
	const auto& meshBounds = RenderSystem::GetMeshBoundsVector();
	std::uniform_int_distribution<uint32_t> bgMeshSelector(2, 4);
	float xPos = levels[_level].halfWidth;
	while (xPos > -levels[_level].halfWidth) {
		uint32_t buildingIndex = bgMeshSelector(gen);
		const auto& bounds = meshBounds[levels[_level].meshIDs[buildingIndex]];
		xPos -= fabs(bounds.min.z) * levels[_level].environmentObjectScale;
		_game->entity() 
			.set([&](Position& p, Orientation& o, Scale& s, StaticMeshComponent& sm) {
			p = { -30,  xPos };
			sm = { levels[_level].meshIDs[buildingIndex] };
			o = { world, world };
			s = { levels[_level].environmentObjectScale, levels[_level].environmentObjectScale, levels[_level].environmentObjectScale, 0 };
				})
			.add<Background>(); // Tag this entity as a Background Object
				xPos -= fabs(bounds.max.z) * levels[_level].environmentObjectScale;
	}
	xPos = levels[_level].halfWidth;
	while(xPos > -levels[_level].halfWidth) {
		const auto& bounds = meshBounds[levels[_level].floorMeshID];
		xPos -= fabs(bounds.min.y) * 1;
		_game->entity()
			.set([&](Position& p, Orientation& o, Scale& s, StaticMeshComponent& sm) {
				p = { -31,  xPos };
				sm = { levels[_level].floorMeshID };
				o = { world, world };
				s = { 1, 1, 1, 0 };
			})
			.add<Floor>(); // Tag this entity as a Floor
		xPos-= fabs(bounds.max.y) * 1;
	}
	/*-----------------------------------------------------------------------*/
	/* Spawn Foreground Entities                                             */
	/*-----------------------------------------------------------------------*/
	std::uniform_int_distribution<uint32_t> fgMeshSelector(0, 1);
	xPos = levels[_level].halfWidth;
	while (xPos > -levels[_level].halfWidth) {
		uint32_t buildingIndex = fgMeshSelector(gen);
		const auto& bounds = meshBounds[levels[_level].meshIDs[buildingIndex]];
		xPos -= fabs(bounds.min.y) * levels[_level].environmentObjectScale;
		_game->entity()
			.set([&](Position& p, Orientation& o, Scale& s, StaticMeshComponent& sm) {
			p = { -30,  xPos };
			sm = { levels[_level].meshIDs[buildingIndex] };
			o = { world, world };
			s = { levels[_level].environmentObjectScale, levels[_level].environmentObjectScale, levels[_level].environmentObjectScale, 0 };
				})
			.add<Foreground>(); // Tag this entity as a Foreground Object
				xPos -= fabs(bounds.max.y) * levels[_level].environmentObjectScale;
	}
	return true;
}

bool LevelData::Unload(std::shared_ptr<flecs::world> _game)
{
	// remove all buildings
	_game->defer_begin(); // required when removing while iterating!
	_game->each([](flecs::entity e,Foreground&) {
		e.destruct(); // destroy this entitiy (happens at frame end)
		});
	_game->each([](flecs::entity e, Floor&) {
		e.destruct(); // destroy this entitiy (happens at frame end)
		});
	_game->each([](flecs::entity e, Background&) {
		e.destruct(); // destroy this entitiy (happens at frame end)
		});
	_game->defer_end(); // required when removing while iterating!

	return true;
}
