#include "PlayerLogic.h"
#include "../Components/Identification.h"
#include "../Components/Physics.h"
#include "../Components/Gameplay.h"
#include "../Components/Visuals.h"
#include "../Entities/Prefabs.h"
#include "../Events/Playevents.h"
#include "../Systems/RenderLogic.h"

using namespace TeamYellow;
using namespace GW::INPUT; // input libs
using namespace GW::AUDIO; // audio libs

// Connects logic to traverse any players and allow a controller to manipulate them
bool PlayerLogic::Init(
							std::shared_ptr<flecs::world> _game, 
							std::weak_ptr<GameConfig> _gameConfig, 
							GW::INPUT::GInput _immediateInput, 
							GW::INPUT::GBufferedInput _bufferedInput, 
							GW::INPUT::GController _controllerInput,
							GW::AUDIO::GAudio _audioEngine,
							GW::CORE::GEventGenerator _eventPusher)
{
	// save a handle to the ECS & game settings
	game = _game;
	gameConfig = _gameConfig;
	immediateInput = _immediateInput;
	bufferedInput = _bufferedInput;
	controllerInput =	_controllerInput;
	audioEngine = _audioEngine;
	eventPusher = _eventPusher;
	
	// Init any helper systems required for this task
	std::shared_ptr<GameConfig> readCfg = gameConfig.lock();
	float xstart = (*readCfg).at("Player1").at("xstart").as<float>();
	float ystart = (*readCfg).at("Player1").at("ystart").as<float>();
	int startHealth             = (*readCfg).at("Player").at("startHealth").as<int>();
	int startLives              = (*readCfg).at("Player").at("startLives").as<int>();
	int startSpecials           = (*readCfg).at("Player").at("startSpecials").as<int>();
	int scoreToLifeReward       = (*readCfg).at("Player").at("scoreToLifeReward").as<int>();
	int scoreToSpecialReward    = (*readCfg).at("Player").at("scoreToSpecialReward").as<int>();
	float gameplayAreaHalfHeight= (*readCfg).at("Player").at("GameplayAreaHalfHeight").as<float>();
	highScore                   = (*readCfg).at("Player").at("savedScore").as<int>();
	float speed = (*readCfg).at("Player1").at("speed").as<float>();
	float turnSpeed = (*readCfg).at("Player1").at("turnSpeed").as<float>();
	chargeTime = (*readCfg).at("Player1").at("chargeTime").as<float>();
	keyUp = (*readCfg).at("Keybinds").at("Up").as<int>();
	keyDown = (*readCfg).at("Keybinds").at("Down").as<int>();
	keyLeft = (*readCfg).at("Keybinds").at("Left").as<int>();
	keyRight = (*readCfg).at("Keybinds").at("Right").as<int>();
	keyFire = (*readCfg).at("Keybinds").at("Fire").as<int>();
	// add logic for updating players
	playerEntities[0] = _game->entity("Player One");
	playerEntities[0].set<PlayerStats>({
		startLives,
		startSpecials,
		scoreToLifeReward,
		scoreToSpecialReward
	});
	playerEntities[0].set<Health>({ startHealth });
	playerSystem = game->system<Player, Position, Orientation, Health, PlayerStats, ControllerID>("Player System")
		.iter([this, speed, turnSpeed, gameplayAreaHalfHeight,
				xstart, ystart, startHealth, startLives, startSpecials](
					flecs::iter it, Player*, Position* p, Orientation* o, Health* h, PlayerStats* playerStats, ControllerID* c) {

		for (auto i : it) {
			if (playerStats->lives > 0 && h->value <= 0) {
				--playerStats->lives;
				// Reset Player back to Starting Health and Position
				h->value = startHealth;
				p[i].value.x = xstart;
				p[i].value.y = ystart;
				// Fire Player Destroyed Event
				std::cout << "Player Was Destroyed!\n";
				snprintf(it.world().entity("HUDCanvas::LivesText").get_mut<UIText>()->text,
					247, "x%d", playerStats->lives);
				PLAY_EVENT_DATA x;
				x.entity_id = it.entity(i);
				GW::GEvent destroyed;
				destroyed.Write(PLAY_EVENT::PLAYER_DESTROYED, x);

 				eventPusher.Push(destroyed);
				game->set<GameStateManager>({STATE_GAMEPLAY, STATE_DESTROYED});
				// If Player has no lives left, fire Player Defeated Event
				if(playerStats->lives == 0)
				{
					std::cout << "Player Was Defeated!\n";
					PLAY_EVENT_DATA x;
					x.entity_id = it.entity(i);
					GW::GEvent defeated;
					defeated.Write(PLAY_EVENT::PLAYER_DEFEATED, x);
					eventPusher.Push(defeated);
					game->set<GameStateManager>({STATE_GAMEPLAY, STATE_GAMEOVER});
					playerStats->lives = startLives;
					playerStats->specials = startSpecials;
					playerStats->scoreToNextLifeReward = 0;
					playerStats->scoreToNextSpecialReward = 0;
					auto gameplayStats = game->get_mut<GameplayStats>();
					if (highScore < gameplayStats->score) highScore = gameplayStats->score;
					gameplayStats->score = 0;
				}
				return;
			}
			// left-right movement
			float xaxis = 0, yaxis = 0, input = 0;
			// Use the controller/keyboard to move the player around the screen			
			if (c[i].index == 0) { // enable keyboard controls for player 1
				immediateInput.GetState(keyLeft, input); yaxis += input;
				immediateInput.GetState(keyRight, input); yaxis -= input;
				immediateInput.GetState(keyUp, input); xaxis += input;
				immediateInput.GetState(keyDown, input); xaxis -= input;
			}
			// grab left-thumb stick
			/*controllerInput.GetState(c[i].index, G_LX_AXIS, input); xaxis += input;
			controllerInput.GetState(c[i].index, G_DPAD_LEFT_BTN, input); xaxis -= input;
			controllerInput.GetState(c[i].index, G_DPAD_RIGHT_BTN, input); xaxis += input;*/
			xaxis = G_LARGER(xaxis, -1);
			xaxis = G_SMALLER(xaxis, 1);
			yaxis = G_LARGER(yaxis, -1);
			yaxis = G_SMALLER(yaxis, 1);

			// apply movement
			p[i].value.x += xaxis * it.delta_time() * speed;
			// limit the player to stay within -1 to +1 NDC
			p[i].value.x = G_LARGER(p[i].value.x, -gameplayAreaHalfHeight);
			p[i].value.x = G_SMALLER(p[i].value.x, gameplayAreaHalfHeight);
			// apply movement
			p[i].value.y += yaxis * it.delta_time() * speed;
			// limit the player to stay within -1 to +1 NDC
			float levelBoundsHalfWidth = it.world().get<LevelStats>()->halfWidth - 100.f;
			p[i].value.y = G_LARGER(p[i].value.y, -levelBoundsHalfWidth);
			p[i].value.y = G_SMALLER(p[i].value.y, levelBoundsHalfWidth);

			float dir = o->target.row1.x;
			if(yaxis > 0) dir = 1;
			if(yaxis < 0) dir = -1;
			o->target.row1.x = dir;
			o->target.row2.y = dir;
			float currentRotation, targetRotation;
			GW::MATH2D::GMatrix2D::GetRotation2F(o->value, currentRotation);
			GW::MATH2D::GMatrix2D::GetRotation2F(o->target, targetRotation);
			currentRotation = fabsf(currentRotation);
			targetRotation = fabsf(targetRotation);
			if(currentRotation < targetRotation)
			{
				float deltaRotation = it.delta_time() * turnSpeed * G_PI_F;
				if(currentRotation + deltaRotation > targetRotation)
					deltaRotation = targetRotation - currentRotation;
				GW::MATH2D::GMatrix2D::Rotate2F(o->value, deltaRotation, o->value);
			} else if(currentRotation > targetRotation) {
				float deltaRotation = it.delta_time() * turnSpeed * G_PI_F;
				if(currentRotation - deltaRotation < targetRotation)
					deltaRotation = currentRotation - targetRotation;
				GW::MATH2D::GMatrix2D::Rotate2F(o->value, -deltaRotation, o->value);
			}

			// fire weapon if they are in a firing state
			if (it.entity(i).has<Firing>()) 
			{
				Position offset = p[i];
				offset.value.y += 0.05f;
				FireLasers(it.world(), offset, o);
				it.entity(i).remove<Firing>();
			}
		}
		
		// process any cached button events after the loop (happens multiple times per frame)
		ProcessInputEvents(it.world());
	});

	game->system<Enemy, Health>("Bomb System")
		.kind(flecs::PostUpdate)
		.iter([this](flecs::iter it, Enemy *e, Health *h) {
		// if you have no health left be destroyed
		if (bomb) for (auto i : it) h[i].value = 0;
	});

	game->system<Player>("Bomb Reset")
		.kind(flecs::PreUpdate)
		.each([this](Player) {
		bomb = false;
	});

	// Create an event cache for when the spacebar/'A' button is pressed
	pressEvents.Create(Max_Frame_Events); // even 32 is probably overkill for one frame
		
	// register for keyboard and controller events
	bufferedInput.Register(pressEvents);
	controllerInput.Register(pressEvents);

    // Setup HUD UI
    auto hudCanvas = _game->entity("HUDCanvas").set<UICanvas>({
        RenderSystem::RegisterUIFont("../Assets/Fonts/Pixel.fnt", "../Assets/Textures/Pixel.tga"),
        RenderSystem::RegisterUISpriteAtlas("../Assets/Textures/UISpriteAtlas.tga"),
        0,
        false
    });

    auto prevScope = _game->set_scope(hudCanvas);
	_game->entity("EnemiesRemainingLabel")
	.set<UIRect>({ 0.F, -0.95F, 480.F, 32.F })
	.set<UIText>({
		{ 0.5F, 0.5F, 0.F, 1.F },   // Font Color
		{ 0.F, 0.F, 1.F, 1.F },     // Outline Color
		0.55F,                      // Outline Width
		1.5F,                       // Font Scale
		"Enemies Remaining:"      // Text Buffer
	});
	auto enemiesRemainingText = _game->entity("EnemiesRemainingText")
	.set<UIRect>({ 0.8F, -0.95F, 122.F, 200.F })
	.set<UIText>({
		{ 0.5F, 0.5F, 0.F, 1.F },   // Font Color
		{ 0.F, 0.F, 1.F, 1.F },     // Outline Color
		0.55F,                      // Outline Width
		1.5F,                       // Font Scale
		""                          // Text Buffer
	});
	_game->entity("HiScoreLabel")
		.set<UIRect>({ -0.95F, -0.95F, 300.F, 100.F })
		.set<UIText>({
			{ 0.5F, 0.5F, 0.F, 1.F },   // Font Color
			{ 0.F, 0.F, 1.F, 1.F },     // Outline Color
			0.55F,                      // Outline Width
			1.5F,                       // Font Scale
			"Hi-Score: "                   // Text Buffer
			});

	auto hScore =_game->entity("HiScoreText")
		.set<UIRect>({ -0.55F, -0.95F, 200.F, 80.F })
		.set<UIText>({
			{ 0.5F, 0.5F, 0.F, 1.F },   // Font Color
			{ 0.F, 0.F, 1.F, 1.F },     // Outline Color
			0.55F,                      // Outline Width
			1.5F,                       // Font Scale
			"0"                    // Text Buffer
			});
	snprintf(hScore.get_mut<UIText>()->text, 247, "%d", highScore);
    _game->entity("ScoreLabel")
    .set<UIRect>({ -0.9F, 0.8F, 200.0F, 80.F })
    .set<UIText>({
        { 0.5F, 0.5F, 0.F, 1.F },   // Font Color
        { 0.F, 0.F, 1.F, 1.F },     // Outline Color
        0.55F,                      // Outline Width
        1.5F,                       // Font Scale
        "Score: "                   // Text Buffer
    });
    _game->entity("ScoreText")
    .set<UIRect>({ -0.6F, 0.8F, 200.F, 80.F })
    .set<UIText>({
        { 0.5F, 0.5F, 0.F, 1.F },   // Font Color
        { 0.F, 0.F, 1.F, 1.F },     // Outline Color
        0.55F,                      // Outline Width
        1.5F,                       // Font Scale
        "0"                         // Text Buffer
    });
    _game->entity("LivesIcon")
    .set<UIRect>({ 0.3F, 0.7F, 120.F, 120.F })
    .set<UISprite>({
        {
            ((24.F / 512) * 1) + (4.F / 512), ((24.F / 512) * 19) + (4.F / 512),
            28.F / 512, 28.F / 512
        }
    });
    auto livesText = _game->entity("LivesText")
    .set<UIRect>({ 0.5F, 0.8F, 200.F, 80.F })
    .set<UIText>({
        { 0.5F, 0.5F, 0.F, 1.F },   // Font Color
        { 0.F, 0.F, 1.F, 1.F },     // Outline Color
        0.55F,                      // Outline Width
        1.5F,                       // Font Scale
        "0"                         // Text Buffer
    });
    snprintf(livesText.get_mut<UIText>()->text, 247, "x%d", startLives);
    _game->entity("SpecialIcon")
    .set<UIRect>({ 0.6F, 0.7F, 120.F, 120.F })
    .set<UISprite>({
        {
            ((24.F / 512) * 2) + (4.F / 512), ((24.F / 512) * 19) + (4.F / 512),
            28.F / 512, 28.F / 512
        }
    });
    auto specialsText = _game->entity("SpecialsText")
    .set<UIRect>({ 0.8F, 0.8F, 200.F, 80.F })
    .set<UIText>({
        { 0.5F, 0.5F, 0.F, 1.F },   // Font Color
        { 0.F, 0.F, 1.F, 1.F },     // Outline Color
        0.55F,                      // Outline Width
        1.5F,                       // Font Scale
        "0"                         // Text Buffer
    });
    snprintf(specialsText.get_mut<UIText>()->text, 247, "x%d", startSpecials);
    _game->set_scope(prevScope);

	// create the on explode handler
	onExplode.Create([this, scoreToLifeReward, scoreToSpecialReward, readCfg](const GW::GEvent& e) {
		PLAY_EVENT event; PLAY_EVENT_DATA eventData;
		if (+e.Read(event, eventData)) {
			if(event == ENEMY_DESTROYED)
			{
				auto& world = eventData.entity_id.world();
				auto point = world.entity(eventData.entity_id).get<Points>();
				auto gameplayStats = world.get_mut<GameplayStats>();
				auto playerStats = playerEntities[0].get_mut<PlayerStats>();
				gameplayStats->score += point->value;
				snprintf(world.entity("HUDCanvas::ScoreText").get_mut<UIText>()->text, 247, "%d", gameplayStats->score);

				// Updating Hi Score based on if its equal to or greater than score.
				if (highScore < gameplayStats->score)
				{
					snprintf(world.entity("HUDCanvas::HiScoreText").get_mut<UIText>()->text, 247, "%d", gameplayStats->score);
					(*readCfg)["Player"]["savedScore"] = gameplayStats->score;
				}

				playerStats->scoreToNextLifeReward -= point->value;
				if(playerStats->scoreToNextLifeReward < 1)
				{
					playerStats->scoreToNextLifeReward = scoreToLifeReward;
					++playerStats->lives;
					snprintf(world.entity("HUDCanvas::LivesText").get_mut<UIText>()->text, 247, "x%d", playerStats->lives);
				}
				playerStats->scoreToNextSpecialReward -= point->value;
				if(playerStats->scoreToNextSpecialReward < 1)
				{
					playerStats->scoreToNextSpecialReward = scoreToSpecialReward;
					++playerStats->specials;
					snprintf(world.entity("HUDCanvas::SpecialsText").get_mut<UIText>()->text, 247, "x%d", playerStats->specials);
				}
				std::cout << "Enemy Was Destroyed!\n";
				std::cout << "Score: "  << gameplayStats->score;
				std::cout << " Lives: " << playerStats->lives;
				std::cout << " Special Weapon Charges: " << playerStats->specials;
				std::cout << std::endl;
			}
		}
	});
	eventPusher.Register(onExplode);
	return true;
}

