#include <random>
#include "LevelLogic.h"
#include "../Components/Identification.h"
#include "../Components/Gameplay.h"
#include "../Components/Physics.h"
#include "../Components/Visuals.h"
#include "../Entities/Prefabs.h"
#include "../Utils/Macros.h"
#include "../Events/Playevents.h"
#include "../Systems/RenderLogic.h"

using namespace TeamYellow;
// Connects logic to traverse any players and allow a controller to manipulate them
bool LevelLogic::Init(	std::shared_ptr<flecs::world> _game,
							std::weak_ptr<const GameConfig> _gameConfig,
							GW::AUDIO::GAudio _audioEngine,
							GW::CORE::GEventGenerator _eventPusher)
{
	spawnCount = 0;
	eventPusher = _eventPusher;
	// save a handle to the ECS & game settings
	game = _game;
	gameConfig = _gameConfig;
	audioEngine = _audioEngine;
	// create an asynchronus version of the world
	gameAsync = game->async_stage(); // just used for adding stuff, don't try to read data
	gameLock.Create();
	// Pull enemy Y start location from config file
	std::shared_ptr<const GameConfig> readCfg = _gameConfig.lock();
	gameplayAreaHalfHeight = (*readCfg).at("Player").at("GameplayAreaHalfHeight").as<float>();

	game->set<Skybox>({ 0, 1 });
	game->system<const Position, const Player>().kind(flecs::OnUpdate)
		.each([this](flecs::entity e, const Position& p, const Player& _) {
			float levelHalfWidth = e.world().get<LevelStats>()->halfWidth;
			spawnOriginY = p.value.y;
			float skyBoxRotationAngleSin = p.value.y / levelHalfWidth;
			float skyBoxRotationAngleCos = sqrtf(1 - (skyBoxRotationAngleSin * skyBoxRotationAngleSin));
			auto skyBox = game->get_mut<Skybox>();
			skyBox->value = { skyBoxRotationAngleCos, skyBoxRotationAngleSin };
	});
	// spins up a job in a thread pool to invoke a function at a regular interval
	timedEvents.Create(spawnDelay * 1000, [this]() {
		// compute random spawn location
		std::random_device rd;  // Will be used to obtain a seed for the random number engine
		std::mt19937 gen(rd()); // Standard mersenne_twister_engine seeded with rd()
        std::uniform_int_distribution<int> randomDir(0, 1);
		int factor = randomDir(gen); // 0 or 1
		int scalar = 1 - 2 * factor; // -1 or 1
		// grab enemy type 1 prefab
		flecs::entity et1; 
		if (RetreivePrefab("Enemy Type1", et1)) {
			const auto enemy1Stats = et1.get<EnemyStats>();
			std::uniform_real_distribution<float> x_range(-gameplayAreaHalfHeight, gameplayAreaHalfHeight);
			std::uniform_real_distribution<float> a_range(enemy1Stats->accMin, enemy1Stats->accMax);
			float Xstart = x_range(gen); // normal rand() doesn't work great multi-threaded
			float accel = a_range(gen);
			// you must ensure the async_stage is thread safe as it has no built-in synchronization
			gameLock.LockSyncWrite();
			// this method of using prefabs is pretty conveinent
			auto e = gameAsync.entity().is_a(et1)
				.set<Velocity>({ 0,0 })
				.set<Acceleration>({ 0, -scalar * accel })
				.set<Position>({ Xstart, spawnOriginY + scalar * enemy1Stats->startY });
			GW::MATH2D::GMATRIX2F world;
			GW::MATH2D::GMatrix2D::Rotate2F(GW::MATH2D::GIdentityMatrix2F, G_PI_F * (1 - factor), world);
			e.set<Orientation>({ world,world });
			// be sure to unlock when done so the main thread can safely merge the changes
			gameLock.UnlockSyncWrite();
		}
	}, 5000); // wait 5 seconds to start enemy wave

	// create a system the runs at the end of the frame only once to merge async changes
	struct LevelSystem {}; // local definition so we control iteration counts
	game->entity("Level System").add<LevelSystem>();
	// only happens once per frame at the very start of the frame
	game->system<LevelSystem>().kind(flecs::OnLoad) // first defined phase
		.each([this](flecs::entity e, LevelSystem& s) {
		// merge any waiting changes from the last frame that happened on other threads
		gameLock.LockSyncWrite();
		gameAsync.merge();
		gameLock.UnlockSyncWrite();
	});

	snprintf(_game->lookup("HUDCanvas::EnemiesRemainingText").get_mut<UIText>()->text, 247, "%d", spawnCount);
	onKill.Create([this](const GW::GEvent& e) {
		PLAY_EVENT event; PLAY_EVENT_DATA eventData;
		if (+e.Read(event, eventData)) {
			if (PLAY_EVENT::ENEMY_DESTROYED == event) {
				spawnCount = spawnCount > 0 ? spawnCount - 1 : 0;
				auto& world = eventData.entity_id.world();
				snprintf(world.entity("HUDCanvas::EnemiesRemainingText").get_mut<UIText>()->text, 247, "%d", spawnCount);
				if (spawnCount == 0) {
					//PlaySound("Enemy Type1");		// TODO: Level Cleared sound.
					game->set<GameStateManager>({STATE_GAMEPLAY, STATE_LEVEL_COMPLETE});
					std::cout << "Level Cleared!" << std::endl;
					PLAY_EVENT_DATA x = { flecs::entity::null() };
					GW::GEvent complete;
					complete.Write(PLAY_EVENT::LEVEL_CLEARED, x);
					eventPusher.Push(complete);		// TODO: register other systems to event
				}
			}
		}
	});
	eventPusher.Register(onKill);


	return true;
}

