#ifndef APPLICATION_H
#define APPLICATION_H

// include events
#include "Events/Playevents.h"
// Contains our global game settings
#include "GameConfig.h"
// Load all entities+prefabs used by the game 
#include "Entities/BulletData.h"
#include "Entities/PlayerData.h"
#include "Entities/EnemyData.h"
#include "Entities/LevelData.h"
// Include all systems used by the game and their associated components
#include "Systems/PlayerLogic.h"
#include "Systems/LevelLogic.h"
#include "Systems/PhysicsLogic.h"
#include "Systems/BulletLogic.h"
#include "Systems/EnemyLogic.h"

namespace TeamYellow { enum LevelState; };

// Allocates and runs all sub-systems essential to operating the game
class Application 
{
	// gateware libs used to access operating system
	GW::SYSTEM::GWindow window; // gateware multi-platform window
	GW::GRAPHICS::GVulkanSurface vulkan; // gateware vulkan API wrapper
	GW::INPUT::GController gamePads; // controller support
	GW::INPUT::GInput immediateInput; // twitch keybaord/mouse
	GW::INPUT::GBufferedInput bufferedInput; // event keyboard/mouse
	GW::AUDIO::GAudio audioEngine; // can create music & sound effects
	// third-party gameplay & utility libraries
	std::shared_ptr<flecs::world> game; // ECS database for gameplay
	std::shared_ptr<GameConfig> gameConfig; // .ini file game settings
	// ECS Entities and Prefabs that need to be loaded
	TeamYellow::BulletData weapons;
	TeamYellow::PlayerData players;
	TeamYellow::EnemyData enemies;
	// specific ECS systems used to run the game
	TeamYellow::PlayerLogic playerSystem;
	TeamYellow::LevelLogic levelSystem;
	TeamYellow::PhysicsLogic physicsSystem;
	TeamYellow::BulletLogic bulletSystem;
	TeamYellow::EnemyLogic enemySystem;
	TeamYellow::LevelData level;
	// EventGenerator for Game Events
	GW::CORE::GEventGenerator eventPusher;
	GW::CORE::GEventCache stateEvents;
	TeamYellow::LevelState currentLevel;
	GW::AUDIO::GMusic currentTrack;

public:
	bool levelLoaded = false;
	bool Init();
	bool Run();
	bool Shutdown();
private:
	bool InitWindow();
	bool InitInput();
	bool InitAudio();
	bool InitGraphics();
	bool InitEntities();
	bool InitUIEntities();
	bool InitSystems();
	bool GameLoop();
	bool InitStateMachine();
};

#endif 