// Free any resources used to run this system
bool PlayerLogic::Shutdown()
{
	playerSystem.destruct();
	game.reset();
	gameConfig.reset();

	return true;
}

// Toggle if a system's Logic is actively running
bool PlayerLogic::Activate(bool runSystem)
{
	pressEvents.Clear();
	if(!runSystem) playerEntities[0].remove<Collidable>();
	else  playerEntities[0].add<Collidable>();
	auto gameplayStats = game->get_mut<GameplayStats>();
	auto playerStats = playerEntities[0].get_mut<PlayerStats>();
	game->entity("HUDCanvas").get_mut<UICanvas>()->isVisible = runSystem;
	snprintf(game->entity("HUDCanvas::ScoreText").get_mut<UIText>()->text, 247, "%d", gameplayStats->score);
	snprintf(game->entity("HUDCanvas::LivesText").get_mut<UIText>()->text, 247, "x%d", playerStats->lives);
	snprintf(game->entity("HUDCanvas::SpecialsText").get_mut<UIText>()->text, 247, "x%d", playerStats->specials);
	if (playerSystem.is_alive()) {
		(runSystem) ? 
			playerSystem.enable() 
			: playerSystem.disable();
		return true;
	}
	return false;
}

bool PlayerLogic::ProcessInputEvents(flecs::world& stage)
{
	// pull any waiting events from the event cache and process them
	GW::GEvent event;
	while (+pressEvents.Pop(event)) {
		bool fire = false;
		GController::Events controller;
		GController::EVENT_DATA c_data;
		GBufferedInput::Events keyboard;
		GBufferedInput::EVENT_DATA k_data;
		// these will only happen when needed
		if (+event.Read(keyboard, k_data)) {
			if (keyboard == GBufferedInput::Events::KEYPRESSED) {
				if (k_data.data == keyFire) {
					fire = true;
					chargeStart = stage.time();
				} else if(k_data.data == G_KEY_P) {
					auto& gameStateManager = game->lookup("Game State Manager");
					game->get_mut<GameStateManager>()->target = STATE_PAUSE;
#ifdef DEV_BUILD
				} else if(k_data.data == G_KEY_0) {
					RenderSystem::DebugToggleMeshBoundsDraw();
				} else if(k_data.data == G_KEY_EQUALS) {
					RenderSystem::DebugToggleOrthographicProjection();
#endif
				}
			}
			if (keyboard == GBufferedInput::Events::KEYRELEASED) {
				if (k_data.data == G_KEY_SPACE) {
					chargeEnd = stage.time();
					if (chargeEnd - chargeStart >= chargeTime) {
						fire = true;
					}
				}
			}
			if (keyboard == GBufferedInput::Events::KEYRELEASED) {
				if (k_data.data == G_KEY_B) {
					PlayerStats* stats = stage.lookup("Player One").get_mut<PlayerStats>();
					if (0 < stats->specials) {
						--stats->specials;
						snprintf(game->entity("HUDCanvas::SpecialsText").get_mut<UIText>()->text,
							247, "x%d", stats->specials);
						bomb = true;
					}
				}
			}
		}
		else if (+event.Read(controller, c_data)) {
			if (controller == GController::Events::CONTROLLERBUTTONVALUECHANGED) {
				if (c_data.inputValue > 0 && c_data.inputCode == G_SOUTH_BTN)
					fire = true;
			}
		}
		if (fire) {
			// grab player one and set them to a firing state
			stage.entity("Player One").add<Firing>();
		}
	}
	return true;
}