// Free any resources used to run this system
bool LevelLogic::Shutdown()
{
	timedEvents = nullptr; // stop adding enemies
	gameAsync.merge(); // get rid of any remaining commands
	game->entity("Level System").destruct();
	// invalidate the shared pointers
	game.reset();
	gameConfig.reset();
	return true;
}

void TeamYellow::LevelLogic::Reset()
{
	spawnDelay = game->get<LevelStats>()->spawnDelay;
	spawnCount = game->get<LevelStats>()->startingSpawnCount;
	snprintf(game->entity("HUDCanvas::EnemiesRemainingText").get_mut<UIText>()->text, 247, "%d", spawnCount);
	timedEvents = nullptr;
	timedEvents.Create(spawnDelay * 1000, [this]() {
		// compute random spawn location
		std::random_device rd;  // Will be used to obtain a seed for the random number engine
		std::mt19937 gen(rd()); // Standard mersenne_twister_engine seeded with rd()
		std::uniform_int_distribution<int> randomDir(0, 1);
		int factor = randomDir(gen); // 0 or 1
		int scalar = 1 - 2 * factor; // -1 or 1
		// grab enemy type 1 prefab
		flecs::entity et1; 
		if (RetreivePrefab("Enemy Type1", et1)) {
			const auto enemyStats = et1.get<EnemyStats>();
			std::uniform_real_distribution<float> x_range(-gameplayAreaHalfHeight, gameplayAreaHalfHeight);
			std::uniform_real_distribution<float> a_range(enemyStats->accMin, enemyStats->accMax);
			float Xstart = x_range(gen); // normal rand() doesn't work great multi-threaded
			float accel = a_range(gen);
			// you must ensure the async_stage is thread safe as it has no built-in synchronization
			gameLock.LockSyncWrite();
			// this method of using prefabs is pretty conveinent
			GW::MATH2D::GMATRIX2F world;
			GW::MATH2D::GMatrix2D::Rotate2F(GW::MATH2D::GIdentityMatrix2F, G_PI_F * (1 - factor), world);
			gameAsync.entity().is_a(et1)
				.set<Velocity>({ 0,0 })
				.set<Acceleration>({ 0, -scalar * accel })
				.set<Position>({ Xstart, spawnOriginY + scalar * enemyStats->startY })
				.set<Orientation>({ world,world });
			// be sure to unlock when done so the main thread can safely merge the changes
			gameLock.UnlockSyncWrite();
		}
	}, 5000); // wait 5 seconds to start enemy wave
}

// Toggle if a system's Logic is actively running
bool LevelLogic::Activate(bool runSystem)
{
	if (runSystem) {
		game->entity("Level System").enable();
		timedEvents.Resume();
	}
	else {
		game->entity("Level System").disable();
		timedEvents.Pause(true, 0);
	}
	return false;
}

// **** SAMPLE OF MULTI_THREADED USE ****
//flecs::world world; // main world
//flecs::world async_stage = world.async_stage();
//
//// From thread
//lock(async_stage_lock);
//flecs::entity e = async_stage.entity().child_of(parent)...
//unlock(async_stage_lock);
//
//// From main thread, periodic
//lock(async_stage_lock);
//async_stage.merge(); // merge all commands to main world
//unlock(async_stage_lock);