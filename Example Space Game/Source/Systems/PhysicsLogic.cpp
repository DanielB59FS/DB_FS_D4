#include "PhysicsLogic.h"
#include "../Components/Physics.h"
#include "../Components/Identification.h"
#include "../Systems/RenderLogic.h"
#include "../Events/Playevents.h"


using namespace TeamYellow;

bool PhysicsLogic::Init(	std::shared_ptr<flecs::world> _game, 
								std::weak_ptr<const GameConfig> _gameConfig)
{	
	// save a handle to the ECS & game settings
	game = _game;
	gameConfig = _gameConfig;
	// **** MOVEMENT ****
	game->system<const Position, const Player>("UpdatePlayerPosition")
		.each([this](flecs::entity e, const Position& p, const Player& _) {
		playerPosition = p.value;
	});
	// update velocity by acceleration
	accSystem = game->system<Velocity, const Acceleration>("Acceleration System")
		.each([](flecs::entity e, Velocity& v, const Acceleration &a) {
		GW::MATH2D::GVECTOR2F accel;
		GW::MATH2D::GVector2D::Scale2F(a.value, e.delta_time(), accel);
		GW::MATH2D::GVector2D::Add2F(accel, v.value, v.value);
	});
	// update position by velocity
	transSystem = game->system<Position, const Velocity>("Translation System")
		.each([&](flecs::entity e, Position& p, const Velocity &v) {
		GW::MATH2D::GVECTOR2F speed;
		GW::MATH2D::GVector2D::Scale2F(v.value, e.delta_time(), speed);
		if (e.has<Enemy>() && !e.has<Bullet>())
			speed.x += (playerPosition.x - p.value.x) * e.delta_time() * 0.25;
		// adding is simple but doesn't account for orientation
		GW::MATH2D::GVector2D::Add2F(speed, p.value, p.value);
	});
	// **** CLEANUP ****
	// clean up any objects that end up offscreen
	cleanSystem = game->system<const Position, const Gameobject>("Cleanup System")
		.each([this](flecs::entity e, const Position& p, const Gameobject& _) {
		if (p.value.x > 45.f || p.value.x < -45.f ||
			p.value.y > playerPosition.y + 80.f || p.value.y < playerPosition.y - 80.f) {
				e.destruct();
		}
	});
	// **** COLLISIONS ****
	// due to wanting to loop through all collidables at once, we do this in two steps:
	// 1. A System will gather all collidables into a shared std::vector
	// 2. A second system will run after, testing/resolving all collidables against each other
	queryCache = game->query<Collidable, StaticMeshComponent, Position, Orientation, Scale>();
	// only happens once per frame at the very start of the frame
	struct CollisionSystem {}; // local definition so we control iteration count (singular)
	game->entity("Detect-Collisions").add<CollisionSystem>();
	game->system<CollisionSystem>()
		.each([this](CollisionSystem& s) {
		const auto& meshBounds = RenderSystem::GetMeshBoundsVector();
		// collect any and all collidable objects
		queryCache.each([this, &meshBounds](flecs::entity e, Collidable& c, StaticMeshComponent& sm, Position& p, Orientation& o, Scale& s) {
			// create a 3x3 matrix for transformation
			GW::MATH2D::GMATRIX3F matrix = {
				s.value.x * o.value.row1.x, o.value.row1.y, 0,
				o.value.row2.x, s.value.y * o.value.row2.y, 0,
				p.value.x, p.value.y, 1
			};
			GW::MATH2D::GVECTOR3F min = { meshBounds[sm.meshID].min.x, -meshBounds[sm.meshID].max.y, 1 };
			GW::MATH2D::GVECTOR3F max = { meshBounds[sm.meshID].max.x, -meshBounds[sm.meshID].min.y, 1 };
			GW::MATH2D::GMatrix2D::MatrixXVector3F(matrix, min, min);
			GW::MATH2D::GMatrix2D::MatrixXVector3F(matrix, max, max);
			SHAPE polygon; // compute buffer for this objects polygon
			// This is critical, if you want to store an entity handle it must be mutable
			polygon.owner = e; // allows later changes
			polygon.bounds = { { min.x, min.y}, {max.x, max.y} };
			// add to vector
			testCache.push_back(polygon);
		});
		// loop through the testCahe resolving all collisions
		for (int i = 0; i < testCache.size(); ++i) {
			// the inner loop starts at the entity after you so you don't double check collisions
			for (int j = i + 1; j < testCache.size(); ++j) {

				// test the two world space polygons for collision
				// possibly make this cheaper by leaving one of them local and using an inverse matrix
				GW::MATH2D::GCollision2D::GCollisionCheck2D result;
				GW::MATH2D::GCollision2D::TestRectangleToRectangle2F(
					testCache[i].bounds, testCache[j].bounds, result);
				if (result == GW::MATH2D::GCollision2D::GCollisionCheck2D::COLLISION) {
					// Create an ECS relationship between the colliders
					// Each system can decide how to respond to this info independently
					testCache[j].owner.add<CollidedWith>(testCache[i].owner);
					testCache[i].owner.add<CollidedWith>(testCache[j].owner);
				}
			}
		}
		// wipe the test cache for the next frame (keeps capacity intact)
		testCache.clear();
	});
	return true;
}

bool PhysicsLogic::Activate(bool runSystem)
{
	if (runSystem) {
		accSystem.enable();
		transSystem.enable();
		cleanSystem.enable();
	}
	else {
		accSystem.disable();
		transSystem.disable();
		cleanSystem.disable();
	}
	return true;
}

bool PhysicsLogic::Shutdown()
{
	queryCache.destruct(); // fixes crash on shutdown
	game->entity("Acceleration System").destruct();
	game->entity("Translation System").destruct();
	game->entity("Cleanup System").destruct();
	return true;
}