// play sound and launch two laser rounds
bool PlayerLogic::FireLasers(flecs::world& stage, Position origin, const Orientation* orient)
{
	// Grab the prefab for a laser round
	flecs::entity bullet;
	RetreivePrefab("Lazer Bullet", bullet);
	Velocity v = *bullet.get<Velocity>();
	v.value.y *= orient->target.data[0];

	//	possible future feature
	/*float x, y;
	x = v.value.x; y = v.value.y;
	v.value.x = x * orient->target.data[0] - y * orient->target.data[1];
	v.value.y = x * orient->target.data[1] + y * orient->target.data[0];*/

	origin.value.x -= 2.f;
	auto laserLeft = stage.entity().is_a(bullet)
		.set<Position>(origin)
		.set<Velocity>(v)
		.set<AlliedWith>({ PLAYER });
	origin.value.x += 2.f;
	auto laserRight = stage.entity().is_a(bullet)
		.set<Position>(origin)
		.set<Velocity>(v)
		.set<AlliedWith>({ PLAYER });
	// if this shot is charged
	if (chargeEnd - chargeStart >= chargeTime) {
		chargeEnd = chargeStart;
		laserLeft.set<ChargedShot>({ 2 });
		laserRight.set<ChargedShot>({ 2 });
	}

	// play the sound of the Lazer prefab
	GW::AUDIO::GSound shoot = *bullet.get<GW::AUDIO::GSound>();
	shoot.Play();

	return true;
}