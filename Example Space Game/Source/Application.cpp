#include "Application.h"
#include "Components/Gameplay.h"
#include "Systems/RenderLogic.h"
#include "Components/Identification.h"
#include "Helper/AudioHelper.h"
// open some Gateware namespaces for conveinence 
// NEVER do this in a header file!
using namespace GW;
using namespace CORE;
using namespace SYSTEM;
using namespace GRAPHICS;
using namespace TeamYellow;

bool Application::Init() 
{
	eventPusher.Create();
	// load all game settigns
	gameConfig = std::make_shared<GameConfig>(); 
	// create the ECS system
	game = std::make_shared<flecs::world>();
	game->set<GameplayStats>({ 0 });
	// init all other systems
	if (InitWindow() == false) 
		return false;
	if (InitInput() == false)
		return false;
	if (InitAudio() == false)
		return false;
	if (InitGraphics() == false)
		return false;
    // NOTE: We Initialize Rendering here so that
	// entities can load in Mesh Data when they're
	// initialized.
	// TODO: Add Error Checking for Init Failure
	RenderSystem::InitBackend(gameConfig, vulkan, window);
	if (InitEntities() == false)
		return false;
	if (InitUIEntities() == false)
		return false;
	if (InitSystems() == false)
		return false;
	if(InitStateMachine() == false)
		return false;
	return true;
}

bool Application::Run() 
{
	VkClearValue clrAndDepth[2];
	clrAndDepth[0].color = { {0, 0, 0, 1} };
	clrAndDepth[1].depthStencil = { 1.0f, 0u };
	// grab vsync selection
	bool vsync = gameConfig->at("Window").at("vsync").as<bool>();
	// set background color from settings
	const char* channels[] = { "red", "green", "blue" };
	for (int i = 0; i < std::size(channels); ++i) {
		clrAndDepth[0].color.float32[i] =
			gameConfig->at("BackGroundColor").at(channels[i]).as<float>();
	}
	// create an event handler to see if the window was closed early
	bool winClosed = false;
	GW::CORE::GEventResponder winHandler;
	winHandler.Create([&winClosed](GW::GEvent e) {
		GW::SYSTEM::GWindow::Events ev;
		if (+e.Read(ev) && ev == GW::SYSTEM::GWindow::Events::DESTROY)
			winClosed = true;
	});	
	window.Register(winHandler);
	while (+window.ProcessWindowEvents())
	{
		if (winClosed == true)
			return true;
		if (+vulkan.StartFrame(2, clrAndDepth))
		{
			if (GameLoop() == false) {
				vulkan.EndFrame(vsync);
				return false;
			}
			if (-vulkan.EndFrame(vsync)) {
				// failing EndFrame is not always a critical error, see the GW docs for specifics
			}
		}
		else
			return false;
	}
	return true;
}

bool Application::Shutdown() 
{
	// disconnect systems from global ECS
	if (playerSystem.Shutdown() == false)
		return false;
	if (levelSystem.Shutdown() == false)
		return false;
	if (physicsSystem.Shutdown() == false)
		return false;
	if (bulletSystem.Shutdown() == false)
		return false;
	if (enemySystem.Shutdown() == false)
		return false;
    RenderSystem::ExitSystems();

	return true;
}

bool Application::InitWindow()
{
	// grab settings
	int width = gameConfig->at("Window").at("width").as<int>();
	int height = gameConfig->at("Window").at("height").as<int>();
	int xstart = gameConfig->at("Window").at("xstart").as<int>();
	int ystart = gameConfig->at("Window").at("ystart").as<int>();
	std::string title = gameConfig->at("Window").at("title").as<std::string>();
	// open window
	if (+window.Create(xstart, ystart, width, height, GWindowStyle::WINDOWEDLOCKED) &&
		+window.SetWindowName(title.c_str())) {
		return true;
	}
	return false;
}

bool Application::InitInput()
{
	if (-gamePads.Create())
		return false;
	if (-immediateInput.Create(window))
		return false;
	if (-bufferedInput.Create(window))
		return false;
	return true;
}

bool Application::InitAudio()
{
	if (-audioEngine.Create())
		return false;
	return true;
}

bool Application::InitGraphics()
{
#ifndef NDEBUG
	const char* debugLayers[] = {
		"VK_LAYER_KHRONOS_validation", // standard validation layer
		//"VK_LAYER_RENDERDOC_Capture" // add this if you have installed RenderDoc
	};
	if (+vulkan.Create(window, GW::GRAPHICS::DEPTH_BUFFER_SUPPORT,
		sizeof(debugLayers) / sizeof(debugLayers[0]),
		debugLayers, 0, nullptr, 0, nullptr, false))
		return true;
#else
	if (+vulkan.Create(window, GW::GRAPHICS::DEPTH_BUFFER_SUPPORT))
		return true;
#endif
	return false;
}

