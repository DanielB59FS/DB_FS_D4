#ifndef RENDERLOGIC_H
#define RENDERLOGIC_H

#include "../GameConfig.h"

namespace TeamYellow
{
namespace RenderSystem
{
bool InitBackend(std::weak_ptr<const GameConfig> _gameConfig, GW::GRAPHICS::GVulkanSurface _vulkan, GW::SYSTEM::GWindow _window);

bool InitSystems(std::shared_ptr<flecs::world> _game);
bool ExitSystems();

uint32_t RegisterSkybox(const char* _cubeMapTexturePath);

uint32_t RegisterMesh(const char* _meshPath, const char* _baseTexturePath);

uint32_t RegisterUIFont(const char* _fontLayoutPath, const char* _atlasTexturePath);

uint32_t RegisterUISpriteAtlas(const char* _atlasTexturePath);
#ifdef DEV_BUILD
void DebugToggleMeshBoundsDraw();
void DebugToggleOrthographicProjection();
#endif

struct MeshBounds {
    GW::MATH::GVECTORF min;
    GW::MATH::GVECTORF max;
};

const std::vector<MeshBounds>& GetMeshBoundsVector();

};
};

#endif