bool Application::InitEntities()
{
	currentTrack.Create("../Music/level_0.wav", audioEngine, 0.01f);
	currentTrack.Play(true);
	// Load bullet prefabs
	if (weapons.Load(game, gameConfig, audioEngine) == false)
		return false;
	// Load the player entities
	if (players.Load(game, gameConfig) == false)
		return false;
	// Load the enemy entities
	if (enemies.Load(game, gameConfig, audioEngine) == false)
		return false;
	if (level.Init(game, gameConfig) == false)
		return false;
	return true;
}

bool Application::InitUIEntities()
{
	uint32_t uiFontID = RenderSystem::RegisterUIFont(
		"../Assets/Fonts/Pixel.fnt", "../Assets/Textures/Pixel.tga");
	// Create Game Start UI Overlay
	auto startCanvas = game->entity("StartCanvas").set<UICanvas>({
		uiFontID,
		~(0u),
		0,
		true
	});
	auto prevScope = game->set_scope(startCanvas);
	game->entity("gameTitleLabel")
	.set<UIRect>({ -0.5F, -0.2F, 740.F, 160.F })
	.set<UIText>({
		{ 0.5F, 0.5F, 0.F, 1.F },   // Font Color
		{ 0.F, 0.F, 1.F, 1.F },     // Outline Color
		0.55F,                      // Outline Width
		3.F,                        // Font Scale
		"Space Dasher"              // Text Buffer
	});
	game->entity("startLabel")
	.set<UIRect>({ -0.5F, 0.4F, 740.F, 160.F })
	.set<UIText>({
		{ 0.5F, 0.5F, 0.F, 1.F },   // Font Color
		{ 0.F, 0.F, 1.F, 1.F },     // Outline Color
		0.55F,                      // Outline Width
		1.5F,                        // Font Scale
		"Press Enter to Start..."// Text Buffer
	});
	game->set_scope(prevScope);
	// Create Game Paused UI Overlay
	auto gamePaused = game->entity("PausedCanvas").set<UICanvas>({
		uiFontID,
		~(0u),
		0,
		false
	});
	prevScope = game->set_scope(gamePaused);
	game->entity("PausedLabel")
	.set<UIRect>({ -0.6F, -0.2F, 840.F, 160.F })
	.set<UIText>({
		{ 0.5F, 0.5F, 0.F, 1.F },   // Font Color
		{ 0.F, 0.F, 1.F, 1.F },     // Outline Color
		0.55F,                      // Outline Width
		3.F,                        // Font Scale
		"Game Paused"           // Text Buffer
	});
	game->set_scope(prevScope);
	// Create Level Completed UI Overlay
	auto levelCompletedCanvas = game->entity("LevelCompletedCanvas").set<UICanvas>({
		uiFontID,
		~(0u),
		0,
		false
	});
	prevScope = game->set_scope(levelCompletedCanvas);
	game->entity("LevelCompletedLabel")
	.set<UIRect>({ -0.6F, -0.2F, 840.F, 160.F })
	.set<UIText>({
		{ 0.5F, 0.5F, 0.F, 1.F },   // Font Color
		{ 0.F, 0.F, 1.F, 1.F },     // Outline Color
		0.55F,                      // Outline Width
		3.F,                        // Font Scale
		"Level Completed"           // Text Buffer
	});
	game->entity("ContinueLabel")
	.set<UIRect>({ -0.5F, 0.F, 740.F, 160.F })
	.set<UIText>({
		{ 0.5F, 0.5F, 0.F, 1.F },   // Font Color
		{ 0.F, 0.F, 1.F, 1.F },     // Outline Color
		0.55F,                      // Outline Width
		1.5F,                        // Font Scale
		"Press Enter to Continue..."// Text Buffer
	});
	game->set_scope(prevScope);
	// Create Game Completed UI Overlay
	auto playerWon = game->entity("YouWonCanvas").set<UICanvas>({
		uiFontID,
		~(0u),
		0,
		false
	});
	prevScope = game->set_scope(playerWon);
	game->entity("YouWonLabel")
	.set<UIRect>({ -0.3F, -0.2F, 1200.F, 160.F })
	.set<UIText>({
		{ 0.5F, 0.5F, 0.F, 1.F },   // Font Color
		{ 0.F, 0.F, 1.F, 1.F },     // Outline Color
		0.55F,                      // Outline Width
		3.F,                        // Font Scale
		"You Won"           // Text Buffer
	});
	game->entity("EndlessLabel")
	.set<UIRect>({ -0.5F, 0.F, 740.F, 160.F })
	.set<UIText>({
		{ 0.5F, 0.5F, 0.F, 1.F },   // Font Color
		{ 0.F, 0.F, 1.F, 1.F },     // Outline Color
		0.55F,                      // Outline Width
		1.5F,                        // Font Scale
		"Endless Mode Activated"// Text Buffer
	});
	game->set_scope(prevScope);
	// Create Life Lost UI Overlay
	auto playerDied = game->entity("YouDiedCanvas").set<UICanvas>({
		uiFontID,
		~(0u),
		0,
		false
	});
	prevScope = game->set_scope(playerDied);
	game->entity("YouDiedLabel")
	.set<UIRect>({ -0.6F, -0.2F, 840.F, 160.F })
	.set<UIText>({
		{ 0.5F, 0.5F, 0.F, 1.F },   // Font Color
		{ 0.F, 0.F, 1.F, 1.F },     // Outline Color
		0.55F,                      // Outline Width
		3.F,                        // Font Scale
		"You Died"           // Text Buffer
	});
	game->set_scope(prevScope);
	// Create Game Over UI Overlay
	auto playerDefeated = game->entity("DefeatedCanvas").set<UICanvas>({
		uiFontID,
		~(0u),
		0,
		false
	});
	prevScope = game->set_scope(playerDefeated);
	game->entity("DefeatedLabel")
	.set<UIRect>({ -0.6F, -0.2F, 840.F, 160.F })
	.set<UIText>({
		{ 0.5F, 0.5F, 0.F, 1.F },   // Font Color
		{ 0.F, 0.F, 1.F, 1.F },     // Outline Color
		0.55F,                      // Outline Width
		3.F,                        // Font Scale
		"GAME OVER"           // Text Buffer
	});
	game->set_scope(prevScope);
	return true;
}

bool Application::InitSystems()
{
	// connect systems to global ECS
	if (playerSystem.Init(	game, gameConfig, immediateInput, bufferedInput, 
							gamePads, audioEngine, eventPusher) == false)
		return false;
	if (levelSystem.Init(game, gameConfig, audioEngine, eventPusher) == false)
		return false;
	// TODO: Add Error Checking for Init Failure
	RenderSystem::InitSystems(game);
	if (physicsSystem.Init(game, gameConfig) == false)
		return false;
	if (bulletSystem.Init(game, gameConfig) == false)
		return false;
	if (enemySystem.Init(game, gameConfig, eventPusher) == false)
		return false;
	playerSystem.Activate(false);
	enemySystem.Activate(false);
	return true;
}

bool Application::GameLoop()
{
	// compute delta time and pass to the ECS system
	static auto start = std::chrono::steady_clock::now();
	double elapsed = std::chrono::duration<double>(
		std::chrono::steady_clock::now() - start).count();
	start = std::chrono::steady_clock::now();
	// let the ECS system run
	return game->progress(static_cast<float>(elapsed)); 
}

bool Application::InitStateMachine() {
	stateEvents.Create(32);
	bufferedInput.Register(stateEvents);
	currentLevel = LEVEL_START;
	level.Load(game, currentLevel);
	levelSystem.Reset();
	game->set<GameStateManager>({STATE_START});
	game->system<GameStateManager>("Game State System")
		.each([this](flecs::entity e, GameStateManager& gameState) {
		if(gameState.state == STATE_GAMEPLAY){
			switch (gameState.target) {
				case STATE_UNDEFINED:
					stateEvents.Clear();
					return;
				case STATE_PAUSE:
					game->entity("PausedCanvas").get_mut<UICanvas>()->isVisible = true;
					playerSystem.Activate(false);
					levelSystem.Activate(false);
					enemySystem.Activate(false);
					physicsSystem.Activate(false);
					currentTrack.Create("../Music/level_0.wav", audioEngine, 0.01f);
					currentTrack.Play(true);
					break;
				case STATE_LEVEL_COMPLETE:
					if(currentLevel == LEVEL_COUNT - 1)
						game->entity("YouWonCanvas").get_mut<UICanvas>()->isVisible = true;
					else
						game->entity("LevelCompletedCanvas").get_mut<UICanvas>()->isVisible = true;
					enemySystem.Clear();
					bulletSystem.Clear();
					levelSystem.Activate(false);
					break;
				case STATE_DESTROYED:
					PlaySound("Enemy Type1");
					game->entity("YouDiedCanvas").get_mut<UICanvas>()->isVisible = true;
					bulletSystem.Clear();
					enemySystem.Clear();
					playerSystem.Activate(false);
					enemySystem.Activate(false);
					levelSystem.Activate(false);
					break;
				case STATE_GAMEOVER:
					game->entity("DefeatedCanvas").get_mut<UICanvas>()->isVisible = true;
					bulletSystem.Clear();
					playerSystem.Activate(false);
					enemySystem.Activate(false);
					break;
			}
			gameState.state = gameState.target;
			return;
		}
		GW::GEvent event;
		while (+stateEvents.Pop(event)) {
			GW::INPUT::GBufferedInput::Events keyboard;
			GW::INPUT::GBufferedInput::EVENT_DATA k_data;
			if (+event.Read(keyboard, k_data)) {
				if (k_data.data == G_KEY_ENTER && keyboard == GW::INPUT::GBufferedInput::Events::KEYPRESSED) {
					switch (gameState.state) {
						case STATE_START:
							game->entity("StartCanvas").get_mut<UICanvas>()->isVisible = false;
							currentTrack.Create("../Music/level_01.wav", audioEngine, 0.01f);
							currentTrack.Play(true);
							gameState.state = STATE_GAMEPLAY;
							gameState.target = STATE_UNDEFINED;
							playerSystem.Activate(true);
							enemySystem.Clear();
							enemySystem.Activate(true);
							currentLevel = LEVEL_ONE;
							level.Load(game, currentLevel);
							levelSystem.Reset();
							break;
						case STATE_PAUSE:
							game->entity("PausedCanvas").get_mut<UICanvas>()->isVisible = false;
							if (currentLevel == LEVEL_ONE) {
								currentTrack.Create("../Music/level_01.wav", audioEngine, 0.01f);
								currentTrack.Play(true);
							}
							else if (currentLevel == LEVEL_TWO) {
								currentTrack.Create("../Music/level_02.wav", audioEngine, 0.01f);
								currentTrack.Play(true);
							}
							else if (currentLevel == LEVEL_THREE) {
								currentTrack.Create("../Music/level_03.wav", audioEngine, 0.01f);
								currentTrack.Play(true);
							}
							gameState.state = STATE_GAMEPLAY;
							gameState.target = STATE_UNDEFINED;
							playerSystem.Activate(true);
							levelSystem.Activate(true);
							enemySystem.Activate(true);
							physicsSystem.Activate(true);
							break;
						case STATE_LEVEL_COMPLETE:
							enemySystem.Clear();
							bulletSystem.Clear();
							if (currentLevel < LEVEL_COUNT) {
								currentLevel = (LevelState)((currentLevel + 1) % LEVEL_COUNT);
								if (currentLevel == LEVEL_TWO) {
									currentTrack.Create("../Music/level_02.wav", audioEngine, 0.01f);
									currentTrack.Play(true);
								}
								if (currentLevel == LEVEL_THREE) {
									currentTrack.Create("../Music/level_03.wav", audioEngine, 0.01f);
									currentTrack.Play(true);
								}
							}
							else
							{
								if(currentLevel == LEVEL_ENDLESS_THREE) currentLevel = LEVEL_COUNT;
								currentLevel = (LevelState)((currentLevel + 1));
							}
							level.Unload(game);
							gameState.state = STATE_GAMEPLAY;
							gameState.target = STATE_UNDEFINED;
							if(currentLevel == LEVEL_START)
							{
								game->entity("YouWonCanvas").get_mut<UICanvas>()->isVisible = false;
								currentLevel = LEVEL_ENDLESS_ONE;
							}
							else
							{
								game->entity("LevelCompletedCanvas").get_mut<UICanvas>()->isVisible = false;
							}
							level.Load(game, currentLevel);
							levelSystem.Reset();
							levelSystem.Activate(true);
							break;
						case STATE_DESTROYED:
							game->entity("YouDiedCanvas").get_mut<UICanvas>()->isVisible = false;
							gameState.state = STATE_GAMEPLAY;
							gameState.target = STATE_UNDEFINED;
							playerSystem.Activate(true);
							enemySystem.Activate(true);
							levelSystem.Activate(true);
							break;
						case STATE_GAMEOVER:
							gameState.state = STATE_START;
							currentLevel = LEVEL_START;
							game->entity("DefeatedCanvas").get_mut<UICanvas>()->isVisible = false;
							game->entity("StartCanvas").get_mut<UICanvas>()->isVisible = true;
							enemySystem.Clear();
							bulletSystem.Clear();
							level.Unload(game);
							level.Load(game, currentLevel);
							levelSystem.Reset();
							enemySystem.Activate(false);
							levelSystem.Activate(true);
							break;
					}
					stateEvents.Clear();
				}
			}
		}
	});
	return true;
}