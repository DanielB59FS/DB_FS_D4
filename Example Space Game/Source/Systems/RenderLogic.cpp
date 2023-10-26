#include "RenderLogic.h"

#include "../Components/Physics.h"
#include "../Components/Visuals.h"
#include "../Components/Identification.h"

#include "../Utils/h2bParser.h"

using namespace GW::MATH;
using GVulkanSurface = GW::GRAPHICS::GVulkanSurface;
using GWindow = GW::SYSTEM::GWindow;

namespace TeamYellow
{
/*===========================================================================*/
/* PRIVATE STRUCTURE DEFINITIONS                                             */
/*===========================================================================*/
namespace RenderSystem
{
/*===========================================================================*/
/* On-Disk Resource Helper Structs                                           */
/*===========================================================================*/
/* Shader SPIR-V Data                                                        */
/*---------------------------------------------------------------------------*/
struct ShaderFileData
{
    uint32_t vertexShaderFileSize;
    uint32_t pixelShaderFileSize;
    uint32_t *vertexShaderFileData;
    uint32_t *pixelShaderFileData;
};
/*---------------------------------------------------------------------------*/
/* BMFont File Data                                                          */
/*---------------------------------------------------------------------------*/
struct BMFontChar
{
    uint32_t x, y;
    uint32_t width, height;
    int32_t xoffset, yoffset;
    int32_t xadvance;
    uint32_t page;
};
struct BMFont
{
    uint32_t lineHeight;
    BMFontChar chars[255];
};
/*---------------------------------------------------------------------------*/
/* TARGA Image Header                                                        */
/*---------------------------------------------------------------------------*/
struct TGAHeader
{
    uint8_t idLength;
    uint8_t colorMapType;
    uint8_t imageType;
    uint16_t firstIndex;
    uint16_t length;
    uint8_t entrySize;
    uint16_t xOrigin;
    uint16_t yOrigin;
    uint16_t width;
    uint16_t height;
    uint8_t bitsPerPixel;
    uint8_t alphaBitDepth : 4;
    uint8_t rightToLeft : 1;
    uint8_t topToBottom : 1;
};
/*---------------------------------------------------------------------------*/
/* DDS Image Headers                                                         */
/*---------------------------------------------------------------------------*/
struct DDSPixelFormat
{
    uint32_t size;
    uint32_t flag;
    uint32_t fourCC;
    uint32_t bitCount;
    uint32_t bitMaskR;
    uint32_t bitMaskG;
    uint32_t bitMaskB;
    uint32_t bitMaskA;
};
struct DDSHeaderDX10
{
    uint32_t dxgiFormat;
    uint32_t dimension;
    uint32_t miscFlag;
    uint32_t arraySize;
    uint32_t msicFlags2;
};
struct DDSHeader
{
    uint32_t size;
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitch;
    uint32_t depth;
    uint32_t mipCount;
    uint32_t pad[11];
    DDSPixelFormat pixelFormat;
    uint32_t caps;
    uint32_t caps2;
    uint32_t caps3;
    uint32_t caps4;
    uint32_t pad2;
};
/*===========================================================================*/
/* In-Memory Draw Resource Representations                                   */
/*===========================================================================*/
/* Per-Vertex Data for UI SDF Pipeline                                       */
/*---------------------------------------------------------------------------*/
struct FontVertex
{
    GW::MATH2D::GVECTOR2F pos;
    GW::MATH2D::GVECTOR2F uv;
};
/*---------------------------------------------------------------------------*/
/* Batch of Font Vertices to draw together, grouped by Font ID and Canvas    */
/*---------------------------------------------------------------------------*/
struct FontBatch
{
    uint32_t fontID;
    uint32_t indexCount;
    uint32_t indexOffset;
    float outlineWidth;
    UIRect rect;
    GVECTORF fontColor, outlineColor;
};
/*---------------------------------------------------------------------------*/
/* Single Sprite Instance within a Canvas                                    */
/*---------------------------------------------------------------------------*/
struct SpriteInstance
{
    GW::MATH::GVECTORF srcRect;
    GW::MATH::GVECTORF dstRect;
};
/*---------------------------------------------------------------------------*/
/* Sprite Batch (Not grouped by specific Sprite, just by Canvas currently)   */
/*---------------------------------------------------------------------------*/
struct SpriteBatch
{
    uint32_t spriteAtlasID;
    uint32_t instanceOffset;
    uint32_t instanceCount;
};
/*---------------------------------------------------------------------------*/
/* CPU-Side Info about Mesh Vertex and Index Data. Actual Data stored in a   */
/* unified GPU-side Buffer.                                                  */
/*---------------------------------------------------------------------------*/
struct Mesh
{
    uint32_t vertexCount;
    uint32_t vertexOffset;
    uint32_t indexCount;
    uint32_t indexOffset;
};
/*---------------------------------------------------------------------------*/
/* Per-Instance Rate Vertex Buffer Information                               */
/*---------------------------------------------------------------------------*/
struct MeshInstanceData
{
    GMATRIXF transform;
    GVECTORF bloomColor;
    uint32_t isGameObject;
};
/*---------------------------------------------------------------------------*/
/* Batch of Mesh Instances grouped by Mesh ID                                */
/*---------------------------------------------------------------------------*/
struct MeshBatch
{
    uint32_t meshID;
    uint32_t instanceCount;
    uint32_t instanceOffset;
};
}
/*===========================================================================*/
/* PRIVATE FUNCTION DECLARATIONS                                             */
/*===========================================================================*/
namespace RenderSystem
{
/*===========================================================================*/
/* Command Buffer Recording and Submission                                   */
/*===========================================================================*/
void SubmitShadowMapDrawCommands    (uint32_t bufferIndex);
void SubmitGameObjectDrawCommands   (uint32_t bufferIndex, const Skybox* skybox);
void SubmitBloomBlurDrawCommands    (uint32_t bufferIndex);
void SubmitUIDrawCommands           (uint32_t bufferIndex);
void SubmitPresentCommandsAndWait   (uint32_t bufferIndex);
/*===========================================================================*/
/* Render Pipeline Objects                                                   */
/*===========================================================================*/
void CreateShaderModules            (VkDevice _device, std::shared_ptr<const GameConfig> _readCfg);
void CreateImmutableSamplers        (VkDevice _device);
void CreateDescriptorSetLayouts     (VkDevice _device);
void CreatePipelineLayouts          (VkDevice _device);
/*---------------------------------------------------------------------------*/
/* CreateRenderPasses                                                        */
/*---------------------------------------------------------------------------*/
void CreateShadowMapRenderPass      (VkDevice _device);
void CreateStaticMeshRenderPass     (VkDevice _device);
void CreateBlurRenderPass          (VkDevice _device);
void CreateUIRenderPass             (VkDevice _device);
/*---------------------------------------------------------------------------*/
/* CreatePipelines                                                           */
/*---------------------------------------------------------------------------*/
void CreateShadowMapPipeline        (VkDevice _device);
void CreateStaticMeshPipeline       (VkDevice _device);
void CreateBlurPipeline             (VkDevice _device);
void CreateSkyboxPipeline           (VkDevice _device);
#ifdef DEV_BUILD
void CreateDebugColliderPipeline    (VkDevice _device);
#endif
void CreateUISDFPipeline            (VkDevice _device);
void CreateUIBlitPipeline           (VkDevice _device);
void CreatePresentPipeline          (VkDevice _device);
void DestroyShaderModules           (VkDevice _device);
void DestroyImmutableSamplers       (VkDevice _device);
void DestroyDescriptorSetLayouts    (VkDevice _device);
void DestroyPipelineLayouts         (VkDevice _device);
void DestroyRenderPasses            (VkDevice _device);
void DestroyPipelines               (VkDevice _device);
/*===========================================================================*/
/* Render Resource Objects                                                   */
/*===========================================================================*/
void CreatePersistentResources      (VkPhysicalDevice _physicalDevice, VkDevice _device);
void CreatePerFrameResources        (VkPhysicalDevice _physicalDevice, VkDevice _device, uint32_t bufferCount);
void DestroyPersistentResources     (VkDevice _device);
void DestroyPerFrameResources       (VkDevice _device, uint32_t bufferCount);
/*===========================================================================*/
/* Resource Loading                                                          */
/*===========================================================================*/
void LoadShaderFileData (const char *vertexShaderPath, const char *pixelShaderPath, GW::SYSTEM::GFile& _fileInterface, ShaderFileData &data);
void FreeShaderFileData (ShaderFileData &data);
/*---------------------------------------------------------------------------*/
/* Font Data                                                                 */
/*---------------------------------------------------------------------------*/
void LoadBMFont         (const char* _filePath, BMFont& _font);
/*---------------------------------------------------------------------------*/
/* Texture Data                                                              */
/*---------------------------------------------------------------------------*/
void LoadTGATexture     (const char* _filePath, VkPhysicalDevice _physicalDevice, VkDevice _device, VkImage* texture, VkDeviceMemory* textureMemory, VkImageView* textureSRV);
void LoadDDSTexture     (const char* _filePath, VkPhysicalDevice _physicalDevice, VkDevice _device, VkImage* texture, VkDeviceMemory* textureMemory, VkImageView* textureSRV);
void LoadDDSCubemap     (const char* _filePath, VkPhysicalDevice _physicalDevice, VkDevice _device, VkImage* texture, VkDeviceMemory* textureMemory, VkImageView* textureSRV);
/*---------------------------------------------------------------------------*/
/* Mesh Data                                                                 */
/*---------------------------------------------------------------------------*/
void LoadH2BMesh        (const char* _filePath, Mesh& _mesh, MeshBounds& _bounds);
#ifdef DEV_BUILD
void UploadMeshBoundsDataToGPU(const MeshBounds& _bounds, uint32_t meshIndex);
#endif
/*===========================================================================*/
/* Miscellaneous Vulkan Helpers                                              */
/*===========================================================================*/
void GetSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceFormatKHR& out);
void GetSurfaceExtent(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkExtent2D& out);
}
/*===========================================================================*/
/* PRIVATE VARIABLE DECLARATIONS                                             */
/*===========================================================================*/
namespace RenderSystem
{
#define MAX_MESH_COUNT 128 // Note: Currently only used for Descriptor Set Counts
/*===========================================================================*/
/* Gateware Objects                                                          */
/*===========================================================================*/
GW::GRAPHICS::GVulkanSurface    vulkan;
GW::SYSTEM::GFile               fileInterface;
GW::CORE::GEventReceiver        shutdown;
GW::CORE::GEventReceiver        resize;
/*===========================================================================*/
/* Flecs Objects                                                             */
/*===========================================================================*/
/* Systems                                                                   */
/*---------------------------------------------------------------------------*/
flecs::system                   updateCameraPosition;
flecs::system                   copyRenderingData;
flecs::system                   present;
/*---------------------------------------------------------------------------*/
/* Queries                                                                   */
/*---------------------------------------------------------------------------*/
flecs::query<UIRect, UIText>    uiTextQuery;
flecs::query<UIRect, UISprite>  uiSpriteQuery;
flecs::query<UICanvas>          uiCanvasTextQuery;
flecs::query<UICanvas>          uiCanvasSpriteQuery;
flecs::query<Position, Orientation, Scale, StaticMeshComponent, Foreground>
                                foregroundSortedQuery;
flecs::query<Position, Orientation, Scale, StaticMeshComponent, Gameobject>
                                gameObjectSortedQuery;
flecs::query<Position, Orientation, Scale, StaticMeshComponent, Background>
                                backgroundSortedQuery;
flecs::query<Position, Orientation, Scale, StaticMeshComponent, Floor>
                                floorSortedQuery;
/*===========================================================================*/
/* Vulkan Objects - queried from Gateware so we don't create/destroy them    */
/*===========================================================================*/
VkPhysicalDevice                physicalDevice;
VkDevice                        device;
VkQueue                         graphicsQueue;
VkCommandPool                   commandPool;
/*===========================================================================*/
/* Swapchain Info used to create Render Targets that get blit to the         */
/* Swapchain image in the Present Pass                                       */
/*===========================================================================*/
uint32_t                        swapchainBufferCount;
VkSurfaceFormatKHR              swapchainFormat;
VkExtent2D                      swapchainExtent;
/*===========================================================================*/
/* Shadow Map Render Pass                                                    */
/*===========================================================================*/
VkRenderPass                    shadowMapRenderPass;
VkPipelineLayout                shadowMapPipelineLayout;
VkShaderModule                  shadowMapVertexShader;
VkShaderModule                  shadowMapPixelShader;
VkPipeline                      shadowMapPipeline;
/*===========================================================================*/
/* Static Mesh Render Pass                                                   */
/*===========================================================================*/
VkRenderPass                    staticMeshRenderPass;
VkSampler                       staticMeshTextureSampler;
VkDescriptorSetLayout           staticMeshDescriptorSetLayout;
VkPipelineLayout                staticMeshPipelineLayout;
VkShaderModule                  staticMeshVertexShader;
VkShaderModule                  staticMeshPixelShader;
VkPipeline                      staticMeshPipeline;
/*===========================================================================*/
/* Blur Render Pass                                                   */
/*===========================================================================*/
VkRenderPass                    blurRenderPass;
VkShaderModule                  blurVertexShader;
VkShaderModule                  blurPixelShader;
VkPipelineLayout                blurPipelineLayout;
VkPipeline                      blurPipeline;
/*===========================================================================*/
/* Skybox Render Pass                                                        */
/*===========================================================================*/
VkPipelineLayout                skyBoxPipelineLayout;
VkShaderModule                  skyBoxVertexShader;
VkShaderModule                  skyBoxPixelShader;
VkPipeline                      skyBoxPipeline;
#ifdef DEV_BUILD
/*===========================================================================*/
/* Debug Collider Render Pass                                                */
/*===========================================================================*/
VkShaderModule                  debugColliderVertexShader;
VkShaderModule                  debugColliderPixelShader;
VkPipeline                      debugColliderPipeline;
#endif
/*===========================================================================*/
/* UI Render Pass                                                            */
/*===========================================================================*/
VkRenderPass                    uiRenderPass;
VkDescriptorSetLayout           uiSDFDescriptorSetLayout;
VkPipelineLayout                uiSDFPipelineLayout;
VkShaderModule                  uiSDFVertexShader;
VkShaderModule                  uiSDFPixelShader;
VkPipeline                      uiSDFPipeline;
VkSampler                       uiBlitSampler;
VkDescriptorSetLayout           uiBlitDescriptorSetLayout;
VkPipelineLayout                uiBlitPipelineLayout;
VkShaderModule                  uiBlitVertexShader;
VkShaderModule                  uiBlitPixelShader;
VkPipeline                      uiBlitPipeline;
/*===========================================================================*/
/* Present Render Pass                                                       */
/*===========================================================================*/
VkRenderPass                    presentRenderPass;
VkSampler                       presentSampler;
VkDescriptorSetLayout           presentDescriptorSetLayout;
VkPipelineLayout                presentPipelineLayout;
VkShaderModule                  presentVertexShader;
VkShaderModule                  presentPixelShader;
VkPipeline                      presentPipeline;
/*===========================================================================*/
/* Staging Buffer                                                            */
/*===========================================================================*/
VkBuffer                        stagingBuffer;
VkDeviceMemory                  stagingMemory;
void*                           stagingMappedMemory;
/*===========================================================================*/
/* Static Mesh Render Resources                                              */
/*===========================================================================*/
/* These are populated whenever RegisterMesh is called on Entity Load and    */
/* are (not supposed to be) not modified between Draw calls so we don't need */
/* one per Frame for these                                                   */
/*---------------------------------------------------------------------------*/
VkBuffer                        meshVertexDataBuffer;
VkDeviceMemory                  meshVertexDataMemory;
VkBuffer                        meshIndexDataBuffer;
VkDeviceMemory                  meshIndexDataMemory;
#ifdef DEV_BUILD
VkBuffer                        debugColliderVertexDataBuffer;
VkDeviceMemory                  debugColliderVertexDataMemory;
VkBuffer                        debugColliderIndexDataBuffer;
VkDeviceMemory                  debugColliderIndexDataMemory;
#endif
/*---------------------------------------------------------------------------*/
/* These contain Instance data sorted by MeshID and are updated every frame  */
/* so we need one buffer per Frame so that we don't overwrite the data being */
/* used by a frame currently in flight.                                      */
/*---------------------------------------------------------------------------*/
std::vector<VkBuffer>           instanceVertexDataBuffers;
std::vector<VkDeviceMemory>     instanceVertexDataMemoryBlocks;
/*---------------------------------------------------------------------------*/
/* Render Pass Attachments - 1 RTV + 1 DTV per frame                         */
/*---------------------------------------------------------------------------*/
std::vector<VkImage>            gameObjectRTs;
std::vector<VkDeviceMemory>     gameObjectRTMemBlocks;
std::vector<VkImageView>        gameObjectRTVs;
std::vector<VkImage>            gameObjectBloomRTs;
std::vector<VkDeviceMemory>     gameObjectBloomRTMemBlocks;
std::vector<VkImageView>        gameObjectBloomRTVs;
std::vector<VkImage>            gameObjectDTs;
std::vector<VkDeviceMemory>     gameObjectDTMemBlocks;
std::vector<VkImageView>        gameObjectDTVs;
std::vector<VkFramebuffer>      gameObjectFramebuffers;
/*---------------------------------------------------------------------------*/
/* Read-only Texture Data and Descriptor Sets for each Registered Mesh       */
/*---------------------------------------------------------------------------*/
std::vector<VkImage>            materialTextures;
std::vector<VkDeviceMemory>     materialTextureMemBlocks;
std::vector<VkImageView>        materialTextureSRVs;
VkDescriptorPool                materialDescriptorPool;
std::vector<VkDescriptorSet>    materialDescriptorSets;
/*===========================================================================*/
/* Shadow Map Render Resources                                               */
/*===========================================================================*/
std::vector<VkImage>            shadowMapDTs;
std::vector<VkDeviceMemory>     shadowMapDTMemBlocks;
std::vector<VkImageView>        shadowMapDTVs;
std::vector<VkFramebuffer>      shadowMapFramebuffers;
VkDescriptorPool                shadowMapDescriptorPool;
std::vector<VkDescriptorSet>    shadowMapDescriptorSets;
/*===========================================================================*/
/* Cubemap Render Resources                                                  */
/*===========================================================================*/
/* Read-only Cubemap Data and Descriptor Sets for each Registered Mesh       */
/*---------------------------------------------------------------------------*/
std::vector<VkImage>            cubeMapTextures;
std::vector<VkDeviceMemory>     cubeMapTextureMemBlocks;
std::vector<VkImageView>        cubeMapTextureSRVs;
VkDescriptorPool                cubeMapDescriptorPool;
std::vector<VkDescriptorSet>    cubeMapDescriptorSets;
/*===========================================================================*/
/* Blur Render Resources                                                     */
/*===========================================================================*/
std::vector<VkImage>            blurRTs;
std::vector<VkDeviceMemory>     blurRTMemBlocks;
std::vector<VkImageView>        blurRTVs;
std::vector<VkFramebuffer>      blurPingFramebuffers;
std::vector<VkFramebuffer>      blurPongFramebuffers;
VkDescriptorPool                blurDescriptorPool;
std::vector<VkDescriptorSet>    blurPingDescriptorSets;
std::vector<VkDescriptorSet>    blurPongDescriptorSets;
/*===========================================================================*/
/* UI Render Resources                                                       */
/*===========================================================================*/
/* UI Vertex/Index Buffers                                                   */
/*---------------------------------------------------------------------------*/
/* These are updated every frame so we have one per swapchain buffer Image   */
/*---------------------------------------------------------------------------*/
std::vector<VkBuffer>           uiSDFVertexDataBuffers;
std::vector<VkDeviceMemory>     uiSDFVertexDataMemoryBlocks;
std::vector<VkBuffer>           uiSDFIndexDataBuffers;
std::vector<VkDeviceMemory>     uiSDFIndexDataMemoryBlocks;
std::vector<VkBuffer>           uiBlitInstanceDataBuffers;
std::vector<VkDeviceMemory>     uiBlitInstanceDataMemoryBlocks;
/*---------------------------------------------------------------------------*/
/* Render Pass Attachments - 1 RTV per frame                                 */
/*---------------------------------------------------------------------------*/
std::vector<VkImage>            uiRTs;
std::vector<VkDeviceMemory>     uiRTMemBlocks;
std::vector<VkImageView>        uiRTVs;
std::vector<VkFramebuffer>      uiFramebuffers;
/*---------------------------------------------------------------------------*/
/* Read-only Atlas Texture Data and Descriptor Sets for each Registered Font */
/*---------------------------------------------------------------------------*/
std::vector<VkImage>            fontAtlasTextures;
std::vector<VkDeviceMemory>     fontAtlasTextureMemBlocks;
std::vector<VkImageView>        fontAtlasTextureSRVs;
VkDescriptorPool                uiSDFDescriptorPool;
std::vector<VkDescriptorSet>    uiSDFDescriptorSets;
std::vector<VkImage>            spriteAtlasTextures;
std::vector<VkDeviceMemory>     spriteAtlasTextureMemBlocks;
std::vector<VkImageView>        spriteAtlasTextureSRVs;
VkDescriptorPool                uiBlitDescriptorPool;
std::vector<VkDescriptorSet>    uiBlitDescriptorSets;
/*===========================================================================*/
/* Present Pass Descriptors                                                  */
/*===========================================================================*/
VkDescriptorPool                perFrameDescriptorPool;
std::vector<VkDescriptorSet>    perFrameDescriptorSets;
/*===========================================================================*/
/* CPU-side Render Resources                                                 */
/*===========================================================================*/
/* Font Layout Data                                                          */
/*---------------------------------------------------------------------------*/
std::vector<BMFont>             fontLayouts;
/*---------------------------------------------------------------------------*/
/* Per-Frame Font Batch List - Cleared Every Frame                           */
/*---------------------------------------------------------------------------*/
std::vector<FontBatch>          fontBatches;
/*---------------------------------------------------------------------------*/
/* Per-Frame Sprite Batch List - Cleared Every Frame                         */
/*---------------------------------------------------------------------------*/
std::vector<SpriteBatch>        spriteBatches;
/*---------------------------------------------------------------------------*/
/* Mesh Vertex/Index Offset Data and Mesh Bounds Data                        */
/*---------------------------------------------------------------------------*/
std::vector<Mesh>               meshVector;
std::vector<MeshBounds>         meshBoundsVector;
/*---------------------------------------------------------------------------*/
/* Per-Frame Mesh Batch List - Cleared Every Frame                           */
/*---------------------------------------------------------------------------*/
std::vector<MeshBatch>          foregroundMeshBatchesVector;
std::vector<MeshBatch>          gameobjectMeshBatchesVector;
std::vector<MeshBatch>          backgroundMeshBatchesVector;
/*===========================================================================*/
/* INI Configurable Values                                                   */
/*===========================================================================*/
float                           gameCameraDistanceToGameplayPlane;
float                           foregroundObjectDistanceToGameplayPlane;
float                           backgroundObjectDistanceToGameplayPlane;
float                           gameCameraYPosition;
const GVECTORF                  globalDirectionalLightDir = { 0, -1, 1, 0 };
/*===========================================================================*/
/* Runtime Dependent Values                                                  */
/*===========================================================================*/
uint32_t                        swapchainBufferIndex;
#ifdef DEV_BUILD
bool                            debugDrawMeshBounds;
bool                            debugDrawOrthographic;
#endif
}

bool RenderSystem::InitBackend(std::weak_ptr<const GameConfig> _gameConfig, GVulkanSurface _vulkan, GWindow _window)
{
    _window.GetClientWidth(swapchainExtent.width);
    _window.GetClientHeight(swapchainExtent.height);

    vulkan = _vulkan;
    vulkan.GetPhysicalDevice((void**)&physicalDevice);
    vulkan.GetDevice((void **)&device);
    vulkan.GetGraphicsQueue((void**)&graphicsQueue);
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    vulkan.GetSurface((void**)&surface);
    GetSurfaceFormat(physicalDevice, surface, swapchainFormat);
    GetSurfaceExtent(physicalDevice, surface, swapchainExtent);
    vulkan.GetCommandPool((void**)&commandPool);
    vulkan.GetRenderPass((void**)&presentRenderPass);

    fileInterface.Create();
    std::shared_ptr<const GameConfig> readCfg = _gameConfig.lock();
    gameCameraDistanceToGameplayPlane = (*readCfg).at("RenderSystem").at("CameraDistanceToGameplayPlane").as<float>();
    foregroundObjectDistanceToGameplayPlane = (*readCfg).at("RenderSystem").at("ForegroundObjectDistanceToGameplayPlane").as<float>();
    backgroundObjectDistanceToGameplayPlane = (*readCfg).at("RenderSystem").at("BackgroundObjectDistanceToGameplayPlane").as<float>();
    CreateShaderModules(device, readCfg);
    readCfg.reset();

    CreateImmutableSamplers(device);
    CreateDescriptorSetLayouts(device);
    CreatePipelineLayouts(device);
    CreateShadowMapRenderPass(device);
    CreateShadowMapPipeline(device);
    CreateStaticMeshRenderPass(device);
    CreateBlurRenderPass(device);
    CreateBlurPipeline(device);
    CreateSkyboxPipeline(device);
    CreateStaticMeshPipeline(device);
#ifdef DEV_BUILD
    CreateDebugColliderPipeline(device);
#endif
    CreateUIRenderPass(device);
    CreateUISDFPipeline(device);
    CreateUIBlitPipeline(device);
    CreatePresentPipeline(device);

    CreatePersistentResources(physicalDevice, device);
    vulkan.GetSwapchainImageCount(swapchainBufferCount);
    CreatePerFrameResources(physicalDevice, device, swapchainBufferCount);

    shutdown.Create(_vulkan, [&]() {
        if (+shutdown.Find(GVulkanSurface::Events::RELEASE_RESOURCES, true)) {
                vkDeviceWaitIdle(device);

                DestroyPerFrameResources(device, swapchainBufferCount);
                DestroyPersistentResources(device);

                DestroyPipelines(device);
                DestroyShaderModules(device);
                DestroyRenderPasses(device);
                DestroyPipelineLayouts(device);
                DestroyDescriptorSetLayouts(device);
                DestroyImmutableSamplers(device);
        }
    });
    resize.Create(_vulkan, [&]() {
        GVulkanSurface::EVENT_DATA resizeEventData;
        if (+resize.Find(GVulkanSurface::Events::REBUILD_PIPELINE, true, resizeEventData)) {
            vkDeviceWaitIdle(device);

            DestroyPerFrameResources(device, swapchainBufferCount);

            swapchainExtent.width = resizeEventData.surfaceExtent[0];
            swapchainExtent.height = resizeEventData.surfaceExtent[1];
            vulkan.GetSwapchainImageCount(swapchainBufferCount);

            CreatePerFrameResources(physicalDevice, device, swapchainBufferCount);
        }
    });

    return false;
}

bool RenderSystem::InitSystems(std::shared_ptr<flecs::world> _game)
{
    struct VulkanBackend {};
    _game->entity("Vulkan Backend").add<VulkanBackend>();

    uiSpriteQuery = _game->query_builder<UIRect, UISprite>().build();
    uiTextQuery = _game->query_builder<UIRect, UIText>().build();
    uiCanvasTextQuery = _game->query_builder<UICanvas>()
    .order_by<UICanvas>([](flecs::entity_t e1, const UICanvas *ui1, flecs::entity_t e2, const UICanvas *ui2) {
        return (ui1->fontID > ui2->fontID) - (ui1->fontID < ui2->fontID);
    })
    .build();
    uiCanvasSpriteQuery = _game->query_builder<UICanvas>()
    .order_by<UICanvas>([](flecs::entity_t e1, const UICanvas *ui1, flecs::entity_t e2, const UICanvas *ui2) {
        return (ui1->spriteAtlasID > ui2->spriteAtlasID) - (ui1->spriteAtlasID < ui2->spriteAtlasID);
    })
    .build();

    foregroundSortedQuery = _game->query_builder<Position, Orientation, Scale, StaticMeshComponent, Foreground>()
    .order_by<StaticMeshComponent>([] (flecs::entity_t e1, const StaticMeshComponent *sm1, flecs::entity_t e2, const StaticMeshComponent *sm2) {
            return (sm1->meshID > sm2->meshID) - (sm1->meshID < sm2->meshID);
    })
    .build();
    gameObjectSortedQuery = _game->query_builder<Position, Orientation, Scale, StaticMeshComponent, Gameobject>()
    .order_by<StaticMeshComponent>([] (flecs::entity_t e1, const StaticMeshComponent *sm1, flecs::entity_t e2, const StaticMeshComponent *sm2) {
            return (sm1->meshID > sm2->meshID) - (sm1->meshID < sm2->meshID);
    })
    .build();
    backgroundSortedQuery = _game->query_builder<Position, Orientation, Scale, StaticMeshComponent, Background>()
    .order_by<StaticMeshComponent>([] (flecs::entity_t e1, const StaticMeshComponent *sm1, flecs::entity_t e2, const StaticMeshComponent *sm2) {
            return (sm1->meshID > sm2->meshID) - (sm1->meshID < sm2->meshID);
    })
    .build();
    floorSortedQuery = _game->query_builder<Position, Orientation, Scale, StaticMeshComponent, Floor>()
    .order_by<StaticMeshComponent>([] (flecs::entity_t e1, const StaticMeshComponent *sm1, flecs::entity_t e2, const StaticMeshComponent *sm2) {
            return (sm1->meshID > sm2->meshID) - (sm1->meshID < sm2->meshID);
    })
    .build();

    updateCameraPosition = _game->system<const Position, const Player>()
        .kind(flecs::OnStore)
        .each([&](flecs::entity e, const Position& p, const Player& _) {
            gameCameraYPosition = p.value.y;
        });

    copyRenderingData = _game->system<VulkanBackend>()
     .kind(flecs::OnStore)
     .each([&](flecs::entity e, VulkanBackend& s) {
        vulkan.GetSwapchainCurrentImage(swapchainBufferIndex);
        /*-------------------------------------------------------------------*/
        /* Font Glyph Vertex + Index Data                                    */
        /*-------------------------------------------------------------------*/
        fontBatches.clear();
        uint32_t vertexCount = 0, indexCount = 0;
        FontVertex* vertex = (FontVertex*)stagingMappedMemory;
        uiCanvasTextQuery.each([&vertexCount, &indexCount, &vertex](flecs::entity canvasEntity, const UICanvas& canvas) {
            if(!canvas.isVisible || canvas.fontID == ~(0u)) return;
            uiTextQuery.each([&](flecs::entity textEntity, const UIRect& rect, const UIText& text) {
                if(!textEntity.has(flecs::ChildOf, canvasEntity)) return;
                BMFont& font = fontLayouts[canvas.fontID];
                FontBatch fontBatch = {};
                fontBatch.fontID = canvas.fontID;
                fontBatch.indexOffset = indexCount;
                fontBatch.outlineWidth = text.outlineWidth;
                fontBatch.rect = rect;
                fontBatch.fontColor = text.fontColor;
                fontBatch.outlineColor = text.outlineColor;
                float posX = rect.x;
                float posY = rect.y;
                float scaleX = (36.F / swapchainExtent.width) * text.fontSize;
                float scaleY = (36.F / swapchainExtent.height) * text.fontSize;
                float lineDelta = (font.lineHeight / 36.F) * scaleY;
                for(auto it = &text.text[0]; it != &text.text[247] && *it != '\0'; ++it)
                {
                    if(*it == '\n')
                    {
                        posX = rect.x;
                        posY += lineDelta;
                        ++it;
                        continue;
                    }
                    BMFontChar* fontCharInfo = &font.chars[*it];
                    if(fontCharInfo->width == 0) fontCharInfo->width = 36;
                    /*===================================================================*/
                    /* Glyph Parameters                                                  */
                    /*===================================================================*/
                    /* Glyph Dimensions                                                  */
                    /*-------------------------------------------------------------------*/
                    float charw = (((float)fontCharInfo->width) / 36.F) * scaleX;
                    float charh = (((float)fontCharInfo->height) / 36.F) * scaleY;
                    /*-------------------------------------------------------------------*/
                    /* Font UV                                                           */
                    /*-------------------------------------------------------------------*/
                    float us = ((float)fontCharInfo->x) / 512.F;
                    float ts = ((float)fontCharInfo->y) / 512.F;
                    float ue = ((float)(fontCharInfo->x + fontCharInfo->width)) / 512.F;
                    float te = ((float)(fontCharInfo->y + fontCharInfo->height)) / 512.F;
                    /*-------------------------------------------------------------------*/
                    /* Offsets relative to Cursor Position                               */
                    /*-------------------------------------------------------------------*/
                    float xo = (((float)fontCharInfo->xoffset) / 36.F) * scaleX;
                    float yo = (((float)fontCharInfo->yoffset) / 36.F) * scaleY;
                    /*===========================================================*/
                    /* Write Vertices to Buffer                                  */
                    /*===========================================================*/
                    /* Bottom-Right                                              */
                    /*-----------------------------------------------------------*/
                    vertex[0].pos = GW::MATH2D::GVECTOR2F { posX + charw + xo, posY + yo + charh };
                    vertex[0].uv = GW::MATH2D::GVECTOR2F { ue, te };
                    /*-----------------------------------------------------------*/
                    /* Bottom-Left                                               */
                    /*-----------------------------------------------------------*/
                    vertex[1].pos = GW::MATH2D::GVECTOR2F { posX + xo, posY + yo + charh };
                    vertex[1].uv = GW::MATH2D::GVECTOR2F { us, te };
                    /*-----------------------------------------------------------*/
                    /* Top-Left                                                  */
                    /*-----------------------------------------------------------*/
                    vertex[2].pos = GW::MATH2D::GVECTOR2F { posX + xo, posY + yo };
                    vertex[2].uv = GW::MATH2D::GVECTOR2F { us, ts };
                    /*-----------------------------------------------------------*/
                    /* Top-Right                                                 */
                    /*-----------------------------------------------------------*/
                    vertex[3].pos = GW::MATH2D::GVECTOR2F { posX + charw + xo, posY + yo };
                    vertex[3].uv = GW::MATH2D::GVECTOR2F { ue, ts };
                    /*===========================================================*/
                    vertex = &vertex[4];
                    vertexCount += 4;
                    fontBatch.indexCount += 6;
                    indexCount += 6;
                    float advance = ((float)(fontCharInfo->xadvance) / 36.F) * scaleX;
                    posX += advance;
                }
                fontBatches.push_back(fontBatch);
            });
        });
        if(vertexCount) {
            GvkHelper::copy_buffer(device, commandPool, graphicsQueue,
                stagingBuffer, uiSDFVertexDataBuffers[swapchainBufferIndex],
                sizeof(FontVertex) * vertexCount);
        }
        uint32_t* index = (uint32_t*)stagingMappedMemory;
        for(int i = 0; i < vertexCount; i += 4)
        {
            *index++ = i + 0;
            *index++ = i + 1;
            *index++ = i + 2;
            *index++ = i + 2;
            *index++ = i + 3;
            *index++ = i + 0;
        }
        if(indexCount) {
            GvkHelper::copy_buffer(device, commandPool, graphicsQueue,
                stagingBuffer, uiSDFIndexDataBuffers[swapchainBufferIndex],
                sizeof(uint32_t) * indexCount);
        }
        /*-------------------------------------------------------------------*/
        /* UI Sprite Instance Data                                           */
        /*-------------------------------------------------------------------*/
        spriteBatches.clear();
        uint32_t spriteInstanceCount = 0;
        SpriteInstance* instance = (SpriteInstance*)stagingMappedMemory;
        uiCanvasTextQuery.each([&spriteInstanceCount, &instance](flecs::entity canvasEntity, const UICanvas& canvas) {
            if(!canvas.isVisible || canvas.spriteAtlasID == ~(0u)) return;
            SpriteBatch spriteBatch = {};
            spriteBatch.spriteAtlasID = canvas.spriteAtlasID;
            spriteBatch.instanceOffset = spriteInstanceCount;
            uiSpriteQuery.each([&](flecs::entity e, const UIRect& rect, const UISprite& sprite) {
                instance->srcRect = sprite.srcRect;
                instance->dstRect.x = rect.x;
                instance->dstRect.y = rect.y;
                instance->dstRect.z = rect.width / swapchainExtent.width;
                instance->dstRect.w = rect.height / swapchainExtent.height;
                ++instance;
                ++spriteBatch.instanceCount;
                ++spriteInstanceCount;
            });
            spriteBatches.push_back(spriteBatch);
        });
        if(spriteInstanceCount) {
            GvkHelper::copy_buffer(device, commandPool, graphicsQueue,
                stagingBuffer, uiBlitInstanceDataBuffers[swapchainBufferIndex],
                sizeof(SpriteInstance) * spriteInstanceCount);
        }
        /*-------------------------------------------------------------------*/
        /* Static Mesh Instance Data                                         */
        /*-------------------------------------------------------------------*/
        uint32_t instanceCount = 0;
        MeshInstanceData* instanceData = (MeshInstanceData*)stagingMappedMemory;
        MeshBatch meshBatch;
        backgroundMeshBatchesVector.clear();
        meshBatch.meshID = ~(0u);
        backgroundSortedQuery.each([&](flecs::entity e, Position& p, Orientation& o, Scale& s, StaticMeshComponent& sm, Background) {
            if(meshBatch.meshID != sm.meshID)
            {
                if(meshBatch.meshID != ~(0u)) backgroundMeshBatchesVector.push_back(meshBatch);
                meshBatch.meshID = sm.meshID;
                meshBatch.instanceOffset = instanceCount;
                meshBatch.instanceCount = 0;
            }
            instanceData->transform = GW::MATH::GIdentityMatrixF;
            instanceData->transform.row4.x = backgroundObjectDistanceToGameplayPlane;
            instanceData->transform.row4.y = p.value.x;
            instanceData->transform.row4.z = p.value.y;
            instanceData->transform.row1.x = -o.value.row1.x * s.value.x;
            instanceData->transform.row1.z =  o.value.row2.x * s.value.y;
            instanceData->transform.row3.x =  o.value.row1.y * s.value.x;
            instanceData->transform.row3.z = -o.value.row2.y * s.value.y;
            instanceData->transform.row2.y = s.value.z;
            instanceData->isGameObject = 0;
            instanceData->bloomColor = { 0, 0, 0, 1 };
            ++meshBatch.instanceCount;
            ++instanceCount;
            ++instanceData;
        });
        if(meshBatch.meshID != ~(0u)) backgroundMeshBatchesVector.push_back(meshBatch);
        floorSortedQuery.each([&](flecs::entity e, Position& p, Orientation& o, Scale& s, StaticMeshComponent& sm, Floor) {
            if(meshBatch.meshID != sm.meshID)
            {
                if(meshBatch.meshID != ~(0u)) backgroundMeshBatchesVector.push_back(meshBatch);
                meshBatch.meshID = sm.meshID;
                meshBatch.instanceOffset = instanceCount;
                meshBatch.instanceCount = 0;
            }
            instanceData->transform = GW::MATH::GIdentityMatrixF;
            instanceData->transform.row4.y = p.value.x;
            instanceData->transform.row4.z = p.value.y;
            instanceData->transform.row1.x = -o.value.row1.x * s.value.x;
            instanceData->transform.row1.z =  o.value.row2.x * s.value.y;
            instanceData->transform.row3.x =  o.value.row1.y * s.value.x;
            instanceData->transform.row3.z = -o.value.row2.y * s.value.y;
            instanceData->transform.row2.y = s.value.z;
            instanceData->isGameObject = 0;
            instanceData->bloomColor = { 0, 0, 0, 1 };
            ++meshBatch.instanceCount;
            ++instanceCount;
            ++instanceData;
        });
        if(meshBatch.meshID != ~(0u)) backgroundMeshBatchesVector.push_back(meshBatch);
        gameobjectMeshBatchesVector.clear();
        meshBatch.meshID = ~(0u);
        gameObjectSortedQuery.each([&](flecs::entity e, Position& p, Orientation& o, Scale& s, StaticMeshComponent& sm, Gameobject) {
            if(meshBatch.meshID != sm.meshID)
            {
                if(meshBatch.meshID != ~(0u)) gameobjectMeshBatchesVector.push_back(meshBatch);
                meshBatch.meshID = sm.meshID;
                meshBatch.instanceOffset = instanceCount;
                meshBatch.instanceCount = 0;
            }
            instanceData->transform = GW::MATH::GIdentityMatrixF;
            instanceData->transform.row4.y = p.value.x;
            instanceData->transform.row4.z = p.value.y;
            instanceData->transform.row1.x = -o.value.row1.x * s.value.x;
            instanceData->transform.row1.z =  o.value.row2.x * s.value.y;
            instanceData->transform.row3.x =  o.value.row1.y * s.value.x;
            instanceData->transform.row3.z = -o.value.row2.y * s.value.y;
            instanceData->transform.row2.y = s.value.z;
            instanceData->isGameObject = 1;
            instanceData->bloomColor = { 0, 0, 0, 1 };
            if(e.has<Bullet>())
            {
                instanceData->bloomColor.x = 1;
                if(e.get<AlliedWith>()->faction == Faction::PLAYER)
                {
                    instanceData->bloomColor.y = 1;
                    instanceData->bloomColor.z = 1;
                }
            }
            ++meshBatch.instanceCount;
            ++instanceCount;
            ++instanceData;
        });
        if(meshBatch.meshID != ~(0u)) gameobjectMeshBatchesVector.push_back(meshBatch);
        foregroundMeshBatchesVector.clear();
        meshBatch.meshID = ~(0u);
        foregroundSortedQuery.each([&](flecs::entity e, Position& p, Orientation& o, Scale& s, StaticMeshComponent& sm, Foreground) {
            if(meshBatch.meshID != sm.meshID)
            {
                if(meshBatch.meshID != ~(0u)) foregroundMeshBatchesVector.push_back(meshBatch);
                meshBatch.meshID = sm.meshID;
                meshBatch.instanceOffset = instanceCount;
                meshBatch.instanceCount = 0;
            }
            instanceData->transform = GW::MATH::GIdentityMatrixF;
            instanceData->transform.row4.x = foregroundObjectDistanceToGameplayPlane;
            instanceData->transform.row4.y = p.value.x;
            instanceData->transform.row4.z = p.value.y;
            instanceData->transform.row1.x = -o.value.row1.x * s.value.x;
            instanceData->transform.row1.z =  o.value.row2.x * s.value.y;
            instanceData->transform.row3.x =  o.value.row1.y * s.value.x;
            instanceData->transform.row3.z = -o.value.row2.y * s.value.y;
            instanceData->transform.row2.y = s.value.z;
            instanceData->isGameObject = 0;
            instanceData->bloomColor = { 0, 0, 0, 1 };
            ++meshBatch.instanceCount;
            ++instanceCount;
            ++instanceData;
        });
        if(meshBatch.meshID != ~(0u)) foregroundMeshBatchesVector.push_back(meshBatch);
        if(instanceCount) {
            GvkHelper::copy_buffer(device, commandPool, graphicsQueue,
                stagingBuffer, instanceVertexDataBuffers[swapchainBufferIndex],
                sizeof(MeshInstanceData) * instanceCount);
        }
     });

    present = _game->system<VulkanBackend>()
        .kind(flecs::OnStore)
        .each([&](flecs::entity e, VulkanBackend& s) {
            const Skybox* skybox = e.world().get<Skybox>();
            SubmitShadowMapDrawCommands(swapchainBufferIndex);
            SubmitGameObjectDrawCommands(swapchainBufferIndex, skybox);
            SubmitBloomBlurDrawCommands(swapchainBufferIndex);
            SubmitUIDrawCommands(swapchainBufferIndex);
            vkQueueWaitIdle(graphicsQueue);
            SubmitPresentCommandsAndWait(swapchainBufferIndex);
        });

    return false;
}

bool RenderSystem::ExitSystems()
{
    updateCameraPosition.destruct();
    copyRenderingData.destruct();
    foregroundSortedQuery.destruct();
    gameObjectSortedQuery.destruct();
    backgroundSortedQuery.destruct();
    floorSortedQuery.destruct();
    uiCanvasTextQuery.destruct();
    uiCanvasSpriteQuery.destruct();
    uiTextQuery.destruct();
    uiSpriteQuery.destruct();
    present.destruct();

    return true;
}

uint32_t RenderSystem::RegisterSkybox(const char* _cubeMapTexturePath)
{
    uint32_t outID = cubeMapDescriptorSets.size();
    /*=======================================================================*/
    /* DDS Texture File                                                      */
    /*=======================================================================*/
    VkImage cubeMapTexture;
    VkDeviceMemory cubeMapTextureMemory;
    VkImageView cubeMapTextureSRV;
    LoadDDSCubemap(_cubeMapTexturePath, physicalDevice, device, &cubeMapTexture, &cubeMapTextureMemory, &cubeMapTextureSRV);
    cubeMapTextures.push_back(cubeMapTexture);
    cubeMapTextureMemBlocks.push_back(cubeMapTextureMemory);
    cubeMapTextureSRVs.push_back(cubeMapTextureSRV);
    /*=======================================================================*/
    /* Allocate Descriptor Set                                               */
    /*=======================================================================*/
    VkDescriptorSet descriptorSet;
    VkDescriptorSetAllocateInfo alloc_info;
    ZeroMemory(&alloc_info, sizeof(VkDescriptorSetAllocateInfo));
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorSetCount= 1;
    alloc_info.descriptorPool = cubeMapDescriptorPool;
    alloc_info.pSetLayouts = &staticMeshDescriptorSetLayout;
    vkAllocateDescriptorSets(device, &alloc_info, &descriptorSet);
    cubeMapDescriptorSets.push_back(descriptorSet);
    /*=======================================================================*/
    /* Update Descriptor Set                                                 */
    /*=======================================================================*/
    VkDescriptorImageInfo imageInfo;
    ZeroMemory(&imageInfo, sizeof(VkDescriptorImageInfo));
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = cubeMapTextureSRV;
    VkWriteDescriptorSet descriptorWrites[1];
    ZeroMemory(descriptorWrites, sizeof(VkWriteDescriptorSet));
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = cubeMapDescriptorSets[outID];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorWrites[0].pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(device, 1, descriptorWrites, 0, NULL);
    return outID;
}

uint32_t RenderSystem::RegisterMesh(const char* _meshPath, const char* _baseTexturePath)
{
    uint32_t outID = meshVector.size();
    /*=======================================================================*/
    /* H2B Mesh File                                                         */
    /*=======================================================================*/
    Mesh mesh;
    MeshBounds bounds;
    ZeroMemory(&mesh, sizeof(Mesh));
    uint32_t vertexOffset=0, indexOffset = 0;
    if(meshVector.size() > 0)
    {
        vertexOffset= meshVector.back().vertexOffset + meshVector.back().vertexCount;
        indexOffset= meshVector.back().indexOffset + meshVector.back().indexCount;
    }
    mesh.vertexOffset= vertexOffset;
    mesh.indexOffset= indexOffset;
    LoadH2BMesh(_meshPath, mesh, bounds);
    meshVector.push_back(mesh);
    meshBoundsVector.push_back(bounds);
#ifdef DEV_BUILD
    UploadMeshBoundsDataToGPU(bounds, outID);
#endif
    /*=======================================================================*/
    /* DDS Texture File                                                      */
    /*=======================================================================*/
    VkImage texture;
    VkDeviceMemory textureMemory;
    VkImageView textureSRV;
    LoadDDSTexture(_baseTexturePath, physicalDevice, device, &texture, &textureMemory, &textureSRV);
    materialTextures.push_back(texture);
    materialTextureMemBlocks.push_back(textureMemory);
    materialTextureSRVs.push_back(textureSRV);
    /*=======================================================================*/
    /* Allocate Descriptor Set                                               */
    /*=======================================================================*/
    VkDescriptorSet descriptorSet;
    VkDescriptorSetAllocateInfo alloc_info;
    ZeroMemory(&alloc_info, sizeof(VkDescriptorSetAllocateInfo));
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorSetCount= 1;
    alloc_info.descriptorPool = materialDescriptorPool;
    alloc_info.pSetLayouts = &staticMeshDescriptorSetLayout;
    vkAllocateDescriptorSets(device, &alloc_info, &descriptorSet);
    materialDescriptorSets.push_back(descriptorSet);
    /*=======================================================================*/
    /* Update Descriptor Set                                                 */
    /*=======================================================================*/
    VkDescriptorImageInfo imageInfo;
    ZeroMemory(&imageInfo, sizeof(VkDescriptorImageInfo));
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = textureSRV;
    VkWriteDescriptorSet descriptorWrites[1];
    ZeroMemory(descriptorWrites, sizeof(VkWriteDescriptorSet));
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = materialDescriptorSets[outID];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorWrites[0].pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(device, 1, descriptorWrites, 0, NULL);
    return outID;
}

uint32_t RenderSystem::RegisterUIFont(const char* _fontLayoutPath, const char *_atlasTexturePath)
{
    uint32_t outID = fontLayouts.size();
    BMFont fontLayout;
    LoadBMFont(_fontLayoutPath, fontLayout);
    VkImage fontTexture;
    VkDeviceMemory fontTextureMemory;
    VkImageView fontTextureSRV;
    LoadTGATexture(_atlasTexturePath, physicalDevice, device, &fontTexture, &fontTextureMemory, &fontTextureSRV);
    fontLayouts.push_back(fontLayout);
    fontAtlasTextures.push_back(fontTexture);
    fontAtlasTextureMemBlocks.push_back(fontTextureMemory);
    fontAtlasTextureSRVs.push_back(fontTextureSRV);
    /*=======================================================================*/
    /* Allocate Descriptor Set                                               */
    /*=======================================================================*/
    VkDescriptorSet descriptorSet;
    VkDescriptorSetAllocateInfo alloc_info;
    ZeroMemory(&alloc_info, sizeof(VkDescriptorSetAllocateInfo));
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorSetCount= 1;
    alloc_info.descriptorPool = uiSDFDescriptorPool;
    alloc_info.pSetLayouts = &uiSDFDescriptorSetLayout;
    vkAllocateDescriptorSets(device, &alloc_info, &descriptorSet);
    uiSDFDescriptorSets.push_back(descriptorSet);
    /*=======================================================================*/
    /* Update Descriptor Set                                                 */
    /*=======================================================================*/
    VkDescriptorImageInfo imageInfo;
    ZeroMemory(&imageInfo, sizeof(VkDescriptorImageInfo));
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = fontTextureSRV;
    VkWriteDescriptorSet descriptorWrites[1];
    ZeroMemory(descriptorWrites, sizeof(VkWriteDescriptorSet));
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = uiSDFDescriptorSets[outID];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorWrites[0].pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(device, 1, descriptorWrites, 0, NULL);
    return outID;
}

uint32_t RenderSystem::RegisterUISpriteAtlas(const char *_atlasTexturePath)
{
    uint32_t outID = spriteAtlasTextures.size();
    VkImage spriteAtlasTexture;
    VkDeviceMemory spriteAtlasTextureMemory;
    VkImageView spriteAtlasTextureSRV;
    LoadTGATexture(_atlasTexturePath, physicalDevice, device, &spriteAtlasTexture, &spriteAtlasTextureMemory, &spriteAtlasTextureSRV);
    spriteAtlasTextures.push_back(spriteAtlasTexture);
    spriteAtlasTextureMemBlocks.push_back(spriteAtlasTextureMemory);
    spriteAtlasTextureSRVs.push_back(spriteAtlasTextureSRV);
    /*=======================================================================*/
    /* Allocate Descriptor Set                                               */
    /*=======================================================================*/
    VkDescriptorSet descriptorSet;
    VkDescriptorSetAllocateInfo alloc_info;
    ZeroMemory(&alloc_info, sizeof(VkDescriptorSetAllocateInfo));
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorSetCount= 1;
    alloc_info.descriptorPool = uiBlitDescriptorPool;
    alloc_info.pSetLayouts = &uiBlitDescriptorSetLayout;
    vkAllocateDescriptorSets(device, &alloc_info, &descriptorSet);
    uiBlitDescriptorSets.push_back(descriptorSet);
    /*=======================================================================*/
    /* Update Descriptor Set                                                 */
    /*=======================================================================*/
    VkDescriptorImageInfo imageInfo;
    ZeroMemory(&imageInfo, sizeof(VkDescriptorImageInfo));
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = spriteAtlasTextureSRV;
    VkWriteDescriptorSet descriptorWrites[1];
    ZeroMemory(descriptorWrites, sizeof(VkWriteDescriptorSet));
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = uiBlitDescriptorSets[outID];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorWrites[0].pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(device, 1, descriptorWrites, 0, NULL);
    return outID;
}
#ifdef DEV_BUILD
void RenderSystem::DebugToggleMeshBoundsDraw() { debugDrawMeshBounds = !debugDrawMeshBounds; }
void RenderSystem::DebugToggleOrthographicProjection() { debugDrawOrthographic = !debugDrawOrthographic; }
#endif
const std::vector<RenderSystem::MeshBounds>& RenderSystem::GetMeshBoundsVector()
{
    return meshBoundsVector;
}

void RenderSystem::SubmitShadowMapDrawCommands(uint32_t bufferIndex)
{
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    GvkHelper::signal_command_start(device, commandPool, &cmd);
    VkClearValue clearValues[1];
    clearValues[0].depthStencil = {1.0f, 0u};
    VkRenderPassBeginInfo begin_info;
    ZeroMemory(&begin_info, sizeof(VkRenderPassBeginInfo));
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.renderPass = shadowMapRenderPass;
    begin_info.framebuffer = shadowMapFramebuffers[bufferIndex];
    begin_info.renderArea = { 0, 0, 1024, 1024 };
    begin_info.clearValueCount = 1;
    begin_info.pClearValues = clearValues;
    vkCmdBeginRenderPass(cmd, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
    VkDeviceSize vertexBufferOffsets[2] = { 0, 0 };
    VkBuffer vertexBuffers[2] = { meshVertexDataBuffer, instanceVertexDataBuffers[bufferIndex] };
    vkCmdBindVertexBuffers(cmd, 0, 2, vertexBuffers, vertexBufferOffsets);
    vkCmdBindIndexBuffer(cmd, meshIndexDataBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowMapPipeline);
    GMATRIXF proj, lightMatrix;
    GVECTORF lightPosition, lookAtPosition = { 0, 0, gameCameraYPosition, 1 };
    GVector::ScaleF(globalDirectionalLightDir, -50, lightPosition);
    GVector::AddVectorF(lightPosition, lookAtPosition, lightPosition);
    GMatrix::LookAtLHF(lightPosition, lookAtPosition, GVECTORF{0.0, 1.0, 0.0, 0.0}, lightMatrix);
    GMatrix::IdentityF(proj);
    proj.row2.data[1] = -1.f / 100;
    proj.row1.data[0] = -proj.row2.data[1];
    proj.row3.data[2] = 1.f / (200.f - 0.1f);
    proj.row4.data[2] = -0.1f / (200.f - 0.1f);
    GMatrix::MultiplyMatrixF(lightMatrix, proj, lightMatrix);
    GMatrix::TransposeF(lightMatrix, lightMatrix);
    vkCmdPushConstants(cmd, shadowMapPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
        0, sizeof(GMATRIXF), &lightMatrix);
    VkViewport viewport = { 0, 0, 1024, 1024, 0, 1 };
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor = { 0, 0, 1024, 1024 };
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    for(uint32_t i = 0; i < backgroundMeshBatchesVector.size(); ++i)
    {
        const auto& meshBatch = backgroundMeshBatchesVector[i];
        const auto& mesh = meshVector[meshBatch.meshID];
        if(!meshBatch.instanceCount) continue;
        vkCmdDrawIndexed(cmd,
            mesh.indexCount, meshBatch.instanceCount,
            mesh.indexOffset, mesh.vertexOffset, meshBatch.instanceOffset);
    }
    for(uint32_t i = 0; i < gameobjectMeshBatchesVector.size(); ++i)
    {
        const auto& meshBatch = gameobjectMeshBatchesVector[i];
        const auto& mesh = meshVector[meshBatch.meshID];
        if(!meshBatch.instanceCount) continue;
        vkCmdDrawIndexed(cmd,
            mesh.indexCount, meshBatch.instanceCount,
            mesh.indexOffset, mesh.vertexOffset, meshBatch.instanceOffset);
    }
    for(uint32_t i = 0; i < foregroundMeshBatchesVector.size(); ++i)
    {
        const auto& meshBatch = foregroundMeshBatchesVector[i];
        const auto& mesh = meshVector[meshBatch.meshID];
        if(!meshBatch.instanceCount) continue;
        vkCmdDrawIndexed(cmd,
            mesh.indexCount, meshBatch.instanceCount,
            mesh.indexOffset, mesh.vertexOffset, meshBatch.instanceOffset);
    }
    vkCmdEndRenderPass(cmd);
    GvkHelper::signal_command_end(device, graphicsQueue, commandPool, &cmd);
}

void RenderSystem::SubmitGameObjectDrawCommands(uint32_t bufferIndex, const Skybox* skybox)
{
    float skyBoxScale = 1;
    /*=======================================================================*/
    /* Calculate Rendering Matrices                                          */
    /*=======================================================================*/
    /* Main Camera View Projection Matrix                                    */
    /*-----------------------------------------------------------------------*/
    GMATRIXF viewProjectionMatrix;
    GMATRIXF view, proj, lightMatrix;
    GMatrix::LookAtLHF(GVECTORF{ -gameCameraDistanceToGameplayPlane, 0, gameCameraYPosition, 1 },
                       GVECTORF{ 0, 0, gameCameraYPosition, 1 },
                       GVECTORF{0.0, 1.0, 0.0, 0.0}, view);
    float aspectRatio;
    vulkan.GetAspectRatio(aspectRatio);
#if DEV_BUILD
    if(debugDrawOrthographic)
    {
        skyBoxScale = gameCameraDistanceToGameplayPlane * aspectRatio;
        GMatrix::IdentityF(proj);
        proj.row2.data[1] = -1.f / gameCameraDistanceToGameplayPlane;
        proj.row1.data[0] = -proj.row2.data[1] / aspectRatio;
        proj.row3.data[2] = 1.f / (100.f - 0.1f);
        proj.row4.data[2] = -0.1f / (100.f - 0.1f);
    } else {
        GMatrix::ProjectionVulkanLHF(G_DEGREE_TO_RADIAN_F(90), aspectRatio, 0.1F,
                                     100.0F, proj);
    }
#else
    GMatrix::ProjectionVulkanLHF(G_DEGREE_TO_RADIAN_F(90), aspectRatio, 0.1F,
                                 100.0F, proj);
#endif
    GMatrix::MultiplyMatrixF(view, proj, viewProjectionMatrix);
    GMatrix::TransposeF(viewProjectionMatrix, viewProjectionMatrix);
    /*-----------------------------------------------------------------------*/
    /* Skybox Projection Matrix                                              */
    /*-----------------------------------------------------------------------*/
    GMATRIXF skyBoxMatrix = GIdentityMatrixF;
    skyBoxMatrix.row1.x =  skybox->value.x * skyBoxScale;
    skyBoxMatrix.row1.z = -skybox->value.y * skyBoxScale;
    skyBoxMatrix.row2.y = -1 * skyBoxScale;
    skyBoxMatrix.row3.x =  skybox->value.y * skyBoxScale;
    skyBoxMatrix.row3.z =  skybox->value.x * skyBoxScale;
    GMatrix::MultiplyMatrixF(skyBoxMatrix, proj, skyBoxMatrix);
    GMatrix::TransposeF(skyBoxMatrix, skyBoxMatrix);
    /*-----------------------------------------------------------------------*/
    /* Global Directional Light Matrix                                       */
    /*-----------------------------------------------------------------------*/
    GVECTORF lightPosition, lookAtPosition = { 0, 0, gameCameraYPosition, 1 };
    GVector::ScaleF(globalDirectionalLightDir, -50, lightPosition);
    GVector::AddVectorF(lightPosition, lookAtPosition, lightPosition);
    GMatrix::LookAtLHF(lightPosition, lookAtPosition, GVECTORF{0.0, 1.0, 0.0, 0.0}, lightMatrix);
    GMatrix::IdentityF(proj);
    proj.row2.data[1] = -1.f / 100;
    proj.row1.data[0] = -proj.row2.data[1];
    proj.row3.data[2] = 1.f / (200.f - 0.1f);
    proj.row4.data[2] = -0.1f / (200.f - 0.1f);
    GMatrix::MultiplyMatrixF(lightMatrix, proj, lightMatrix);
    GMatrix::TransposeF(lightMatrix, lightMatrix);
    /*=======================================================================*/
    /* Record Rendering Commands                                             */
    /*=======================================================================*/
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    GvkHelper::signal_command_start(device, commandPool, &cmd);
    VkClearValue clearValues[3];
    clearValues[0].color = {{0.39F, 0.58F, 0.93f, 1}};
    clearValues[1].color = {{ 0.F, 0.F, 0.f, 0}};
    clearValues[2].depthStencil = {1.0f, 0u};
    VkRenderPassBeginInfo begin_info;
    ZeroMemory(&begin_info, sizeof(VkRenderPassBeginInfo));
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.renderPass = staticMeshRenderPass;
    begin_info.framebuffer = gameObjectFramebuffers[bufferIndex];
    begin_info.renderArea = { 0, 0, swapchainExtent.width, swapchainExtent.height };
    begin_info.clearValueCount = 3;
    begin_info.pClearValues = clearValues;
    vkCmdBeginRenderPass(cmd, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
    /*-----------------------------------------------------------------------*/
    /* Main Static Mesh Rendering                                            */
    /*-----------------------------------------------------------------------*/
    VkDeviceSize vertexBufferOffsets[2] = { 0, 0 };
    VkBuffer vertexBuffers[2] = { meshVertexDataBuffer, instanceVertexDataBuffers[bufferIndex] };
    vkCmdBindVertexBuffers(cmd, 0, 2, vertexBuffers, vertexBufferOffsets);
    vkCmdBindIndexBuffer(cmd, meshIndexDataBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, staticMeshPipeline);
    vkCmdPushConstants(cmd, staticMeshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
        0, sizeof(GMATRIXF), &viewProjectionMatrix);
    vkCmdPushConstants(cmd, staticMeshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
        sizeof(GMATRIXF), sizeof(GMATRIXF), &lightMatrix);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, staticMeshPipelineLayout,
        1, 1, &shadowMapDescriptorSets[bufferIndex],
        0, nullptr);
    VkViewport viewport = {0,
                           (float)swapchainExtent.height,
                           (float)swapchainExtent.width,
                           -1.F * swapchainExtent.height,
                           0,
                           1};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor = { 0, 0, swapchainExtent.width, swapchainExtent.height };
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    for(uint32_t i = 0; i < backgroundMeshBatchesVector.size(); ++i)
    {
        const auto& meshBatch = backgroundMeshBatchesVector[i];
        const auto& mesh = meshVector[meshBatch.meshID];
        if(!meshBatch.instanceCount) continue;
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, staticMeshPipelineLayout,
            0, 1, &materialDescriptorSets[meshBatch.meshID],
            0, nullptr);
        vkCmdDrawIndexed(cmd,
            mesh.indexCount, meshBatch.instanceCount,
            mesh.indexOffset, mesh.vertexOffset, meshBatch.instanceOffset);
    }
    for(uint32_t i = 0; i < gameobjectMeshBatchesVector.size(); ++i)
    {
        const auto& meshBatch = gameobjectMeshBatchesVector[i];
        const auto& mesh = meshVector[meshBatch.meshID];
        if(!meshBatch.instanceCount) continue;
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, staticMeshPipelineLayout,
            0, 1, &materialDescriptorSets[meshBatch.meshID],
            0, nullptr);
        vkCmdDrawIndexed(cmd,
            mesh.indexCount, meshBatch.instanceCount,
            mesh.indexOffset, mesh.vertexOffset, meshBatch.instanceOffset);
    }
    for(uint32_t i = 0; i < foregroundMeshBatchesVector.size(); ++i)
    {
        const auto& meshBatch = foregroundMeshBatchesVector[i];
        const auto& mesh = meshVector[meshBatch.meshID];
        if(!meshBatch.instanceCount) continue;
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, staticMeshPipelineLayout,
            0, 1, &materialDescriptorSets[meshBatch.meshID],
            0, nullptr);
        vkCmdDrawIndexed(cmd,
            mesh.indexCount, meshBatch.instanceCount,
            mesh.indexOffset, mesh.vertexOffset, meshBatch.instanceOffset);
    }
#ifdef DEV_BUILD
    if(debugDrawMeshBounds)
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, debugColliderPipeline);
        vkCmdBindVertexBuffers(cmd, 0, 1, &debugColliderVertexDataBuffer, vertexBufferOffsets);
        vkCmdBindIndexBuffer(cmd, debugColliderIndexDataBuffer, 0, VK_INDEX_TYPE_UINT32);
        for(uint32_t i = 0; i < gameobjectMeshBatchesVector.size(); ++i)
        {
            const auto& meshBatch = gameobjectMeshBatchesVector[i];
            const auto& mesh = meshVector[meshBatch.meshID];
            if(!meshBatch.instanceCount) continue;
            vkCmdDrawIndexed(cmd, 8, meshBatch.instanceCount,
                8 * meshBatch.meshID, 4 * meshBatch.meshID, meshBatch.instanceOffset);
        }
    }
#endif
    /*-----------------------------------------------------------------------*/
    /* Skybox Rendering                                                      */
    /*-----------------------------------------------------------------------*/
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skyBoxPipeline);
    vkCmdPushConstants(cmd, skyBoxPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
        0, sizeof(GMATRIXF), &skyBoxMatrix);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skyBoxPipelineLayout,
        0, 1, &cubeMapDescriptorSets[skybox->skyBoxTextureID],
        0, nullptr);
    vkCmdDraw(cmd, 36, 1, 0, 0);
    vkCmdEndRenderPass(cmd);
    GvkHelper::signal_command_end(device, graphicsQueue, commandPool, &cmd);
}

void RenderSystem::SubmitBloomBlurDrawCommands(uint32_t bufferIndex)
{
    VkViewport viewport = {
        0, 0,
        (float)swapchainExtent.width, (float)swapchainExtent.height,
        0, 1 };
    VkRect2D scissor = {
        0, 0,
        (uint32_t)swapchainExtent.width, (uint32_t)swapchainExtent.height
    };
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    GvkHelper::signal_command_start(device, commandPool, &cmd);

    VkRenderPassBeginInfo begin_info;
    ZeroMemory(&begin_info, sizeof(VkRenderPassBeginInfo));
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.renderPass = blurRenderPass;
    begin_info.renderArea = { 0, 0, swapchainExtent.width, swapchainExtent.height };
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, blurPipeline);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    for(int i = 0; i < 5; ++i)
    {
        uint32_t blurDirection = 0;
        begin_info.framebuffer = blurPingFramebuffers[bufferIndex];
        vkCmdBeginRenderPass(cmd, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdPushConstants(cmd, blurPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(uint32_t), &blurDirection);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, blurPipelineLayout,
            0, 1, &blurPingDescriptorSets[bufferIndex],
            0, nullptr);
        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRenderPass(cmd);
        blurDirection = 1;
        begin_info.framebuffer = blurPongFramebuffers[bufferIndex];
        vkCmdBeginRenderPass(cmd, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdPushConstants(cmd, blurPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(uint32_t), &blurDirection);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, blurPipelineLayout,
            0, 1, &blurPongDescriptorSets[bufferIndex],
            0, nullptr);
        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRenderPass(cmd);
    }
    for(uint32_t i = 0; i < spriteBatches.size(); ++i)
    {
        const auto& spriteBatch = spriteBatches[i];


    }
    GvkHelper::signal_command_end(device, graphicsQueue, commandPool, &cmd);
}

void RenderSystem::SubmitUIDrawCommands(uint32_t bufferIndex)
{
    VkViewport viewport = {
        0, (float)swapchainExtent.height,
        (float)swapchainExtent.width, -1.F * swapchainExtent.height,
        0, 1 };
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    GvkHelper::signal_command_start(device, commandPool, &cmd);
    VkClearValue clearValues[1];
    clearValues[0].color = {{0.F, 0.F, 0.F, 0.F}};
    VkRenderPassBeginInfo begin_info;
    ZeroMemory(&begin_info, sizeof(VkRenderPassBeginInfo));
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.renderPass = uiRenderPass;
    begin_info.framebuffer = uiFramebuffers[bufferIndex];
    begin_info.renderArea = { 0, 0, swapchainExtent.width, swapchainExtent.height };
    begin_info.clearValueCount = 1;
    begin_info.pClearValues = clearValues;
    vkCmdBeginRenderPass(cmd, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
    VkDeviceSize vertexBufferOffset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &uiSDFVertexDataBuffers[bufferIndex], &vertexBufferOffset);
    vkCmdBindIndexBuffer(cmd, uiSDFIndexDataBuffers[bufferIndex], 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, uiSDFPipeline);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    uint32_t currentlyBoundFontID = ~(0u);
    for(uint32_t i = 0; i < fontBatches.size(); ++i)
    {
        const auto& fontBatch = fontBatches[i];
        if(!fontBatch.indexCount) continue;
        VkRect2D scissor = {
            (int32_t)((fontBatch.rect.x + 1) * 0.5F * swapchainExtent.width),
            (int32_t)(((1 - fontBatch.rect.y) * 0.5F * swapchainExtent.height) - fontBatch.rect.height),
            (uint32_t)fontBatch.rect.width,
            (uint32_t)fontBatch.rect.height
        };
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        if(currentlyBoundFontID != fontBatch.fontID)
        {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, uiSDFPipelineLayout,
                0, 1, &uiSDFDescriptorSets[fontBatch.fontID],
                0, nullptr);
            currentlyBoundFontID = fontBatch.fontID;
        }
        vkCmdPushConstants(cmd, uiSDFPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
            sizeof(GVECTORF) * 0, sizeof(GVECTORF), &fontBatch.fontColor);
        vkCmdPushConstants(cmd, uiSDFPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
            sizeof(GVECTORF) * 1, sizeof(GVECTORF), &fontBatch.outlineColor);
        vkCmdPushConstants(cmd, uiSDFPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
            sizeof(GVECTORF) * 2, sizeof(float), &fontBatch.outlineWidth);
        vkCmdDrawIndexed(cmd, fontBatch.indexCount, 1, fontBatch.indexOffset, 0, 0);
    }
    vkCmdBindVertexBuffers(cmd, 0, 1, &uiBlitInstanceDataBuffers[bufferIndex], &vertexBufferOffset);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, uiBlitPipeline);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor = {
        0, 0,
        (uint32_t)swapchainExtent.width, (uint32_t)swapchainExtent.height
    };
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    for(uint32_t i = 0; i < spriteBatches.size(); ++i)
    {
        const auto& spriteBatch = spriteBatches[i];
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, uiBlitPipelineLayout,
            0, 1, &uiBlitDescriptorSets[spriteBatch.spriteAtlasID],
            0, nullptr);
        vkCmdDraw(cmd, 6, spriteBatch.instanceCount, 0, spriteBatch.instanceOffset);
    }
    vkCmdEndRenderPass(cmd);
    GvkHelper::signal_command_end(device, graphicsQueue, commandPool, &cmd);
}

void RenderSystem::SubmitPresentCommandsAndWait(uint32_t bufferIndex)
{
    VkCommandBuffer commandBuffer;
    vulkan.GetCommandBuffer(bufferIndex, (void**)&commandBuffer);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, presentPipeline);
    VkViewport viewport = {0,
                           (float)swapchainExtent.height,
                           (float)swapchainExtent.width,
                           -1.F * swapchainExtent.height,
                           0,
                           1};
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    VkRect2D scissor = { 0, 0, swapchainExtent.width, swapchainExtent.height };
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, presentPipelineLayout,
        0, 1, &perFrameDescriptorSets[bufferIndex],
        0, nullptr);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}

void RenderSystem::CreateShaderModules(VkDevice _device, std::shared_ptr<const GameConfig> _readCfg)
{
    /*=======================================================================*/
    /* Read GameConfig.ini to get Shader File Paths                          */
    /*=======================================================================*/
    std::string vertexShaderSource, pixelShaderSource;
    /*-----------------------------------------------------------------------*/
    /* Shadow Map Shaders                                                    */
    /*-----------------------------------------------------------------------*/
    vertexShaderSource = (*_readCfg).at("RenderSystem").at("ShadowVS").as<std::string>();
    pixelShaderSource = (*_readCfg).at("RenderSystem").at("ShadowPS").as<std::string>();
    ShaderFileData shadowMapShaders;
    LoadShaderFileData(vertexShaderSource.c_str(), pixelShaderSource.c_str(), fileInterface,
                        shadowMapShaders);
    /*-----------------------------------------------------------------------*/
    /* Static Mesh Shaders                                                   */
    /*-----------------------------------------------------------------------*/
    vertexShaderSource = (*_readCfg).at("RenderSystem").at("StaticMeshVS").as<std::string>();
    pixelShaderSource = (*_readCfg).at("RenderSystem").at("StaticMeshPS").as<std::string>();
    ShaderFileData staticMeshShaders;
    LoadShaderFileData(vertexShaderSource.c_str(), pixelShaderSource.c_str(), fileInterface,
                        staticMeshShaders);
    /*-----------------------------------------------------------------------*/
    /* Blur Shaders                                                          */
    /*-----------------------------------------------------------------------*/
    vertexShaderSource = (*_readCfg).at("RenderSystem").at("BlurVS").as<std::string>();
    pixelShaderSource = (*_readCfg).at("RenderSystem").at("BlurPS").as<std::string>();
    ShaderFileData blurShaders;
    LoadShaderFileData(vertexShaderSource.c_str(), pixelShaderSource.c_str(), fileInterface,
                        blurShaders);
    /*-----------------------------------------------------------------------*/
    /* Skybox Shaders                                                        */
    /*-----------------------------------------------------------------------*/
    vertexShaderSource = (*_readCfg).at("RenderSystem").at("SkyboxVS").as<std::string>();
    pixelShaderSource = (*_readCfg).at("RenderSystem").at("SkyboxPS").as<std::string>();
    ShaderFileData skyboxShaders;
    LoadShaderFileData(vertexShaderSource.c_str(), pixelShaderSource.c_str(), fileInterface,
                        skyboxShaders);
#ifdef DEV_BUILD
    /*-----------------------------------------------------------------------*/
    /* Debug Collider Shaders                                                */
    /*-----------------------------------------------------------------------*/
    vertexShaderSource = (*_readCfg).at("RenderSystem").at("DebugColliderVS").as<std::string>();
    pixelShaderSource = (*_readCfg).at("RenderSystem").at("DebugColliderPS").as<std::string>();
    ShaderFileData debugColliderShaders;
    LoadShaderFileData(vertexShaderSource.c_str(), pixelShaderSource.c_str(), fileInterface,
                        debugColliderShaders);
#endif
    /*-----------------------------------------------------------------------*/
    /* UI SDF Shaders                                                        */
    /*-----------------------------------------------------------------------*/
    vertexShaderSource = (*_readCfg).at("RenderSystem").at("SDFVS").as<std::string>();
    pixelShaderSource = (*_readCfg).at("RenderSystem").at("SDFPS").as<std::string>();
    ShaderFileData uiSDFShaders;
    LoadShaderFileData(vertexShaderSource.c_str(), pixelShaderSource.c_str(), fileInterface,
                        uiSDFShaders);
    /*-----------------------------------------------------------------------*/
    /* UI Blit Shaders                                                       */
    /*-----------------------------------------------------------------------*/
    vertexShaderSource = (*_readCfg).at("RenderSystem").at("BlitVS").as<std::string>();
    pixelShaderSource = (*_readCfg).at("RenderSystem").at("BlitPS").as<std::string>();
    ShaderFileData uiBlitShaders;
    LoadShaderFileData(vertexShaderSource.c_str(), pixelShaderSource.c_str(), fileInterface,
                        uiBlitShaders);
    /*-----------------------------------------------------------------------*/
    /* Present Blit Shaders                                                  */
    /*-----------------------------------------------------------------------*/
    vertexShaderSource = (*_readCfg).at("RenderSystem").at("PresentVS").as<std::string>();
    pixelShaderSource = (*_readCfg).at("RenderSystem").at("PresentPS").as<std::string>();
    ShaderFileData presentShaders;
    LoadShaderFileData(vertexShaderSource.c_str(), pixelShaderSource.c_str(), fileInterface,
                        presentShaders);
    /*-----------------------------------------------------------------------*/
    /*=======================================================================*/
    /* Create Shader Modules                                                 */
    /*=======================================================================*/
    VkShaderModuleCreateInfo create_info;
    ZeroMemory(&create_info, sizeof(VkShaderModuleCreateInfo));
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    /*-----------------------------------------------------------------------*/
    /* Shadow Map - Vertex Shader                                            */
    /*-----------------------------------------------------------------------*/
    create_info.codeSize = shadowMapShaders.vertexShaderFileSize;
    create_info.pCode = shadowMapShaders.vertexShaderFileData;
    vkCreateShaderModule(_device, &create_info, NULL, &shadowMapVertexShader);
    /*-----------------------------------------------------------------------*/
    /* Shadow Map - Pixel Shader                                             */
    /*-----------------------------------------------------------------------*/
    create_info.codeSize = shadowMapShaders.pixelShaderFileSize;
    create_info.pCode = shadowMapShaders.pixelShaderFileData;
    vkCreateShaderModule(_device, &create_info, NULL, &shadowMapPixelShader);
    /*-----------------------------------------------------------------------*/
    /* Static Mesh - Vertex Shader                                           */
    /*-----------------------------------------------------------------------*/
    create_info.codeSize = staticMeshShaders.vertexShaderFileSize;
    create_info.pCode = staticMeshShaders.vertexShaderFileData;
    vkCreateShaderModule(_device, &create_info, NULL, &staticMeshVertexShader);
    /*-----------------------------------------------------------------------*/
    /* Static Mesh - Pixel Shader                                            */
    /*-----------------------------------------------------------------------*/
    create_info.codeSize = staticMeshShaders.pixelShaderFileSize;
    create_info.pCode = staticMeshShaders.pixelShaderFileData;
    vkCreateShaderModule(_device, &create_info, NULL, &staticMeshPixelShader);
    /*-----------------------------------------------------------------------*/
    /* Blur - Vertex Shader                                                  */
    /*-----------------------------------------------------------------------*/
    create_info.codeSize = blurShaders.vertexShaderFileSize;
    create_info.pCode = blurShaders.vertexShaderFileData;
    vkCreateShaderModule(_device, &create_info, NULL, &blurVertexShader);
    /*-----------------------------------------------------------------------*/
    /* Blur - Pixel Shader                                                   */
    /*-----------------------------------------------------------------------*/
    create_info.codeSize = blurShaders.pixelShaderFileSize;
    create_info.pCode = blurShaders.pixelShaderFileData;
    vkCreateShaderModule(_device, &create_info, NULL, &blurPixelShader);
    /*-----------------------------------------------------------------------*/
    /* Skybox - Vertex Shader                                           */
    /*-----------------------------------------------------------------------*/
    create_info.codeSize = skyboxShaders.vertexShaderFileSize;
    create_info.pCode = skyboxShaders.vertexShaderFileData;
    vkCreateShaderModule(_device, &create_info, NULL, &skyBoxVertexShader);
    /*-----------------------------------------------------------------------*/
    /* Skybox - Pixel Shader                                            */
    /*-----------------------------------------------------------------------*/
    create_info.codeSize = skyboxShaders.pixelShaderFileSize;
    create_info.pCode = skyboxShaders.pixelShaderFileData;
    vkCreateShaderModule(_device, &create_info, NULL, &skyBoxPixelShader);
#ifdef DEV_BUILD
    /*-----------------------------------------------------------------------*/
    /* Debug Collider - Vertex Shader                                        */
    /*-----------------------------------------------------------------------*/
    create_info.codeSize = debugColliderShaders.vertexShaderFileSize;
    create_info.pCode = debugColliderShaders.vertexShaderFileData;
    vkCreateShaderModule(_device, &create_info, NULL, &debugColliderVertexShader);
    /*-----------------------------------------------------------------------*/
    /* Debug Collider - Pixel Shader                                         */
    /*-----------------------------------------------------------------------*/
    create_info.codeSize = debugColliderShaders.pixelShaderFileSize;
    create_info.pCode = debugColliderShaders.pixelShaderFileData;
    vkCreateShaderModule(_device, &create_info, NULL, &debugColliderPixelShader);
#endif
    /*-----------------------------------------------------------------------*/
    /* UI SDF - Vertex Shader                                                */
    /*-----------------------------------------------------------------------*/
    create_info.codeSize = uiSDFShaders.vertexShaderFileSize;
    create_info.pCode = uiSDFShaders.vertexShaderFileData;
    vkCreateShaderModule(_device, &create_info, NULL, &uiSDFVertexShader);
    /*-----------------------------------------------------------------------*/
    /* UI SDF - Pixel Shader                                                 */
    /*-----------------------------------------------------------------------*/
    create_info.codeSize = uiSDFShaders.pixelShaderFileSize;
    create_info.pCode = uiSDFShaders.pixelShaderFileData;
    vkCreateShaderModule(_device, &create_info, NULL, &uiSDFPixelShader);
    /*-----------------------------------------------------------------------*/
    /* UI Blit - Vertex Shader                                               */
    /*-----------------------------------------------------------------------*/
    create_info.codeSize = uiBlitShaders.vertexShaderFileSize;
    create_info.pCode = uiBlitShaders.vertexShaderFileData;
    vkCreateShaderModule(_device, &create_info, NULL, &uiBlitVertexShader);
    /*-----------------------------------------------------------------------*/
    /* UI Blit - Pixel Shader                                                */
    /*-----------------------------------------------------------------------*/
    create_info.codeSize = uiBlitShaders.pixelShaderFileSize;
    create_info.pCode = uiBlitShaders.pixelShaderFileData;
    vkCreateShaderModule(_device, &create_info, NULL, &uiBlitPixelShader);
    /*-----------------------------------------------------------------------*/
    /* Present - Vertex Shader                                               */
    /*-----------------------------------------------------------------------*/
    create_info.codeSize = presentShaders.vertexShaderFileSize;
    create_info.pCode = presentShaders.vertexShaderFileData;
    vkCreateShaderModule(_device, &create_info, NULL, &presentVertexShader);
    /*-----------------------------------------------------------------------*/
    /* Present - Pixel Shader                                                */
    /*-----------------------------------------------------------------------*/
    create_info.codeSize = presentShaders.pixelShaderFileSize;
    create_info.pCode = presentShaders.pixelShaderFileData;
    vkCreateShaderModule(_device, &create_info, NULL, &presentPixelShader);
    /*-----------------------------------------------------------------------*/
    FreeShaderFileData(shadowMapShaders);
    FreeShaderFileData(staticMeshShaders);
    FreeShaderFileData(blurShaders);
    FreeShaderFileData(skyboxShaders);
    FreeShaderFileData(uiSDFShaders);
    FreeShaderFileData(uiBlitShaders);
    FreeShaderFileData(presentShaders);
}

void RenderSystem::DestroyShaderModules(VkDevice _device)
{
    vkDestroyShaderModule(_device, skyBoxVertexShader, NULL);
    vkDestroyShaderModule(_device, skyBoxPixelShader, NULL);
    vkDestroyShaderModule(_device, shadowMapVertexShader, NULL);
    vkDestroyShaderModule(_device, shadowMapPixelShader, NULL);
    vkDestroyShaderModule(_device, staticMeshVertexShader, NULL);
    vkDestroyShaderModule(_device, staticMeshPixelShader, NULL);
    vkDestroyShaderModule(_device, blurVertexShader, NULL);
    vkDestroyShaderModule(_device, blurPixelShader, NULL);
#ifdef DEV_BUILD
    vkDestroyShaderModule(_device, debugColliderVertexShader, NULL);
    vkDestroyShaderModule(_device, debugColliderPixelShader, NULL);
#endif
    vkDestroyShaderModule(_device, uiSDFVertexShader, NULL);
    vkDestroyShaderModule(_device, uiSDFPixelShader, NULL);
    vkDestroyShaderModule(_device, uiBlitVertexShader, NULL);
    vkDestroyShaderModule(_device, uiBlitPixelShader, NULL);
    vkDestroyShaderModule(_device, presentVertexShader, NULL);
    vkDestroyShaderModule(_device, presentPixelShader, NULL);
}

void RenderSystem::CreateShadowMapRenderPass(VkDevice _device)
{
    VkRenderPassCreateInfo create_info;
    VkAttachmentDescription renderPassAttachments[1];
    ZeroMemory(renderPassAttachments, sizeof(VkAttachmentDescription));
    /*-----------------------------------------------------------------------*/
    /* Depth Attachment                                                      */
    /*-----------------------------------------------------------------------*/
    renderPassAttachments[0].format         = VK_FORMAT_D32_SFLOAT;
    renderPassAttachments[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    renderPassAttachments[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    renderPassAttachments[0].samples        = VK_SAMPLE_COUNT_1_BIT;
    renderPassAttachments[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    renderPassAttachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    renderPassAttachments[0].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    renderPassAttachments[0].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    VkAttachmentReference renderPassDepthAttachment;
    ZeroMemory(&renderPassDepthAttachment, sizeof(VkAttachmentReference));
    renderPassDepthAttachment.attachment    = 0;
    renderPassDepthAttachment.layout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    /*-----------------------------------------------------------------------*/
    /* Subpasses                                                             */
    /*-----------------------------------------------------------------------*/
    VkSubpassDescription renderPassSubpasses[1];
    ZeroMemory(renderPassSubpasses, sizeof(VkSubpassDescription));
    renderPassSubpasses[0].pDepthStencilAttachment = &renderPassDepthAttachment;
    ZeroMemory(&create_info, sizeof(VkRenderPassCreateInfo));
    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    create_info.attachmentCount = 1;
    create_info.pAttachments    = renderPassAttachments;
    create_info.subpassCount    = 1;
    create_info.pSubpasses      = renderPassSubpasses;
    vkCreateRenderPass(_device, &create_info, NULL, &shadowMapRenderPass);
}

void RenderSystem::CreateStaticMeshRenderPass(VkDevice _device)
{
    VkRenderPassCreateInfo create_info;
    const uint32_t renderPassAttachmentCount = 3;
    VkAttachmentDescription renderPassAttachments[renderPassAttachmentCount];
    ZeroMemory(renderPassAttachments, renderPassAttachmentCount * sizeof(VkAttachmentDescription));
    VkAttachmentReference renderPassColorAttachments[2];
    ZeroMemory(&renderPassColorAttachments, 2 * sizeof(VkAttachmentReference));
    /*-----------------------------------------------------------------------*/
    /* Color Attachment                                                      */
    /*-----------------------------------------------------------------------*/
    renderPassAttachments[0].format = swapchainFormat.format;
    renderPassAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    renderPassAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    renderPassAttachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    renderPassAttachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    renderPassAttachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    renderPassAttachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    renderPassAttachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    renderPassColorAttachments[0].attachment = 0;
    renderPassColorAttachments[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    /*-----------------------------------------------------------------------*/
    /* Bloom Color Attachment                                                */
    /*-----------------------------------------------------------------------*/
    renderPassAttachments[1].format = swapchainFormat.format;
    renderPassAttachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    renderPassAttachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    renderPassAttachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    renderPassAttachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    renderPassAttachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    renderPassAttachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    renderPassAttachments[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    renderPassColorAttachments[1].attachment = 1;
    renderPassColorAttachments[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    /*-----------------------------------------------------------------------*/
    /* Depth Attachment                                                      */
    /*-----------------------------------------------------------------------*/
    renderPassAttachments[2].format = VK_FORMAT_D16_UNORM;
    renderPassAttachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    renderPassAttachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    renderPassAttachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
    renderPassAttachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    renderPassAttachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    renderPassAttachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    renderPassAttachments[2].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkAttachmentReference renderPassDepthAttachment;
    ZeroMemory(&renderPassDepthAttachment, sizeof(VkAttachmentReference));
    renderPassDepthAttachment.attachment = 2;
    renderPassDepthAttachment.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    /*-----------------------------------------------------------------------*/
    /* Subpasses                                                             */
    /*-----------------------------------------------------------------------*/
    VkSubpassDescription renderPassSubpasses[1];
    ZeroMemory(&renderPassSubpasses[0], sizeof(VkSubpassDescription));
    renderPassSubpasses[0].colorAttachmentCount = 2;
    renderPassSubpasses[0].pColorAttachments = renderPassColorAttachments;
    renderPassSubpasses[0].pDepthStencilAttachment = &renderPassDepthAttachment;
    ZeroMemory(&create_info, sizeof(VkRenderPassCreateInfo));
    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    create_info.attachmentCount = renderPassAttachmentCount;
    create_info.pAttachments = renderPassAttachments;
    create_info.subpassCount = 1;
    create_info.pSubpasses = renderPassSubpasses;
    vkCreateRenderPass(_device, &create_info, NULL, &staticMeshRenderPass);
}

void RenderSystem::CreateBlurRenderPass(VkDevice _device)
{
    VkRenderPassCreateInfo create_info;
    const uint32_t renderPassAttachmentCount = 1;
    VkAttachmentDescription renderPassAttachments[renderPassAttachmentCount];
    ZeroMemory(renderPassAttachments, renderPassAttachmentCount * sizeof(VkAttachmentDescription));
    VkAttachmentReference renderPassColorAttachments[1];
    ZeroMemory(&renderPassColorAttachments, 1 * sizeof(VkAttachmentReference));
    /*-----------------------------------------------------------------------*/
    /* Color Attachment                                                      */
    /*-----------------------------------------------------------------------*/
    renderPassAttachments[0].format = swapchainFormat.format;
    renderPassAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    renderPassAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    renderPassAttachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    renderPassAttachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    renderPassAttachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    renderPassAttachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    renderPassAttachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    renderPassColorAttachments[0].attachment = 0;
    renderPassColorAttachments[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    /*-----------------------------------------------------------------------*/
    /* Subpasses                                                             */
    /*-----------------------------------------------------------------------*/
    VkSubpassDescription renderPassSubpasses[1];
    ZeroMemory(&renderPassSubpasses[0], sizeof(VkSubpassDescription));
    renderPassSubpasses[0].colorAttachmentCount = 1;
    renderPassSubpasses[0].pColorAttachments = renderPassColorAttachments;
    ZeroMemory(&create_info, sizeof(VkRenderPassCreateInfo));
    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    create_info.attachmentCount = renderPassAttachmentCount;
    create_info.pAttachments = renderPassAttachments;
    create_info.subpassCount = 1;
    create_info.pSubpasses = renderPassSubpasses;
    vkCreateRenderPass(_device, &create_info, NULL, &blurRenderPass);
}

void RenderSystem::CreateUIRenderPass(VkDevice _device)
{
    VkRenderPassCreateInfo create_info;
    /*=======================================================================*/
    /* Static Mesh Render Pass                                               */
    /*=======================================================================*/
    VkAttachmentDescription renderPassAttachments[1];
    ZeroMemory(renderPassAttachments, sizeof(VkAttachmentDescription));
    /*-----------------------------------------------------------------------*/
    /* Color Attachment                                                      */
    /*-----------------------------------------------------------------------*/
    renderPassAttachments[0].format = swapchainFormat.format;
    renderPassAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    renderPassAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    renderPassAttachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    renderPassAttachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    renderPassAttachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    renderPassAttachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    renderPassAttachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkAttachmentReference renderPassColorAttachment;
    ZeroMemory(&renderPassColorAttachment, sizeof(VkAttachmentReference));
    renderPassColorAttachment.attachment = 0;
    renderPassColorAttachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    /*-----------------------------------------------------------------------*/
    /* Subpasses                                                             */
    /*-----------------------------------------------------------------------*/
    VkSubpassDescription renderPassSubpasses[1];
    ZeroMemory(&renderPassSubpasses[0], sizeof(VkSubpassDescription));
    renderPassSubpasses[0].colorAttachmentCount = 1;
    renderPassSubpasses[0].pColorAttachments = &renderPassColorAttachment;
    /*-----------------------------------------------------------------------*/
    /* Create Info                                                           */
    /*-----------------------------------------------------------------------*/
    ZeroMemory(&create_info, sizeof(VkRenderPassCreateInfo));
    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    create_info.attachmentCount = 1;
    create_info.pAttachments = renderPassAttachments;
    create_info.subpassCount = 1;
    create_info.pSubpasses = renderPassSubpasses;
    vkCreateRenderPass(_device, &create_info, NULL, &uiRenderPass);
}

void RenderSystem::DestroyRenderPasses(VkDevice _device)
{
    vkDestroyRenderPass(_device, shadowMapRenderPass, NULL);
    vkDestroyRenderPass(_device, staticMeshRenderPass, NULL);
    vkDestroyRenderPass(_device, blurRenderPass, NULL);
    vkDestroyRenderPass(_device, uiRenderPass, NULL);
}

void RenderSystem::CreateImmutableSamplers(VkDevice _device)
{
    VkSamplerCreateInfo create_info;
    ZeroMemory(&create_info, sizeof(VkSamplerCreateInfo));
    create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    create_info.minFilter = VK_FILTER_LINEAR;
    create_info.magFilter = VK_FILTER_LINEAR;
    create_info.maxLod = VK_LOD_CLAMP_NONE;
    create_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    vkCreateSampler(_device, &create_info, NULL, &staticMeshTextureSampler);
    vkCreateSampler(_device, &create_info, NULL, &presentSampler);
    create_info.minFilter = VK_FILTER_NEAREST;
    create_info.magFilter = VK_FILTER_NEAREST;
    vkCreateSampler(_device, &create_info, NULL, &uiBlitSampler);
}

void RenderSystem::DestroyImmutableSamplers(VkDevice _device)
{
    vkDestroySampler(_device, staticMeshTextureSampler, NULL);
    vkDestroySampler(_device, presentSampler, NULL);
    vkDestroySampler(_device, uiBlitSampler, NULL);
}

void RenderSystem::CreateDescriptorSetLayouts(VkDevice _device)
{
    VkDescriptorSetLayoutBinding bindings[4];
    ZeroMemory(bindings, sizeof(VkDescriptorSetLayoutBinding) * 3);
    VkDescriptorSetLayoutCreateInfo create_info;
    ZeroMemory(&create_info, sizeof(VkDescriptorSetLayoutCreateInfo));
    create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    /*-----------------------------------------------------------------------*/
    bindings[0].binding = 0;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[0].descriptorCount= 1;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings[1].binding = 0;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].descriptorCount= 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings[2].binding = 0;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[2].descriptorCount= 1;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings[3].binding = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[3].descriptorCount= 1;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    /*-----------------------------------------------------------------------*/
    create_info.bindingCount = 2;
    create_info.pBindings = &bindings[2];
    bindings[3].pImmutableSamplers = &staticMeshTextureSampler;
    vkCreateDescriptorSetLayout(_device, &create_info, NULL, &staticMeshDescriptorSetLayout);
    /*-----------------------------------------------------------------------*/
    vkCreateDescriptorSetLayout(_device, &create_info, NULL, &uiSDFDescriptorSetLayout);
    bindings[3].pImmutableSamplers = &uiBlitSampler;
    vkCreateDescriptorSetLayout(_device, &create_info, NULL, &uiBlitDescriptorSetLayout);
    /*-----------------------------------------------------------------------*/
    bindings[1].binding = 1;
    bindings[2].binding = 2;
    bindings[3].binding = 3;
    create_info.bindingCount = 4;
    create_info.pBindings = bindings;
    bindings[3].pImmutableSamplers = &presentSampler;
    vkCreateDescriptorSetLayout(_device, &create_info, NULL, &presentDescriptorSetLayout);
}

void RenderSystem::DestroyDescriptorSetLayouts(VkDevice _device)
{
    vkDestroyDescriptorSetLayout(_device, staticMeshDescriptorSetLayout, NULL);
    vkDestroyDescriptorSetLayout(_device, uiSDFDescriptorSetLayout, NULL);
    vkDestroyDescriptorSetLayout(_device, uiBlitDescriptorSetLayout, NULL);
    vkDestroyDescriptorSetLayout(_device, presentDescriptorSetLayout, NULL);
}

void RenderSystem::CreatePipelineLayouts(VkDevice _device)
{
    VkPushConstantRange pushConstantRanges[1];
    VkDescriptorSetLayout descriptorSetLayouts[2];
    VkPipelineLayoutCreateInfo create_info;
    ZeroMemory(&create_info, sizeof(VkPipelineLayoutCreateInfo));
    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    create_info.pSetLayouts=descriptorSetLayouts;
    create_info.pPushConstantRanges=pushConstantRanges;
   /*-----------------------------------------------------------------------*/
    pushConstantRanges[0].size = sizeof(GMATRIXF);
    pushConstantRanges[0].offset=0;
    pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    create_info.pushConstantRangeCount=1;
    vkCreatePipelineLayout(_device, &create_info, NULL, &shadowMapPipelineLayout);
    /*-----------------------------------------------------------------------*/
    create_info.setLayoutCount= 1;
    descriptorSetLayouts[0]=staticMeshDescriptorSetLayout;
    vkCreatePipelineLayout(_device, &create_info, NULL, &skyBoxPipelineLayout);
    /*-----------------------------------------------------------------------*/
    pushConstantRanges[0].size = sizeof(GMATRIXF) * 2;
    create_info.setLayoutCount= 2;
    descriptorSetLayouts[1]=uiBlitDescriptorSetLayout;
    vkCreatePipelineLayout(_device, &create_info, NULL, &staticMeshPipelineLayout);
    /*-----------------------------------------------------------------------*/
    pushConstantRanges[0].size = sizeof(uint32_t);
    pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    create_info.setLayoutCount= 1;
    descriptorSetLayouts[0]=uiBlitDescriptorSetLayout;
    vkCreatePipelineLayout(_device, &create_info, NULL, &blurPipelineLayout);
    /*-----------------------------------------------------------------------*/
    pushConstantRanges[0].size = sizeof(GVECTORF) * 3;
    descriptorSetLayouts[0]=uiSDFDescriptorSetLayout;
    vkCreatePipelineLayout(_device, &create_info, NULL, &uiSDFPipelineLayout);
    /*-----------------------------------------------------------------------*/
    create_info.pushConstantRangeCount=0;
    descriptorSetLayouts[0]=uiBlitDescriptorSetLayout;
    vkCreatePipelineLayout(_device, &create_info, NULL, &uiBlitPipelineLayout);
    /*-----------------------------------------------------------------------*/
    descriptorSetLayouts[0]=presentDescriptorSetLayout;
    vkCreatePipelineLayout(_device, &create_info, NULL, &presentPipelineLayout);
}

void RenderSystem::DestroyPipelineLayouts(VkDevice _device)
{
    vkDestroyPipelineLayout(_device, skyBoxPipelineLayout, NULL);
    vkDestroyPipelineLayout(_device, staticMeshPipelineLayout, NULL);
    vkDestroyPipelineLayout(_device, blurPipelineLayout, NULL);
    vkDestroyPipelineLayout(_device, shadowMapPipelineLayout, NULL);
    vkDestroyPipelineLayout(_device, uiSDFPipelineLayout, NULL);
    vkDestroyPipelineLayout(_device, uiBlitPipelineLayout, NULL);
    vkDestroyPipelineLayout(_device, presentPipelineLayout, NULL);
}

void RenderSystem::CreateShadowMapPipeline(VkDevice _device)
{
    /*=======================================================================*/
    /* Shader Stages                                                         */
    /*=======================================================================*/
    VkPipelineShaderStageCreateInfo stage_create_info[2];
    ZeroMemory(stage_create_info, sizeof(VkPipelineShaderStageCreateInfo) * 2);
    /*-----------------------------------------------------------------------*/
    /* Vertex Shader                                                         */
    /*-----------------------------------------------------------------------*/
    stage_create_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_create_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage_create_info[0].pName = "main";
    stage_create_info[0].module = shadowMapVertexShader;
    /*-----------------------------------------------------------------------*/
    /* Fragment Shader                                                       */
    /*-----------------------------------------------------------------------*/
    stage_create_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_create_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stage_create_info[1].pName = "main";
    stage_create_info[1].module = shadowMapPixelShader;
    /*=======================================================================*/
    /* Input Assembly State                                                  */
    /*=======================================================================*/
    VkPipelineInputAssemblyStateCreateInfo assembly_create_info;
    ZeroMemory(&assembly_create_info, sizeof(VkPipelineInputAssemblyStateCreateInfo));
    assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    assembly_create_info.primitiveRestartEnable = false;
    /*=======================================================================*/
    /* Vertex Input State                                                    */
    /*=======================================================================*/
    VkPipelineVertexInputStateCreateInfo input_vertex_info = {};
    input_vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    /*-----------------------------------------------------------------------*/
    /* Vertex Bindings                                                       */
    /*-----------------------------------------------------------------------*/
    VkVertexInputBindingDescription vertex_binding_descriptions[2];
    ZeroMemory(vertex_binding_descriptions, sizeof(VkVertexInputBindingDescription) * 2);
    /*-----------------------------------------------------------------------*/
    /* Per Vertex Data                                                       */
    /*-----------------------------------------------------------------------*/
    vertex_binding_descriptions[0].binding = 0;
    vertex_binding_descriptions[0].stride = sizeof(H2B::VERTEX);
    vertex_binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    /*-----------------------------------------------------------------------*/
    /* Per Instance Data                                                     */
    /*-----------------------------------------------------------------------*/
    vertex_binding_descriptions[1].binding = 1;
    vertex_binding_descriptions[1].stride = sizeof(MeshInstanceData);
    vertex_binding_descriptions[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    /*-----------------------------------------------------------------------*/
    input_vertex_info.vertexBindingDescriptionCount = 2;
    input_vertex_info.pVertexBindingDescriptions = vertex_binding_descriptions;
    /*-----------------------------------------------------------------------*/
    /* Vertex Attributes                                                     */
    /*-----------------------------------------------------------------------*/
    VkVertexInputAttributeDescription vertex_attribute_descriptions[5];
    ZeroMemory(vertex_attribute_descriptions, sizeof(VkVertexInputAttributeDescription) * 5);
    /*-----------------------------------------------------------------------*/
    /* Vertex - Position                                                     */
    /*-----------------------------------------------------------------------*/
    vertex_attribute_descriptions[0].binding=0;
    vertex_attribute_descriptions[0].location= 0;
    vertex_attribute_descriptions[0].offset= sizeof(H2B::VECTOR) * 0;
    vertex_attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    /*-----------------------------------------------------------------------*/
    /* Instance - World Transform Matrix                                     */
    /*-----------------------------------------------------------------------*/
    vertex_attribute_descriptions[1].binding= 1;
    vertex_attribute_descriptions[1].location= 1;
    vertex_attribute_descriptions[1].offset= sizeof(GVECTORF) * 0;
    vertex_attribute_descriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vertex_attribute_descriptions[2].binding= 1;
    vertex_attribute_descriptions[2].location= 2;
    vertex_attribute_descriptions[2].offset= sizeof(GVECTORF) * 1;
    vertex_attribute_descriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vertex_attribute_descriptions[3].binding= 1;
    vertex_attribute_descriptions[3].location= 3;
    vertex_attribute_descriptions[3].offset= sizeof(GVECTORF) * 2;
    vertex_attribute_descriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vertex_attribute_descriptions[4].binding= 1;
    vertex_attribute_descriptions[4].location= 4;
    vertex_attribute_descriptions[4].offset= sizeof(GVECTORF) * 3;
    vertex_attribute_descriptions[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    /*-----------------------------------------------------------------------*/
    input_vertex_info.vertexAttributeDescriptionCount = 5;
    input_vertex_info.pVertexAttributeDescriptions = vertex_attribute_descriptions;
    /*=======================================================================*/
    /* Viewport State                                                        */
    /*=======================================================================*/
    VkPipelineViewportStateCreateInfo viewport_create_info;
    ZeroMemory(&viewport_create_info, sizeof(VkPipelineViewportStateCreateInfo));
    viewport_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_create_info.viewportCount = 1;
    viewport_create_info.scissorCount = 1;
    /*=======================================================================*/
    /* Rasterization State                                                   */
    /*=======================================================================*/
    VkPipelineRasterizationStateCreateInfo rasterization_create_info;
    ZeroMemory(&rasterization_create_info, sizeof(VkPipelineRasterizationStateCreateInfo));
    rasterization_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_create_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_create_info.lineWidth = 1.0f;
    rasterization_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization_create_info.depthBiasEnable = VK_TRUE;
    rasterization_create_info.depthBiasConstantFactor = 0.005F;
    /*=======================================================================*/
    /* Multisampling State                                                   */
    /*=======================================================================*/
    VkPipelineMultisampleStateCreateInfo multisample_create_info;
    ZeroMemory(&multisample_create_info, sizeof(VkPipelineMultisampleStateCreateInfo));
    multisample_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample_create_info.minSampleShading = 1.0f;
    /*=======================================================================*/
    /* Depth Stencil State                                                   */
    /*=======================================================================*/
    VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info;
    ZeroMemory(&depth_stencil_create_info, sizeof(VkPipelineDepthStencilStateCreateInfo));
    depth_stencil_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_create_info.minDepthBounds = 0.0f;
    depth_stencil_create_info.maxDepthBounds = 1.0f;
    depth_stencil_create_info.depthTestEnable = VK_TRUE;
    depth_stencil_create_info.depthWriteEnable = VK_TRUE;
    depth_stencil_create_info.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    /*=======================================================================*/
    /* Color Blend State                                                     */
    /*=======================================================================*/
    VkPipelineColorBlendStateCreateInfo color_blend_create_info;
    ZeroMemory(&color_blend_create_info, sizeof(VkPipelineColorBlendStateCreateInfo));
    color_blend_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    /*=======================================================================*/
    /* Dynamic State                                                         */
    /*=======================================================================*/
    VkDynamicState dynamic_state[2] = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamic_create_info = {};
    dynamic_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_create_info.dynamicStateCount = 2;
    dynamic_create_info.pDynamicStates = dynamic_state;
    /*=======================================================================*/
    /*                                                                       */
    /*=======================================================================*/
    VkGraphicsPipelineCreateInfo create_info;
    ZeroMemory(&create_info, sizeof(VkGraphicsPipelineCreateInfo));
    create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create_info.stageCount = 2;
    create_info.pStages = stage_create_info;
    create_info.pInputAssemblyState = &assembly_create_info;
    create_info.pVertexInputState = &input_vertex_info;
    create_info.pViewportState = &viewport_create_info;
    create_info.pRasterizationState = &rasterization_create_info;
    create_info.pMultisampleState = &multisample_create_info;
    create_info.pDepthStencilState = &depth_stencil_create_info;
    create_info.pColorBlendState = &color_blend_create_info;
    create_info.pDynamicState = &dynamic_create_info;
    create_info.layout = shadowMapPipelineLayout;
    create_info.renderPass = shadowMapRenderPass;
    vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &create_info, NULL, &shadowMapPipeline);
}

void RenderSystem::CreateStaticMeshPipeline(VkDevice _device)
{
    /*=======================================================================*/
    /* Shader Stages                                                         */
    /*=======================================================================*/
    VkPipelineShaderStageCreateInfo stage_create_info[2];
    ZeroMemory(stage_create_info, sizeof(VkPipelineShaderStageCreateInfo) * 2);
    /*-----------------------------------------------------------------------*/
    /* Vertex Shader                                                         */
    /*-----------------------------------------------------------------------*/
    stage_create_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_create_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage_create_info[0].pName = "main";
    stage_create_info[0].module = staticMeshVertexShader;
    /*-----------------------------------------------------------------------*/
    /* Fragment Shader                                                       */
    /*-----------------------------------------------------------------------*/
    stage_create_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_create_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stage_create_info[1].pName = "main";
    stage_create_info[1].module = staticMeshPixelShader;
    /*=======================================================================*/
    /* Input Assembly State                                                  */
    /*=======================================================================*/
    VkPipelineInputAssemblyStateCreateInfo assembly_create_info;
    ZeroMemory(&assembly_create_info, sizeof(VkPipelineInputAssemblyStateCreateInfo));
    assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    assembly_create_info.primitiveRestartEnable = false;
    /*=======================================================================*/
    /* Vertex Input State                                                    */
    /*=======================================================================*/
    VkPipelineVertexInputStateCreateInfo input_vertex_info = {};
    input_vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    /*-----------------------------------------------------------------------*/
    /* Vertex Bindings                                                       */
    /*-----------------------------------------------------------------------*/
    VkVertexInputBindingDescription vertex_binding_descriptions[2];
    ZeroMemory(vertex_binding_descriptions, sizeof(VkVertexInputBindingDescription) * 2);
    /*-----------------------------------------------------------------------*/
    /* Per Vertex Data                                                       */
    /*-----------------------------------------------------------------------*/
    vertex_binding_descriptions[0].binding = 0;
    vertex_binding_descriptions[0].stride = sizeof(H2B::VERTEX);
    vertex_binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    /*-----------------------------------------------------------------------*/
    /* Per Instance Data                                                     */
    /*-----------------------------------------------------------------------*/
    vertex_binding_descriptions[1].binding = 1;
    vertex_binding_descriptions[1].stride = sizeof(MeshInstanceData);
    vertex_binding_descriptions[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    /*-----------------------------------------------------------------------*/
    input_vertex_info.vertexBindingDescriptionCount = 2;
    input_vertex_info.pVertexBindingDescriptions = vertex_binding_descriptions;
    /*-----------------------------------------------------------------------*/
    /* Vertex Attributes                                                     */
    /*-----------------------------------------------------------------------*/
    VkVertexInputAttributeDescription vertex_attribute_descriptions[9];
    ZeroMemory(vertex_attribute_descriptions, sizeof(VkVertexInputAttributeDescription) * 9);
    /*-----------------------------------------------------------------------*/
    /* Vertex - Position                                                     */
    /*-----------------------------------------------------------------------*/
    vertex_attribute_descriptions[0].binding=0;
    vertex_attribute_descriptions[0].location= 0;
    vertex_attribute_descriptions[0].offset= sizeof(H2B::VECTOR) * 0;
    vertex_attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    /*-----------------------------------------------------------------------*/
    /* Vertex - Texture Coordinates                                          */
    /*-----------------------------------------------------------------------*/
    vertex_attribute_descriptions[1].binding=0;
    vertex_attribute_descriptions[1].location= 1;
    vertex_attribute_descriptions[1].offset= sizeof(H2B::VECTOR) * 1;
    vertex_attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    /*-----------------------------------------------------------------------*/
    /* Vertex - Normal                                                       */
    /*-----------------------------------------------------------------------*/
    vertex_attribute_descriptions[2].binding=0;
    vertex_attribute_descriptions[2].location= 2;
    vertex_attribute_descriptions[2].offset= sizeof(H2B::VECTOR) * 2;
    vertex_attribute_descriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    /*-----------------------------------------------------------------------*/
    /* Instance - World Transform Matrix                                     */
    /*-----------------------------------------------------------------------*/
    vertex_attribute_descriptions[3].binding= 1;
    vertex_attribute_descriptions[3].location= 3;
    vertex_attribute_descriptions[3].offset= sizeof(GVECTORF) * 0;
    vertex_attribute_descriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vertex_attribute_descriptions[4].binding= 1;
    vertex_attribute_descriptions[4].location= 4;
    vertex_attribute_descriptions[4].offset= sizeof(GVECTORF) * 1;
    vertex_attribute_descriptions[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vertex_attribute_descriptions[5].binding= 1;
    vertex_attribute_descriptions[5].location= 5;
    vertex_attribute_descriptions[5].offset= sizeof(GVECTORF) * 2;
    vertex_attribute_descriptions[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vertex_attribute_descriptions[6].binding= 1;
    vertex_attribute_descriptions[6].location= 6;
    vertex_attribute_descriptions[6].offset= sizeof(GVECTORF) * 3;
    vertex_attribute_descriptions[6].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    /*-----------------------------------------------------------------------*/
    vertex_attribute_descriptions[7].binding= 1;
    vertex_attribute_descriptions[7].location= 7;
    vertex_attribute_descriptions[7].offset= sizeof(GVECTORF) * 4;
    vertex_attribute_descriptions[7].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    /*-----------------------------------------------------------------------*/
    vertex_attribute_descriptions[8].binding= 1;
    vertex_attribute_descriptions[8].location= 8;
    vertex_attribute_descriptions[8].offset= sizeof(GVECTORF) * 5;
    vertex_attribute_descriptions[8].format = VK_FORMAT_R32_UINT;
    /*-----------------------------------------------------------------------*/
    input_vertex_info.vertexAttributeDescriptionCount = 9;
    input_vertex_info.pVertexAttributeDescriptions = vertex_attribute_descriptions;
    /*=======================================================================*/
    /* Viewport State                                                        */
    /*=======================================================================*/
    VkPipelineViewportStateCreateInfo viewport_create_info;
    ZeroMemory(&viewport_create_info, sizeof(VkPipelineViewportStateCreateInfo));
    viewport_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_create_info.viewportCount = 1;
    viewport_create_info.scissorCount = 1;
    /*=======================================================================*/
    /* Rasterization State                                                   */
    /*=======================================================================*/
    VkPipelineRasterizationStateCreateInfo rasterization_create_info;
    ZeroMemory(&rasterization_create_info, sizeof(VkPipelineRasterizationStateCreateInfo));
    rasterization_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_create_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_create_info.lineWidth = 1.0f;
    rasterization_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    /*=======================================================================*/
    /* Multisampling State                                                   */
    /*=======================================================================*/
    VkPipelineMultisampleStateCreateInfo multisample_create_info;
    ZeroMemory(&multisample_create_info, sizeof(VkPipelineMultisampleStateCreateInfo));
    multisample_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample_create_info.minSampleShading = 1.0f;
    /*=======================================================================*/
    /* Depth Stencil State                                                   */
    /*=======================================================================*/
    VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info;
    ZeroMemory(&depth_stencil_create_info, sizeof(VkPipelineDepthStencilStateCreateInfo));
    depth_stencil_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_create_info.minDepthBounds = 0.0f;
    depth_stencil_create_info.maxDepthBounds = 1.0f;
    depth_stencil_create_info.depthTestEnable = VK_TRUE;
    depth_stencil_create_info.depthWriteEnable = VK_TRUE;
    depth_stencil_create_info.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    /*=======================================================================*/
    /* Color Blend State                                                     */
    /*=======================================================================*/
    VkPipelineColorBlendAttachmentState color_blend_attachment_states[2] = {};
    color_blend_attachment_states[0].colorWriteMask = 0xF;
    color_blend_attachment_states[0].blendEnable = VK_FALSE;
    color_blend_attachment_states[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
    color_blend_attachment_states[0].dstColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
    color_blend_attachment_states[0].colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_states[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment_states[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    color_blend_attachment_states[0].alphaBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_states[1].colorWriteMask = 0xF;
    color_blend_attachment_states[1].blendEnable = VK_FALSE;
    color_blend_attachment_states[1].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
    color_blend_attachment_states[1].dstColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
    color_blend_attachment_states[1].colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_states[1].srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment_states[1].dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    color_blend_attachment_states[1].alphaBlendOp = VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo color_blend_create_info;
    ZeroMemory(&color_blend_create_info, sizeof(VkPipelineColorBlendStateCreateInfo));
    color_blend_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_create_info.attachmentCount = 2;
    color_blend_create_info.pAttachments = color_blend_attachment_states;
    /*=======================================================================*/
    /* Dynamic State                                                         */
    /*=======================================================================*/
    VkDynamicState dynamic_state[2] = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamic_create_info = {};
    dynamic_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_create_info.dynamicStateCount = 2;
    dynamic_create_info.pDynamicStates = dynamic_state;
    /*=======================================================================*/
    /*                                                                       */
    /*=======================================================================*/
    VkGraphicsPipelineCreateInfo create_info;
    ZeroMemory(&create_info, sizeof(VkGraphicsPipelineCreateInfo));
    create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create_info.stageCount = 2;
    create_info.pStages = stage_create_info;
    create_info.pInputAssemblyState = &assembly_create_info;
    create_info.pVertexInputState = &input_vertex_info;
    create_info.pViewportState = &viewport_create_info;
    create_info.pRasterizationState = &rasterization_create_info;
    create_info.pMultisampleState = &multisample_create_info;
    create_info.pDepthStencilState = &depth_stencil_create_info;
    create_info.pColorBlendState = &color_blend_create_info;
    create_info.pDynamicState = &dynamic_create_info;
    create_info.layout = staticMeshPipelineLayout;
    create_info.renderPass = staticMeshRenderPass;
    vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &create_info, NULL, &staticMeshPipeline);
}

void RenderSystem::CreateBlurPipeline(VkDevice _device)
{
    /*=======================================================================*/
    /* Shader Stages                                                         */
    /*=======================================================================*/
    VkPipelineShaderStageCreateInfo stage_create_info[2];
    ZeroMemory(stage_create_info, sizeof(VkPipelineShaderStageCreateInfo) * 2);
    /*-----------------------------------------------------------------------*/
    /* Vertex Shader                                                         */
    /*-----------------------------------------------------------------------*/
    stage_create_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_create_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage_create_info[0].pName = "main";
    stage_create_info[0].module = blurVertexShader;
    /*-----------------------------------------------------------------------*/
    /* Fragment Shader                                                       */
    /*-----------------------------------------------------------------------*/
    stage_create_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_create_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stage_create_info[1].pName = "main";
    stage_create_info[1].module = blurPixelShader;
    /*=======================================================================*/
    /* Input Assembly State                                                  */
    /*=======================================================================*/
    VkPipelineInputAssemblyStateCreateInfo assembly_create_info;
    ZeroMemory(&assembly_create_info, sizeof(VkPipelineInputAssemblyStateCreateInfo));
    assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    assembly_create_info.primitiveRestartEnable = false;
    /*=======================================================================*/
    /* Vertex Input State                                                    */
    /*=======================================================================*/
    VkPipelineVertexInputStateCreateInfo input_vertex_info = {};
    input_vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    /*=======================================================================*/
    /* Viewport State                                                        */
    /*=======================================================================*/
    VkPipelineViewportStateCreateInfo viewport_create_info;
    ZeroMemory(&viewport_create_info, sizeof(VkPipelineViewportStateCreateInfo));
    viewport_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_create_info.viewportCount = 1;
    viewport_create_info.scissorCount = 1;
    /*=======================================================================*/
    /* Rasterization State                                                   */
    /*=======================================================================*/
    VkPipelineRasterizationStateCreateInfo rasterization_create_info;
    ZeroMemory(&rasterization_create_info, sizeof(VkPipelineRasterizationStateCreateInfo));
    rasterization_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_create_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_create_info.lineWidth = 1.0f;
    rasterization_create_info.cullMode = VK_CULL_MODE_NONE;
    rasterization_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    /*=======================================================================*/
    /* Multisampling State                                                   */
    /*=======================================================================*/
    VkPipelineMultisampleStateCreateInfo multisample_create_info;
    ZeroMemory(&multisample_create_info, sizeof(VkPipelineMultisampleStateCreateInfo));
    multisample_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample_create_info.minSampleShading = 1.0f;
    /*=======================================================================*/
    /* Depth Stencil State                                                   */
    /*=======================================================================*/
    VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info;
    ZeroMemory(&depth_stencil_create_info, sizeof(VkPipelineDepthStencilStateCreateInfo));
    depth_stencil_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_create_info.minDepthBounds = 0.0f;
    depth_stencil_create_info.maxDepthBounds = 1.0f;
    /*=======================================================================*/
    /* Color Blend State                                                     */
    /*=======================================================================*/
    VkPipelineColorBlendAttachmentState color_blend_attachment_state = {};
    color_blend_attachment_state.colorWriteMask = 0xF;
    color_blend_attachment_state.blendEnable = VK_TRUE;
    color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo color_blend_create_info;
    ZeroMemory(&color_blend_create_info, sizeof(VkPipelineColorBlendStateCreateInfo));
    color_blend_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_create_info.attachmentCount = 1;
    color_blend_create_info.pAttachments = &color_blend_attachment_state;
    /*=======================================================================*/
    /* Dynamic State                                                         */
    /*=======================================================================*/
    VkDynamicState dynamic_state[2] = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamic_create_info = {};
    dynamic_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_create_info.dynamicStateCount = 2;
    dynamic_create_info.pDynamicStates = dynamic_state;
    /*=======================================================================*/
    VkGraphicsPipelineCreateInfo create_info;
    ZeroMemory(&create_info, sizeof(VkGraphicsPipelineCreateInfo));
    create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create_info.stageCount = 2;
    create_info.pStages = stage_create_info;
    create_info.pInputAssemblyState = &assembly_create_info;
    create_info.pVertexInputState = &input_vertex_info;
    create_info.pViewportState = &viewport_create_info;
    create_info.pRasterizationState = &rasterization_create_info;
    create_info.pMultisampleState = &multisample_create_info;
    create_info.pDepthStencilState = &depth_stencil_create_info;
    create_info.pColorBlendState = &color_blend_create_info;
    create_info.pDynamicState = &dynamic_create_info;
    create_info.layout = blurPipelineLayout;
    create_info.renderPass = blurRenderPass;
    vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &create_info, NULL, &blurPipeline);
}

void RenderSystem::CreateSkyboxPipeline(VkDevice _device)
{
    /*=======================================================================*/
    /* Shader Stages                                                         */
    /*=======================================================================*/
    VkPipelineShaderStageCreateInfo stage_create_info[2];
    ZeroMemory(stage_create_info, sizeof(VkPipelineShaderStageCreateInfo) * 2);
    /*-----------------------------------------------------------------------*/
    /* Vertex Shader                                                         */
    /*-----------------------------------------------------------------------*/
    stage_create_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_create_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage_create_info[0].pName = "main";
    stage_create_info[0].module = skyBoxVertexShader;
    /*-----------------------------------------------------------------------*/
    /* Fragment Shader                                                       */
    /*-----------------------------------------------------------------------*/
    stage_create_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_create_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stage_create_info[1].pName = "main";
    stage_create_info[1].module = skyBoxPixelShader;
    /*=======================================================================*/
    /* Input Assembly State                                                  */
    /*=======================================================================*/
    VkPipelineInputAssemblyStateCreateInfo assembly_create_info;
    ZeroMemory(&assembly_create_info, sizeof(VkPipelineInputAssemblyStateCreateInfo));
    assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    assembly_create_info.primitiveRestartEnable = false;
    /*=======================================================================*/
    /* Vertex Input State                                                    */
    /*=======================================================================*/
    VkPipelineVertexInputStateCreateInfo input_vertex_info = {};
    input_vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    /*=======================================================================*/
    /* Viewport State                                                        */
    /*=======================================================================*/
    VkPipelineViewportStateCreateInfo viewport_create_info;
    ZeroMemory(&viewport_create_info, sizeof(VkPipelineViewportStateCreateInfo));
    viewport_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_create_info.viewportCount = 1;
    viewport_create_info.scissorCount = 1;
    /*=======================================================================*/
    /* Rasterization State                                                   */
    /*=======================================================================*/
    VkPipelineRasterizationStateCreateInfo rasterization_create_info;
    ZeroMemory(&rasterization_create_info, sizeof(VkPipelineRasterizationStateCreateInfo));
    rasterization_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_create_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_create_info.lineWidth = 1.0f;
    rasterization_create_info.cullMode = VK_CULL_MODE_NONE;
    rasterization_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    /*=======================================================================*/
    /* Multisampling State                                                   */
    /*=======================================================================*/
    VkPipelineMultisampleStateCreateInfo multisample_create_info;
    ZeroMemory(&multisample_create_info, sizeof(VkPipelineMultisampleStateCreateInfo));
    multisample_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample_create_info.minSampleShading = 1.0f;
    /*=======================================================================*/
    /* Depth Stencil State                                                   */
    /*=======================================================================*/
    VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info;
    ZeroMemory(&depth_stencil_create_info, sizeof(VkPipelineDepthStencilStateCreateInfo));
    depth_stencil_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_create_info.minDepthBounds = 0.0f;
    depth_stencil_create_info.maxDepthBounds = 1.0f;
    depth_stencil_create_info.depthTestEnable = VK_TRUE;
    depth_stencil_create_info.depthWriteEnable = VK_FALSE;
    depth_stencil_create_info.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    /*=======================================================================*/
    /* Color Blend State                                                     */
    /*=======================================================================*/
    VkPipelineColorBlendAttachmentState color_blend_attachment_states[2] = {};
    color_blend_attachment_states[0].colorWriteMask = 0xF;
    color_blend_attachment_states[0].blendEnable = VK_FALSE;
    color_blend_attachment_states[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
    color_blend_attachment_states[0].dstColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
    color_blend_attachment_states[0].colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_states[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment_states[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    color_blend_attachment_states[0].alphaBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_states[1].colorWriteMask = 0xF;
    color_blend_attachment_states[1].blendEnable = VK_FALSE;
    color_blend_attachment_states[1].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
    color_blend_attachment_states[1].dstColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
    color_blend_attachment_states[1].colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_states[1].srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment_states[1].dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    color_blend_attachment_states[1].alphaBlendOp = VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo color_blend_create_info;
    ZeroMemory(&color_blend_create_info, sizeof(VkPipelineColorBlendStateCreateInfo));
    color_blend_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_create_info.attachmentCount = 2;
    color_blend_create_info.pAttachments = color_blend_attachment_states;
    /*=======================================================================*/
    /* Dynamic State                                                         */
    /*=======================================================================*/
    VkDynamicState dynamic_state[2] = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamic_create_info = {};
    dynamic_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_create_info.dynamicStateCount = 2;
    dynamic_create_info.pDynamicStates = dynamic_state;
    /*=======================================================================*/
    /*                                                                       */
    /*=======================================================================*/
    VkGraphicsPipelineCreateInfo create_info;
    ZeroMemory(&create_info, sizeof(VkGraphicsPipelineCreateInfo));
    create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create_info.stageCount = 2;
    create_info.pStages = stage_create_info;
    create_info.pInputAssemblyState = &assembly_create_info;
    create_info.pVertexInputState = &input_vertex_info;
    create_info.pViewportState = &viewport_create_info;
    create_info.pRasterizationState = &rasterization_create_info;
    create_info.pMultisampleState = &multisample_create_info;
    create_info.pDepthStencilState = &depth_stencil_create_info;
    create_info.pColorBlendState = &color_blend_create_info;
    create_info.pDynamicState = &dynamic_create_info;
    create_info.layout = skyBoxPipelineLayout;
    create_info.renderPass = staticMeshRenderPass;
    vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &create_info, NULL, &skyBoxPipeline);
}
#ifdef DEV_BUILD
void RenderSystem::CreateDebugColliderPipeline(VkDevice _device)
{
    /*=======================================================================*/
    /* Shader Stages                                                         */
    /*=======================================================================*/
    VkPipelineShaderStageCreateInfo stage_create_info[2];
    ZeroMemory(stage_create_info, sizeof(VkPipelineShaderStageCreateInfo) * 2);
    /*-----------------------------------------------------------------------*/
    /* Vertex Shader                                                         */
    /*-----------------------------------------------------------------------*/
    stage_create_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_create_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage_create_info[0].pName = "main";
    stage_create_info[0].module = debugColliderVertexShader;
    /*-----------------------------------------------------------------------*/
    /* Fragment Shader                                                       */
    /*-----------------------------------------------------------------------*/
    stage_create_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_create_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stage_create_info[1].pName = "main";
    stage_create_info[1].module = debugColliderPixelShader;
    /*=======================================================================*/
    /* Input Assembly State                                                  */
    /*=======================================================================*/
    VkPipelineInputAssemblyStateCreateInfo assembly_create_info;
    ZeroMemory(&assembly_create_info, sizeof(VkPipelineInputAssemblyStateCreateInfo));
    assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    assembly_create_info.primitiveRestartEnable = false;
    /*=======================================================================*/
    /* Vertex Input State                                                    */
    /*=======================================================================*/
    VkPipelineVertexInputStateCreateInfo input_vertex_info = {};
    input_vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    /*-----------------------------------------------------------------------*/
    /* Vertex Bindings                                                       */
    /*-----------------------------------------------------------------------*/
    VkVertexInputBindingDescription vertex_binding_descriptions[2];
    ZeroMemory(vertex_binding_descriptions, sizeof(VkVertexInputBindingDescription) * 2);
    /*-----------------------------------------------------------------------*/
    /* Per Vertex Data                                                       */
    /*-----------------------------------------------------------------------*/
    vertex_binding_descriptions[0].binding = 0;
    vertex_binding_descriptions[0].stride = sizeof(H2B::VECTOR);
    vertex_binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    /*-----------------------------------------------------------------------*/
    /* Per Instance Data                                                     */
    /*-----------------------------------------------------------------------*/
    vertex_binding_descriptions[1].binding = 1;
    vertex_binding_descriptions[1].stride = sizeof(MeshInstanceData);
    vertex_binding_descriptions[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    /*-----------------------------------------------------------------------*/
    input_vertex_info.vertexBindingDescriptionCount = 2;
    input_vertex_info.pVertexBindingDescriptions = vertex_binding_descriptions;
    /*-----------------------------------------------------------------------*/
    /* Vertex Attributes                                                     */
    /*-----------------------------------------------------------------------*/
    VkVertexInputAttributeDescription vertex_attribute_descriptions[5];
    ZeroMemory(vertex_attribute_descriptions, sizeof(VkVertexInputAttributeDescription) * 5);
    /*-----------------------------------------------------------------------*/
    /* Vertex - Position                                                     */
    /*-----------------------------------------------------------------------*/
    vertex_attribute_descriptions[0].binding=0;
    vertex_attribute_descriptions[0].location= 0;
    vertex_attribute_descriptions[0].offset= 0;
    vertex_attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    /*-----------------------------------------------------------------------*/
    /* Instance - World Transform Matrix                                     */
    /*-----------------------------------------------------------------------*/
    vertex_attribute_descriptions[1].binding= 1;
    vertex_attribute_descriptions[1].location= 1;
    vertex_attribute_descriptions[1].offset= sizeof(GVECTORF) * 0;
    vertex_attribute_descriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vertex_attribute_descriptions[2].binding= 1;
    vertex_attribute_descriptions[2].location= 2;
    vertex_attribute_descriptions[2].offset= sizeof(GVECTORF) * 1;
    vertex_attribute_descriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vertex_attribute_descriptions[3].binding= 1;
    vertex_attribute_descriptions[3].location= 3;
    vertex_attribute_descriptions[3].offset= sizeof(GVECTORF) * 2;
    vertex_attribute_descriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vertex_attribute_descriptions[4].binding= 1;
    vertex_attribute_descriptions[4].location= 4;
    vertex_attribute_descriptions[4].offset= sizeof(GVECTORF) * 3;
    vertex_attribute_descriptions[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    /*-----------------------------------------------------------------------*/
    input_vertex_info.vertexAttributeDescriptionCount = 5;
    input_vertex_info.pVertexAttributeDescriptions = vertex_attribute_descriptions;
    /*=======================================================================*/
    /* Viewport State                                                        */
    /*=======================================================================*/
    VkPipelineViewportStateCreateInfo viewport_create_info;
    ZeroMemory(&viewport_create_info, sizeof(VkPipelineViewportStateCreateInfo));
    viewport_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_create_info.viewportCount = 1;
    viewport_create_info.scissorCount = 1;
    /*=======================================================================*/
    /* Rasterization State                                                   */
    /*=======================================================================*/
    VkPipelineRasterizationStateCreateInfo rasterization_create_info;
    ZeroMemory(&rasterization_create_info, sizeof(VkPipelineRasterizationStateCreateInfo));
    rasterization_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_create_info.polygonMode = VK_POLYGON_MODE_LINE;
    rasterization_create_info.lineWidth = 1.0f;
    rasterization_create_info.cullMode = VK_CULL_MODE_NONE;
    rasterization_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    /*=======================================================================*/
    /* Multisampling State                                                   */
    /*=======================================================================*/
    VkPipelineMultisampleStateCreateInfo multisample_create_info;
    ZeroMemory(&multisample_create_info, sizeof(VkPipelineMultisampleStateCreateInfo));
    multisample_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample_create_info.minSampleShading = 1.0f;
    /*=======================================================================*/
    /* Depth Stencil State                                                   */
    /*=======================================================================*/
    VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info;
    ZeroMemory(&depth_stencil_create_info, sizeof(VkPipelineDepthStencilStateCreateInfo));
    depth_stencil_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_create_info.minDepthBounds = 0.0f;
    depth_stencil_create_info.maxDepthBounds = 1.0f;
    depth_stencil_create_info.depthTestEnable = VK_TRUE;
    depth_stencil_create_info.depthWriteEnable = VK_TRUE;
    depth_stencil_create_info.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    /*=======================================================================*/
    /* Color Blend State                                                     */
    /*=======================================================================*/
    VkPipelineColorBlendAttachmentState color_blend_attachment_states[2] = {};
    color_blend_attachment_states[0].colorWriteMask = 0xF;
    color_blend_attachment_states[0].blendEnable = VK_FALSE;
    color_blend_attachment_states[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
    color_blend_attachment_states[0].dstColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
    color_blend_attachment_states[0].colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_states[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment_states[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    color_blend_attachment_states[0].alphaBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_states[1].colorWriteMask = 0xF;
    color_blend_attachment_states[1].blendEnable = VK_FALSE;
    color_blend_attachment_states[1].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
    color_blend_attachment_states[1].dstColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
    color_blend_attachment_states[1].colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_states[1].srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment_states[1].dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    color_blend_attachment_states[1].alphaBlendOp = VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo color_blend_create_info;
    ZeroMemory(&color_blend_create_info, sizeof(VkPipelineColorBlendStateCreateInfo));
    color_blend_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_create_info.attachmentCount = 2;
    color_blend_create_info.pAttachments = color_blend_attachment_states;
    /*=======================================================================*/
    /* Dynamic State                                                         */
    /*=======================================================================*/
    VkDynamicState dynamic_state[2] = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamic_create_info = {};
    dynamic_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_create_info.dynamicStateCount = 2;
    dynamic_create_info.pDynamicStates = dynamic_state;
    /*=======================================================================*/
    /*                                                                       */
    /*=======================================================================*/
    VkGraphicsPipelineCreateInfo create_info;
    ZeroMemory(&create_info, sizeof(VkGraphicsPipelineCreateInfo));
    create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create_info.stageCount = 2;
    create_info.pStages = stage_create_info;
    create_info.pInputAssemblyState = &assembly_create_info;
    create_info.pVertexInputState = &input_vertex_info;
    create_info.pViewportState = &viewport_create_info;
    create_info.pRasterizationState = &rasterization_create_info;
    create_info.pMultisampleState = &multisample_create_info;
    create_info.pDepthStencilState = &depth_stencil_create_info;
    create_info.pColorBlendState = &color_blend_create_info;
    create_info.pDynamicState = &dynamic_create_info;
    create_info.layout = shadowMapPipelineLayout;
    create_info.renderPass = staticMeshRenderPass;
    vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &create_info, NULL, &debugColliderPipeline);
}
#endif
void RenderSystem::CreateUISDFPipeline(VkDevice _device)
{
    /*=======================================================================*/
    /* Shader Stages                                                         */
    /*=======================================================================*/
    VkPipelineShaderStageCreateInfo stage_create_info[2];
    ZeroMemory(stage_create_info, sizeof(VkPipelineShaderStageCreateInfo) * 2);
    /*-----------------------------------------------------------------------*/
    /* Vertex Shader                                                         */
    /*-----------------------------------------------------------------------*/
    stage_create_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_create_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage_create_info[0].pName = "main";
    stage_create_info[0].module = uiSDFVertexShader;
    /*-----------------------------------------------------------------------*/
    /* Fragment Shader                                                       */
    /*-----------------------------------------------------------------------*/
    stage_create_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_create_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stage_create_info[1].pName = "main";
    stage_create_info[1].module = uiSDFPixelShader;
    /*=======================================================================*/
    /* Input Assembly State                                                  */
    /*=======================================================================*/
    VkPipelineInputAssemblyStateCreateInfo assembly_create_info;
    ZeroMemory(&assembly_create_info, sizeof(VkPipelineInputAssemblyStateCreateInfo));
    assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    assembly_create_info.primitiveRestartEnable = false;
    /*=======================================================================*/
    /* Vertex Input State                                                    */
    /*=======================================================================*/
    VkPipelineVertexInputStateCreateInfo input_vertex_info = {};
    input_vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    /*-----------------------------------------------------------------------*/
    /* Vertex Bindings                                                       */
    /*-----------------------------------------------------------------------*/
    VkVertexInputBindingDescription vertex_binding_descriptions[1];
    ZeroMemory(vertex_binding_descriptions, sizeof(VkVertexInputBindingDescription));
    /*-----------------------------------------------------------------------*/
    /* Per Vertex Data                                                       */
    /*-----------------------------------------------------------------------*/
    vertex_binding_descriptions[0].binding = 0;
    vertex_binding_descriptions[0].stride = sizeof(FontVertex);
    vertex_binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    /*-----------------------------------------------------------------------*/
    input_vertex_info.vertexBindingDescriptionCount = 1;
    input_vertex_info.pVertexBindingDescriptions = vertex_binding_descriptions;
    /*-----------------------------------------------------------------------*/
    /* Vertex Attributes                                                     */
    /*-----------------------------------------------------------------------*/
    VkVertexInputAttributeDescription vertex_attribute_descriptions[2];
    ZeroMemory(vertex_attribute_descriptions, sizeof(VkVertexInputAttributeDescription) * 2);
    /*-----------------------------------------------------------------------*/
    /* Vertex - Position                                                     */
    /*-----------------------------------------------------------------------*/
    vertex_attribute_descriptions[0].binding=0;
    vertex_attribute_descriptions[0].location= 0;
    vertex_attribute_descriptions[0].offset= sizeof(GW::MATH2D::GVECTOR2F) * 0;
    vertex_attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    /*-----------------------------------------------------------------------*/
    /* Vertex - Texture Coordinates                                          */
    /*-----------------------------------------------------------------------*/
    vertex_attribute_descriptions[1].binding=0;
    vertex_attribute_descriptions[1].location= 1;
    vertex_attribute_descriptions[1].offset= sizeof(GW::MATH2D::GVECTOR2F) * 1;
    vertex_attribute_descriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    /*-----------------------------------------------------------------------*/
    input_vertex_info.vertexAttributeDescriptionCount = 2;
    input_vertex_info.pVertexAttributeDescriptions = vertex_attribute_descriptions;
    /*=======================================================================*/
    /* Viewport State                                                        */
    /*=======================================================================*/
    VkPipelineViewportStateCreateInfo viewport_create_info;
    ZeroMemory(&viewport_create_info, sizeof(VkPipelineViewportStateCreateInfo));
    viewport_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_create_info.viewportCount = 1;
    viewport_create_info.scissorCount = 1;
    /*=======================================================================*/
    /* Rasterization State                                                   */
    /*=======================================================================*/
    VkPipelineRasterizationStateCreateInfo rasterization_create_info;
    ZeroMemory(&rasterization_create_info, sizeof(VkPipelineRasterizationStateCreateInfo));
    rasterization_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_create_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_create_info.lineWidth = 1.0f;
    rasterization_create_info.cullMode = VK_CULL_MODE_NONE;
    rasterization_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    /*=======================================================================*/
    /* Multisampling State                                                   */
    /*=======================================================================*/
    VkPipelineMultisampleStateCreateInfo multisample_create_info;
    ZeroMemory(&multisample_create_info, sizeof(VkPipelineMultisampleStateCreateInfo));
    multisample_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample_create_info.minSampleShading = 1.0f;
    /*=======================================================================*/
    /* Depth Stencil State                                                   */
    /*=======================================================================*/
    VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info;
    ZeroMemory(&depth_stencil_create_info, sizeof(VkPipelineDepthStencilStateCreateInfo));
    depth_stencil_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_create_info.minDepthBounds = 0.0f;
    depth_stencil_create_info.maxDepthBounds = 1.0f;
    /*=======================================================================*/
    /* Color Blend State                                                     */
    /*=======================================================================*/
    VkPipelineColorBlendAttachmentState color_blend_attachment_state = {};
    color_blend_attachment_state.colorWriteMask = 0xF;
    color_blend_attachment_state.blendEnable = VK_TRUE;
    color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo color_blend_create_info;
    ZeroMemory(&color_blend_create_info, sizeof(VkPipelineColorBlendStateCreateInfo));
    color_blend_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_create_info.attachmentCount = 1;
    color_blend_create_info.pAttachments = &color_blend_attachment_state;
    /*=======================================================================*/
    /* Dynamic State                                                         */
    /*=======================================================================*/
    VkDynamicState dynamic_state[2] = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamic_create_info = {};
    dynamic_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_create_info.dynamicStateCount = 2;
    dynamic_create_info.pDynamicStates = dynamic_state;
    /*=======================================================================*/
    VkGraphicsPipelineCreateInfo create_info;
    ZeroMemory(&create_info, sizeof(VkGraphicsPipelineCreateInfo));
    create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create_info.stageCount = 2;
    create_info.pStages = stage_create_info;
    create_info.pInputAssemblyState = &assembly_create_info;
    create_info.pVertexInputState = &input_vertex_info;
    create_info.pViewportState = &viewport_create_info;
    create_info.pRasterizationState = &rasterization_create_info;
    create_info.pMultisampleState = &multisample_create_info;
    create_info.pDepthStencilState = &depth_stencil_create_info;
    create_info.pColorBlendState = &color_blend_create_info;
    create_info.pDynamicState = &dynamic_create_info;
    create_info.layout = uiSDFPipelineLayout;
    create_info.renderPass = uiRenderPass;
    vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &create_info, NULL, &uiSDFPipeline);
}

void RenderSystem::CreateUIBlitPipeline(VkDevice _device)
{
    /*=======================================================================*/
    /* Shader Stages                                                         */
    /*=======================================================================*/
    VkPipelineShaderStageCreateInfo stage_create_info[2];
    ZeroMemory(stage_create_info, sizeof(VkPipelineShaderStageCreateInfo) * 2);
    /*-----------------------------------------------------------------------*/
    /* Vertex Shader                                                         */
    /*-----------------------------------------------------------------------*/
    stage_create_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_create_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage_create_info[0].pName = "main";
    stage_create_info[0].module = uiBlitVertexShader;
    /*-----------------------------------------------------------------------*/
    /* Fragment Shader                                                       */
    /*-----------------------------------------------------------------------*/
    stage_create_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_create_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stage_create_info[1].pName = "main";
    stage_create_info[1].module = uiBlitPixelShader;
    /*=======================================================================*/
    /* Input Assembly State                                                  */
    /*=======================================================================*/
    VkPipelineInputAssemblyStateCreateInfo assembly_create_info;
    ZeroMemory(&assembly_create_info, sizeof(VkPipelineInputAssemblyStateCreateInfo));
    assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    assembly_create_info.primitiveRestartEnable = false;
    /*=======================================================================*/
    /* Vertex Input State                                                    */
    /*=======================================================================*/
    VkPipelineVertexInputStateCreateInfo input_vertex_info = {};
    input_vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    /*-----------------------------------------------------------------------*/
    /* Vertex Bindings                                                       */
    /*-----------------------------------------------------------------------*/
    VkVertexInputBindingDescription vertex_binding_descriptions[1];
    ZeroMemory(vertex_binding_descriptions, sizeof(VkVertexInputBindingDescription));
    /*-----------------------------------------------------------------------*/
    /* Per Vertex Data                                                       */
    /*-----------------------------------------------------------------------*/
    vertex_binding_descriptions[0].binding = 0;
    vertex_binding_descriptions[0].stride = sizeof(SpriteInstance);
    vertex_binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    /*-----------------------------------------------------------------------*/
    input_vertex_info.vertexBindingDescriptionCount = 1;
    input_vertex_info.pVertexBindingDescriptions = vertex_binding_descriptions;
    /*-----------------------------------------------------------------------*/
    /* Vertex Attributes                                                     */
    /*-----------------------------------------------------------------------*/
    VkVertexInputAttributeDescription vertex_attribute_descriptions[2];
    ZeroMemory(vertex_attribute_descriptions, sizeof(VkVertexInputAttributeDescription) * 2);
    /*-----------------------------------------------------------------------*/
    /* Instance - Source Rect (src.x, src.y, src.width, src.height)          */
    /*-----------------------------------------------------------------------*/
    vertex_attribute_descriptions[0].binding=0;
    vertex_attribute_descriptions[0].location= 0;
    vertex_attribute_descriptions[0].offset= sizeof(GW::MATH::GVECTORF) * 0;
    vertex_attribute_descriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    /*-----------------------------------------------------------------------*/
    /* Instance - Destination Rect (dst.x, dst.y, dst.width, dst.height)     */
    /*-----------------------------------------------------------------------*/
    vertex_attribute_descriptions[1].binding=0;
    vertex_attribute_descriptions[1].location= 1;
    vertex_attribute_descriptions[1].offset= sizeof(GW::MATH::GVECTORF) * 1;
    vertex_attribute_descriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    /*-----------------------------------------------------------------------*/
    input_vertex_info.vertexAttributeDescriptionCount = 2;
    input_vertex_info.pVertexAttributeDescriptions = vertex_attribute_descriptions;
    /*=======================================================================*/
    /* Viewport State                                                        */
    /*=======================================================================*/
    VkPipelineViewportStateCreateInfo viewport_create_info;
    ZeroMemory(&viewport_create_info, sizeof(VkPipelineViewportStateCreateInfo));
    viewport_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_create_info.viewportCount = 1;
    viewport_create_info.scissorCount = 1;
    /*=======================================================================*/
    /* Rasterization State                                                   */
    /*=======================================================================*/
    VkPipelineRasterizationStateCreateInfo rasterization_create_info;
    ZeroMemory(&rasterization_create_info, sizeof(VkPipelineRasterizationStateCreateInfo));
    rasterization_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_create_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_create_info.lineWidth = 1.0f;
    rasterization_create_info.cullMode = VK_CULL_MODE_NONE;
    rasterization_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    /*=======================================================================*/
    /* Multisampling State                                                   */
    /*=======================================================================*/
    VkPipelineMultisampleStateCreateInfo multisample_create_info;
    ZeroMemory(&multisample_create_info, sizeof(VkPipelineMultisampleStateCreateInfo));
    multisample_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample_create_info.minSampleShading = 1.0f;
    /*=======================================================================*/
    /* Depth Stencil State                                                   */
    /*=======================================================================*/
    VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info;
    ZeroMemory(&depth_stencil_create_info, sizeof(VkPipelineDepthStencilStateCreateInfo));
    depth_stencil_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_create_info.minDepthBounds = 0.0f;
    depth_stencil_create_info.maxDepthBounds = 1.0f;
    /*=======================================================================*/
    /* Color Blend State                                                     */
    /*=======================================================================*/
    VkPipelineColorBlendAttachmentState color_blend_attachment_state = {};
    color_blend_attachment_state.colorWriteMask = 0xF;
    color_blend_attachment_state.blendEnable = VK_TRUE;
    color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo color_blend_create_info;
    ZeroMemory(&color_blend_create_info, sizeof(VkPipelineColorBlendStateCreateInfo));
    color_blend_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_create_info.attachmentCount = 1;
    color_blend_create_info.pAttachments = &color_blend_attachment_state;
    /*=======================================================================*/
    /* Dynamic State                                                         */
    /*=======================================================================*/
    VkDynamicState dynamic_state[2] = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamic_create_info = {};
    dynamic_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_create_info.dynamicStateCount = 2;
    dynamic_create_info.pDynamicStates = dynamic_state;
    /*=======================================================================*/
    VkGraphicsPipelineCreateInfo create_info;
    ZeroMemory(&create_info, sizeof(VkGraphicsPipelineCreateInfo));
    create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create_info.stageCount = 2;
    create_info.pStages = stage_create_info;
    create_info.pInputAssemblyState = &assembly_create_info;
    create_info.pVertexInputState = &input_vertex_info;
    create_info.pViewportState = &viewport_create_info;
    create_info.pRasterizationState = &rasterization_create_info;
    create_info.pMultisampleState = &multisample_create_info;
    create_info.pDepthStencilState = &depth_stencil_create_info;
    create_info.pColorBlendState = &color_blend_create_info;
    create_info.pDynamicState = &dynamic_create_info;
    create_info.layout = uiBlitPipelineLayout;
    create_info.renderPass = uiRenderPass;
    vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &create_info, NULL, &uiBlitPipeline);
}

void RenderSystem::CreatePresentPipeline(VkDevice _device)
{
    /*=======================================================================*/
    /* Shader Stages                                                         */
    /*=======================================================================*/
    VkPipelineShaderStageCreateInfo stage_create_info[2];
    ZeroMemory(stage_create_info, sizeof(VkPipelineShaderStageCreateInfo) * 2);
    /*-----------------------------------------------------------------------*/
    /* Vertex Shader                                                         */
    /*-----------------------------------------------------------------------*/
    stage_create_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_create_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage_create_info[0].pName = "main";
    stage_create_info[0].module = presentVertexShader;
    /*-----------------------------------------------------------------------*/
    /* Fragment Shader                                                       */
    /*-----------------------------------------------------------------------*/
    stage_create_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_create_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stage_create_info[1].pName = "main";
    stage_create_info[1].module = presentPixelShader;
    /*=======================================================================*/
    /* Input Assembly State                                                  */
    /*=======================================================================*/
    VkPipelineInputAssemblyStateCreateInfo assembly_create_info;
    ZeroMemory(&assembly_create_info, sizeof(VkPipelineInputAssemblyStateCreateInfo));
    assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    assembly_create_info.primitiveRestartEnable = false;
    /*=======================================================================*/
    /* Vertex Input State                                                    */
    /*=======================================================================*/
    VkPipelineVertexInputStateCreateInfo input_vertex_info = {};
    input_vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    /*=======================================================================*/
    /* Viewport State                                                        */
    /*=======================================================================*/
    VkPipelineViewportStateCreateInfo viewport_create_info;
    ZeroMemory(&viewport_create_info, sizeof(VkPipelineViewportStateCreateInfo));
    viewport_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_create_info.viewportCount = 1;
    viewport_create_info.scissorCount = 1;
    /*=======================================================================*/
    /* Rasterization State                                                   */
    /*=======================================================================*/
    VkPipelineRasterizationStateCreateInfo rasterization_create_info;
    ZeroMemory(&rasterization_create_info, sizeof(VkPipelineRasterizationStateCreateInfo));
    rasterization_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_create_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_create_info.lineWidth = 1.0f;
    rasterization_create_info.cullMode = VK_CULL_MODE_NONE;
    rasterization_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    /*=======================================================================*/
    /* Multisampling State                                                   */
    /*=======================================================================*/
    VkPipelineMultisampleStateCreateInfo multisample_create_info;
    ZeroMemory(&multisample_create_info, sizeof(VkPipelineMultisampleStateCreateInfo));
    multisample_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample_create_info.minSampleShading = 1.0f;
    /*=======================================================================*/
    /* Depth Stencil State                                                   */
    /*=======================================================================*/
    VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info;
    ZeroMemory(&depth_stencil_create_info, sizeof(VkPipelineDepthStencilStateCreateInfo));
    depth_stencil_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_create_info.minDepthBounds = 0.0f;
    depth_stencil_create_info.maxDepthBounds = 1.0f;
    /*=======================================================================*/
    /* Color Blend State                                                     */
    /*=======================================================================*/
    VkPipelineColorBlendAttachmentState color_blend_attachment_state = {};
    color_blend_attachment_state.colorWriteMask = 0xF;
    color_blend_attachment_state.blendEnable = VK_FALSE;
    color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
    color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
    color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo color_blend_create_info;
    ZeroMemory(&color_blend_create_info, sizeof(VkPipelineColorBlendStateCreateInfo));
    color_blend_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_create_info.attachmentCount = 1;
    color_blend_create_info.pAttachments = &color_blend_attachment_state;
    /*=======================================================================*/
    /* Dynamic State                                                         */
    /*=======================================================================*/
    VkDynamicState dynamic_state[2] = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamic_create_info = {};
    dynamic_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_create_info.dynamicStateCount = 2;
    dynamic_create_info.pDynamicStates = dynamic_state;
    /*=======================================================================*/
    /*                                                                       */
    /*=======================================================================*/
    VkGraphicsPipelineCreateInfo create_info;
    ZeroMemory(&create_info, sizeof(VkGraphicsPipelineCreateInfo));
    create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create_info.stageCount = 2;
    create_info.pStages = stage_create_info;
    create_info.pInputAssemblyState = &assembly_create_info;
    create_info.pVertexInputState = &input_vertex_info;
    create_info.pViewportState = &viewport_create_info;
    create_info.pRasterizationState = &rasterization_create_info;
    create_info.pMultisampleState = &multisample_create_info;
    create_info.pDepthStencilState = &depth_stencil_create_info;
    create_info.pColorBlendState = &color_blend_create_info;
    create_info.pDynamicState = &dynamic_create_info;
    create_info.layout = presentPipelineLayout;
    create_info.renderPass = presentRenderPass;
    vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &create_info, NULL, &presentPipeline);
}

void RenderSystem::DestroyPipelines(VkDevice _device)
{
    vkDestroyPipeline(_device, shadowMapPipeline, NULL);
    vkDestroyPipeline(_device, staticMeshPipeline, NULL);
    vkDestroyPipeline(_device, blurPipeline, NULL);
    vkDestroyPipeline(_device, skyBoxPipeline, NULL);
#ifdef DEV_BUILD
    vkDestroyPipeline(_device, debugColliderPipeline, NULL);
#endif
    vkDestroyPipeline(_device, uiSDFPipeline, NULL);
    vkDestroyPipeline(_device, uiBlitPipeline, NULL);
    vkDestroyPipeline(_device, presentPipeline, NULL);
}

void RenderSystem::CreatePersistentResources(VkPhysicalDevice _physicalDevice, VkDevice _device)
{
    GvkHelper::create_buffer(_physicalDevice, _device,
       8 * 1024 * 1024,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &meshVertexDataBuffer,
        &meshVertexDataMemory);
    GvkHelper::create_buffer(_physicalDevice, _device,
        4 * 1024 * 1024,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &meshIndexDataBuffer,
        &meshIndexDataMemory);
#ifdef DEV_BUILD
    GvkHelper::create_buffer(_physicalDevice, _device,
        1024 * 1024,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &debugColliderVertexDataBuffer,
        &debugColliderVertexDataMemory);
    GvkHelper::create_buffer(_physicalDevice, _device,
        1024 * 1024,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &debugColliderIndexDataBuffer,
        &debugColliderIndexDataMemory);
#endif
    GvkHelper::create_buffer(_physicalDevice, _device,
        32 * 1024 * 1024,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        &stagingBuffer,
        &stagingMemory);
    vkMapMemory(_device, stagingMemory, 0, 32 * 1024 * 1024, 0, &stagingMappedMemory);
    /*-----------------------------------------------------------------------*/
    VkDescriptorPoolSize pool_sizes[1];
    pool_sizes[0].descriptorCount = 1;
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    VkDescriptorPoolCreateInfo create_info;
    ZeroMemory(&create_info, sizeof(VkDescriptorPoolCreateInfo));
    create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    create_info.poolSizeCount= 1;
    create_info.pPoolSizes = pool_sizes;
    create_info.maxSets= MAX_MESH_COUNT;
    vkCreateDescriptorPool(_device, &create_info, NULL, &materialDescriptorPool);
    create_info.maxSets= 4;
    vkCreateDescriptorPool(_device, &create_info, NULL, &cubeMapDescriptorPool);
    create_info.maxSets= 32;
    vkCreateDescriptorPool(_device, &create_info, NULL, &uiSDFDescriptorPool);
    vkCreateDescriptorPool(_device, &create_info, NULL, &uiBlitDescriptorPool);
}

void RenderSystem::DestroyPersistentResources(VkDevice _device)
{
    for(uint32_t i = 0; i < cubeMapDescriptorSets.size(); ++i)
    {
        vkDestroyImageView(_device, cubeMapTextureSRVs[i], NULL);
        vkDestroyImage(_device, cubeMapTextures[i], NULL);
        vkFreeMemory(_device, cubeMapTextureMemBlocks[i], NULL);
    }
    vkDestroyDescriptorPool(_device, cubeMapDescriptorPool, NULL);
    for(uint32_t i = 0; i < meshVector.size(); ++i)
    {
        vkDestroyImageView(_device, materialTextureSRVs[i], NULL);
        vkDestroyImage(_device, materialTextures[i], NULL);
        vkFreeMemory(_device, materialTextureMemBlocks[i], NULL);
    }
    vkDestroyDescriptorPool(_device, materialDescriptorPool, NULL);
    for(uint32_t i = 0; i < fontLayouts.size(); ++i)
    {
        vkDestroyImageView(_device, fontAtlasTextureSRVs[i], NULL);
        vkDestroyImage(_device, fontAtlasTextures[i], NULL);
        vkFreeMemory(_device, fontAtlasTextureMemBlocks[i], NULL);
    }
    vkDestroyDescriptorPool(_device, uiSDFDescriptorPool, NULL);
    for(uint32_t i = 0; i < spriteAtlasTextures.size(); ++i)
    {
        vkDestroyImageView(_device, spriteAtlasTextureSRVs[i], NULL);
        vkDestroyImage(_device, spriteAtlasTextures[i], NULL);
        vkFreeMemory(_device, spriteAtlasTextureMemBlocks[i], NULL);
    }
    vkDestroyDescriptorPool(_device, uiBlitDescriptorPool, NULL);
    vkDestroyBuffer(_device, meshVertexDataBuffer, NULL);
    vkDestroyBuffer(_device, meshIndexDataBuffer, NULL);
    vkFreeMemory(_device, meshVertexDataMemory, NULL);
    vkFreeMemory(_device, meshIndexDataMemory, NULL);
#ifdef DEV_BUILD
    vkDestroyBuffer(_device, debugColliderVertexDataBuffer, NULL);
    vkDestroyBuffer(_device, debugColliderIndexDataBuffer, NULL);
    vkFreeMemory(_device, debugColliderVertexDataMemory, NULL);
    vkFreeMemory(_device, debugColliderIndexDataMemory, NULL);
#endif
    std::vector<Mesh>().swap(meshVector);

    vkUnmapMemory(_device, stagingMemory);
    vkDestroyBuffer(_device, stagingBuffer, NULL);
    vkFreeMemory(_device, stagingMemory, NULL);
}

void RenderSystem::CreatePerFrameResources(VkPhysicalDevice _physicalDevice, VkDevice _device, uint32_t bufferCount)
{
    /*=======================================================================*/
    /* Descriptors                                                           */
    /*=======================================================================*/
    /* Descriptor Pool                                                       */
    /*-----------------------------------------------------------------------*/
    VkDescriptorImageInfo imageInfos[3];
    ZeroMemory(imageInfos, sizeof(VkDescriptorImageInfo) * 3);
    VkWriteDescriptorSet descriptorWrites[3];
    ZeroMemory(descriptorWrites, sizeof(VkWriteDescriptorSet) * 3);
    VkDescriptorPoolSize pool_sizes[2];
    pool_sizes[0].descriptorCount = bufferCount;
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    pool_sizes[1].descriptorCount = bufferCount;
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    VkDescriptorPoolCreateInfo create_info;
    ZeroMemory(&create_info, sizeof(VkDescriptorPoolCreateInfo));
    create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    create_info.poolSizeCount= 2;
    create_info.pPoolSizes = pool_sizes;
    create_info.maxSets= bufferCount;
    vkCreateDescriptorPool(_device, &create_info, NULL, &perFrameDescriptorPool);
    vkCreateDescriptorPool(_device, &create_info, NULL, &shadowMapDescriptorPool);
    create_info.maxSets= bufferCount * 2;
    vkCreateDescriptorPool(_device, &create_info, NULL, &blurDescriptorPool);
    /*-----------------------------------------------------------------------*/
    /* Descriptor Sets                                                       */
    /*-----------------------------------------------------------------------*/
    shadowMapDescriptorSets.resize(bufferCount);
    blurPingDescriptorSets.resize(bufferCount);
    blurPongDescriptorSets.resize(bufferCount);
    perFrameDescriptorSets.resize(bufferCount);
    VkDescriptorSetAllocateInfo descriptor_alloc_info;
    ZeroMemory(&descriptor_alloc_info, sizeof(VkDescriptorSetAllocateInfo));
    descriptor_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    /*=======================================================================*/
    uiSDFVertexDataBuffers.resize(bufferCount);
    uiSDFVertexDataMemoryBlocks.resize(bufferCount);
    uiSDFIndexDataBuffers.resize(bufferCount);
    uiSDFIndexDataMemoryBlocks.resize(bufferCount);
    uiBlitInstanceDataBuffers.resize(bufferCount);
    uiBlitInstanceDataMemoryBlocks.resize(bufferCount);
    instanceVertexDataBuffers.resize(bufferCount);
    instanceVertexDataMemoryBlocks.resize(bufferCount);

    gameObjectRTs.resize(bufferCount);
    gameObjectRTMemBlocks.resize(bufferCount);
    gameObjectRTVs.resize(bufferCount);
    gameObjectBloomRTs.resize(bufferCount);
    gameObjectBloomRTMemBlocks.resize(bufferCount);
    gameObjectBloomRTVs.resize(bufferCount);
    gameObjectDTs.resize(bufferCount);
    gameObjectDTMemBlocks.resize(bufferCount);
    gameObjectDTVs.resize(bufferCount);
    gameObjectFramebuffers.resize(bufferCount);

    shadowMapDTs.resize(bufferCount);
    shadowMapDTMemBlocks.resize(bufferCount);
    shadowMapDTVs.resize(bufferCount);
    shadowMapFramebuffers.resize(bufferCount);

    blurRTs.resize(bufferCount);
    blurRTMemBlocks.resize(bufferCount);
    blurRTVs.resize(bufferCount);
    blurPingFramebuffers.resize(bufferCount);
    blurPongFramebuffers.resize(bufferCount);

    uiRTs.resize(bufferCount);
    uiRTMemBlocks.resize(bufferCount);
    uiRTVs.resize(bufferCount);
    uiFramebuffers.resize(bufferCount);

    VkImageView framebufferAttachments[3] = {};
    VkFramebufferCreateInfo framebuffer_create_info;
    ZeroMemory(&framebuffer_create_info, sizeof(VkFramebufferCreateInfo));
    framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_create_info.pAttachments = framebufferAttachments;
    framebuffer_create_info.layers = 1;
    for(uint32_t i = 0;i < bufferCount; ++i)
    {
        /*-------------------------------------------------------------------*/
        /* Per-Frame UI Vertex and Instance Buffers                          */
        /*-------------------------------------------------------------------*/
        GvkHelper::create_buffer(_physicalDevice, _device,
            1024 * 1024,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &uiSDFVertexDataBuffers[i],
            &uiSDFVertexDataMemoryBlocks[i]);
        GvkHelper::create_buffer(_physicalDevice, _device,
            1024 * 1024,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &uiSDFIndexDataBuffers[i],
            &uiSDFIndexDataMemoryBlocks[i]);
        GvkHelper::create_buffer(_physicalDevice, _device,
            1024 * 1024,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &uiBlitInstanceDataBuffers[i],
            &uiBlitInstanceDataMemoryBlocks[i]);
        /*-------------------------------------------------------------------*/
        /* Per-Frame GameObject Instance Buffer                              */
        /*-------------------------------------------------------------------*/
        GvkHelper::create_buffer(_physicalDevice, _device,
            1024 * 1024,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &instanceVertexDataBuffers[i],
            &instanceVertexDataMemoryBlocks[i]);
        /*-------------------------------------------------------------------*/
        /* Per-Frame Shadow Map Draw Depth Target                            */
        /*-------------------------------------------------------------------*/
        GvkHelper::create_image_set(_physicalDevice, _device, commandPool,
            { 1024, 1024, 1 }, graphicsQueue,
            1, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_D32_SFLOAT, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            NULL, &shadowMapDTs[i], &shadowMapDTVs[i], &shadowMapDTMemBlocks[i]);
        /*-------------------------------------------------------------------*/
        /* Shadow Map Framebuffer for this Buffer Index                      */
        /*-------------------------------------------------------------------*/
        framebuffer_create_info.attachmentCount = 1;
        framebufferAttachments[0] = shadowMapDTVs[i];
        framebuffer_create_info.width = 1024;
        framebuffer_create_info.height = 1024;
        framebuffer_create_info.renderPass = shadowMapRenderPass;
        vkCreateFramebuffer(_device, &framebuffer_create_info, NULL, &shadowMapFramebuffers[i]);
        /*-------------------------------------------------------------------*/
        /* Shadow Map Pass Descriptor Set for this Buffer Index              */
        /*-------------------------------------------------------------------*/
        descriptor_alloc_info.descriptorSetCount= 1;
        descriptor_alloc_info.descriptorPool = shadowMapDescriptorPool;
        descriptor_alloc_info.pSetLayouts = &uiBlitDescriptorSetLayout;
        vkAllocateDescriptorSets(_device, &descriptor_alloc_info, &shadowMapDescriptorSets[i]);
        imageInfos[0].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        imageInfos[0].imageView = shadowMapDTVs[i];
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = shadowMapDescriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorWrites[0].pImageInfo = &imageInfos[0];
        vkUpdateDescriptorSets(_device, 1, descriptorWrites, 0, NULL);
        /*-------------------------------------------------------------------*/
        /* Per-Frame GameObject Draw Render Target                           */
        /*-------------------------------------------------------------------*/
        GvkHelper::create_image_set(_physicalDevice, _device, commandPool,
            { swapchainExtent.width, swapchainExtent.height, 1 }, graphicsQueue,
            1, VK_SAMPLE_COUNT_1_BIT, swapchainFormat.format, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            NULL, &gameObjectRTs[i], &gameObjectRTVs[i], &gameObjectRTMemBlocks[i]);
        /*-------------------------------------------------------------------*/
        /* Per-Frame GameObject Draw Bloom Render Target                     */
        /*-------------------------------------------------------------------*/
        GvkHelper::create_image_set(_physicalDevice, _device, commandPool,
            { swapchainExtent.width, swapchainExtent.height, 1 }, graphicsQueue,
            1, VK_SAMPLE_COUNT_1_BIT, swapchainFormat.format, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            NULL, &gameObjectBloomRTs[i], &gameObjectBloomRTVs[i], &gameObjectBloomRTMemBlocks[i]);
        /*-------------------------------------------------------------------*/
        /* Per-Frame GameObject Draw Depth Target                            */
        /*-------------------------------------------------------------------*/
        GvkHelper::create_image_set(_physicalDevice, _device, commandPool,
            { swapchainExtent.width, swapchainExtent.height, 1 }, graphicsQueue,
            1, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_D16_UNORM, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            NULL, &gameObjectDTs[i], &gameObjectDTVs[i], &gameObjectDTMemBlocks[i]);
        /*-------------------------------------------------------------------*/
        /* Game Object Framebuffer for this Buffer Index                     */
        /*-------------------------------------------------------------------*/
        framebuffer_create_info.attachmentCount = 3;
        framebufferAttachments[0] = gameObjectRTVs[i];
        framebufferAttachments[1] = gameObjectBloomRTVs[i];
        framebufferAttachments[2] = gameObjectDTVs[i];
        framebuffer_create_info.width = swapchainExtent.width;
        framebuffer_create_info.height = swapchainExtent.height;
        framebuffer_create_info.renderPass = staticMeshRenderPass;
        vkCreateFramebuffer(_device, &framebuffer_create_info, NULL, &gameObjectFramebuffers[i]);
        /*-------------------------------------------------------------------*/
        /* Per-Frame Blur Render Target                                      */
        /*-------------------------------------------------------------------*/
        GvkHelper::create_image_set(_physicalDevice, _device, commandPool,
            { swapchainExtent.width, swapchainExtent.height, 1 }, graphicsQueue,
            1, VK_SAMPLE_COUNT_1_BIT, swapchainFormat.format, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            NULL, &blurRTs[i], &blurRTVs[i], &blurRTMemBlocks[i]);
        /*-------------------------------------------------------------------*/
        /* Blur Ping Framebuffer for this Buffer Index                       */
        /*-------------------------------------------------------------------*/
        framebuffer_create_info.attachmentCount = 1;
        framebufferAttachments[0] = blurRTVs[i];
        framebuffer_create_info.width = swapchainExtent.width;
        framebuffer_create_info.height = swapchainExtent.height;
        framebuffer_create_info.renderPass = blurRenderPass;
        vkCreateFramebuffer(_device, &framebuffer_create_info, NULL, &blurPingFramebuffers[i]);
        /*-------------------------------------------------------------------*/
        /* Blur Pong Framebuffer for this Buffer Index                       */
        /*-------------------------------------------------------------------*/
        framebuffer_create_info.attachmentCount = 1;
        framebufferAttachments[0] = gameObjectBloomRTVs[i];
        framebuffer_create_info.width = swapchainExtent.width;
        framebuffer_create_info.height = swapchainExtent.height;
        framebuffer_create_info.renderPass = blurRenderPass;
        vkCreateFramebuffer(_device, &framebuffer_create_info, NULL, &blurPongFramebuffers[i]);
        /*-------------------------------------------------------------------*/
        /* Blur Descriptor Set for this Buffer Index                 */
        /*-------------------------------------------------------------------*/
        descriptor_alloc_info.descriptorSetCount= 1;
        descriptor_alloc_info.descriptorPool = blurDescriptorPool;
        descriptor_alloc_info.pSetLayouts = &uiBlitDescriptorSetLayout;
        vkAllocateDescriptorSets(_device, &descriptor_alloc_info, &blurPingDescriptorSets[i]);
        vkAllocateDescriptorSets(_device, &descriptor_alloc_info, &blurPongDescriptorSets[i]);
        imageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[0].imageView = gameObjectBloomRTVs[i];
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = blurPingDescriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorWrites[0].pImageInfo = &imageInfos[0];
        imageInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[1].imageView = blurRTVs[i];
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = blurPongDescriptorSets[i];
        descriptorWrites[1].dstBinding = 0;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorWrites[1].pImageInfo = &imageInfos[1];
        vkUpdateDescriptorSets(_device, 2, descriptorWrites, 0, NULL);
        /*-------------------------------------------------------------------*/
        /* Per-Frame UI Draw Render Target                                   */
        /*-------------------------------------------------------------------*/
        GvkHelper::create_image_set(_physicalDevice, _device, commandPool,
            { swapchainExtent.width, swapchainExtent.height, 1 }, graphicsQueue,
            1, VK_SAMPLE_COUNT_1_BIT, swapchainFormat.format, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            NULL, &uiRTs[i], &uiRTVs[i], &uiRTMemBlocks[i]);
        /*-------------------------------------------------------------------*/
        /* UI Framebuffer for this Buffer Index                              */
        /*-------------------------------------------------------------------*/
        framebuffer_create_info.attachmentCount = 1;
        framebufferAttachments[0] = uiRTVs[i];
        framebuffer_create_info.width = swapchainExtent.width;
        framebuffer_create_info.height = swapchainExtent.height;
        framebuffer_create_info.renderPass = uiRenderPass;
        vkCreateFramebuffer(_device, &framebuffer_create_info, NULL, &uiFramebuffers[i]);
        /*-------------------------------------------------------------------*/
        /* Present Pass Descriptor Set for this Buffer Index                 */
        /*-------------------------------------------------------------------*/
        descriptor_alloc_info.descriptorSetCount= 1;
        descriptor_alloc_info.descriptorPool = perFrameDescriptorPool;
        descriptor_alloc_info.pSetLayouts = &presentDescriptorSetLayout;
        vkAllocateDescriptorSets(_device, &descriptor_alloc_info, &perFrameDescriptorSets[i]);
        imageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[0].imageView = gameObjectRTVs[i];
        imageInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[1].imageView = blurRTVs[i];
        imageInfos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[2].imageView = uiRTVs[i];
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = perFrameDescriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorWrites[0].pImageInfo = &imageInfos[0];
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = perFrameDescriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorWrites[1].pImageInfo = &imageInfos[1];
        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = perFrameDescriptorSets[i];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorWrites[2].pImageInfo = &imageInfos[2];
        vkUpdateDescriptorSets(_device, 3, descriptorWrites, 0, NULL);
    }
}

void RenderSystem::DestroyPerFrameResources(VkDevice _device, uint32_t bufferCount)
{
    for(uint32_t i=0;i<bufferCount;++i)
    {
        vkDestroyFramebuffer(_device, uiFramebuffers[i], NULL);
        vkDestroyImageView(_device, uiRTVs[i], NULL);
        vkDestroyImage(_device, uiRTs[i], NULL);
        vkFreeMemory(_device, uiRTMemBlocks[i], NULL);

        vkDestroyFramebuffer(_device, gameObjectFramebuffers[i], NULL);
        vkDestroyImageView(_device, gameObjectRTVs[i], NULL);
        vkDestroyImage(_device, gameObjectRTs[i], NULL);
        vkFreeMemory(_device, gameObjectRTMemBlocks[i], NULL);
        vkDestroyImageView(_device, gameObjectBloomRTVs[i], NULL);
        vkDestroyImage(_device, gameObjectBloomRTs[i], NULL);
        vkFreeMemory(_device, gameObjectBloomRTMemBlocks[i], NULL);
        vkDestroyImageView(_device, gameObjectDTVs[i], NULL);
        vkDestroyImage(_device, gameObjectDTs[i], NULL);
        vkFreeMemory(_device, gameObjectDTMemBlocks[i], NULL);

        vkDestroyFramebuffer(_device, shadowMapFramebuffers[i], NULL);
        vkDestroyImageView(_device, shadowMapDTVs[i], NULL);
        vkDestroyImage(_device, shadowMapDTs[i], NULL);
        vkFreeMemory(_device, shadowMapDTMemBlocks[i], NULL);

        vkDestroyFramebuffer(_device, blurPingFramebuffers[i], NULL);
        vkDestroyFramebuffer(_device, blurPongFramebuffers[i], NULL);
        vkDestroyImageView(_device, blurRTVs[i], NULL);
        vkDestroyImage(_device, blurRTs[i], NULL);
        vkFreeMemory(_device, blurRTMemBlocks[i], NULL);

        vkDestroyBuffer(_device, uiSDFVertexDataBuffers[i], NULL);
        vkFreeMemory(_device, uiSDFVertexDataMemoryBlocks[i], NULL);
        vkDestroyBuffer(_device, uiSDFIndexDataBuffers[i], NULL);
        vkFreeMemory(_device, uiSDFIndexDataMemoryBlocks[i], NULL);
        vkDestroyBuffer(_device, uiBlitInstanceDataBuffers[i], NULL);
        vkFreeMemory(_device, uiBlitInstanceDataMemoryBlocks[i], NULL);
        vkDestroyBuffer(_device, instanceVertexDataBuffers[i], NULL);
        vkFreeMemory(_device, instanceVertexDataMemoryBlocks[i], NULL);
    }
    vkDestroyDescriptorPool(_device, shadowMapDescriptorPool, NULL);
    vkDestroyDescriptorPool(_device, blurDescriptorPool, NULL);
    vkDestroyDescriptorPool(_device, perFrameDescriptorPool, NULL);
}

#define ptr_offset(x, offset) ((void*)((uint8_t*)x + offset))
void RenderSystem::LoadShaderFileData(const char *vertexShaderPath, const char *pixelShaderPath, GW::SYSTEM::GFile& _fileInterface, ShaderFileData &data)
{
    uint32_t vertexShaderFileSize = 0, pixelShaderFileSize = 0;
    _fileInterface.GetFileSize(vertexShaderPath, vertexShaderFileSize);
    _fileInterface.GetFileSize(pixelShaderPath, pixelShaderFileSize);
    if(!vertexShaderFileSize || !pixelShaderFileSize) return;
    data.vertexShaderFileSize = vertexShaderFileSize;
    data.pixelShaderFileSize = pixelShaderFileSize;
    data.vertexShaderFileData = (uint32_t*)malloc(data.vertexShaderFileSize + data.pixelShaderFileSize);
    data.pixelShaderFileData = (uint32_t*)ptr_offset(data.vertexShaderFileData, data.vertexShaderFileSize);
    _fileInterface.OpenBinaryRead(vertexShaderPath);
    _fileInterface.Read((char*)data.vertexShaderFileData, data.vertexShaderFileSize);
    _fileInterface.CloseFile();
    _fileInterface.OpenBinaryRead(pixelShaderPath);
    _fileInterface.Read((char*)data.pixelShaderFileData, data.pixelShaderFileSize);
    _fileInterface.CloseFile();
}
void RenderSystem::FreeShaderFileData(ShaderFileData &data)
{
    free(data.vertexShaderFileData);
}
void RenderSystem::LoadBMFont(const char *_filePath, BMFont &_font)
{
    for(unsigned i=0; i < 255; ++i) _font.chars[i].page = ~(0u);
    uint32_t fileSize;
    fileInterface.GetFileSize(_filePath, fileSize);
    fileInterface.OpenTextRead(_filePath);
    char linebuf[1024];
    while(fileInterface.ReadLine(linebuf, 1024, '\n') == GW::GReturn::SUCCESS)
    {
        if(strncmp(linebuf, "common", 6) == 0)
        {
            char* ptr= nullptr;
            _font.lineHeight = strtoul(&linebuf[18], &ptr, 10);
        }
        else if(strncmp(linebuf, "char", 4) == 0)
        {
            char* ptr= nullptr;
            uint32_t id = strtoul(&linebuf[8], &ptr, 10);

            _font.chars[id].x = strtoul(&linebuf[18], &ptr, 10);
            _font.chars[id].y = strtoul(&linebuf[25], &ptr, 10);

            _font.chars[id].width  = strtoul(&linebuf[36], &ptr, 10);
            _font.chars[id].height = strtoul(&linebuf[48], &ptr, 10);

            _font.chars[id].xoffset = strtol(&linebuf[61], &ptr, 10);
            _font.chars[id].yoffset = strtol(&linebuf[74], &ptr, 10);

            _font.chars[id].xadvance = strtol(&linebuf[88], &ptr, 10);
            _font.chars[id].page     = strtoul(&linebuf[98], &ptr, 10);
        }
    }
    fileInterface.CloseFile();
}
void RenderSystem::LoadTGATexture(const char *_filePath, VkPhysicalDevice _physicalDevice, VkDevice _device, VkImage* texture, VkDeviceMemory* textureMemory, VkImageView* textureSRV)
{
    fileInterface.OpenBinaryRead(_filePath);
    TGAHeader tgaHeader;
    fileInterface.Read((char*)&tgaHeader.idLength, sizeof(uint8_t) * 3);
    fileInterface.Read((char*)&tgaHeader.firstIndex, sizeof(uint16_t) * 2);
    fileInterface.Read((char*)&tgaHeader.entrySize, sizeof(uint8_t));
    fileInterface.Read((char*)&tgaHeader.xOrigin, sizeof(uint16_t) * 4);
    fileInterface.Read((char*)&tgaHeader.bitsPerPixel, sizeof(uint8_t) * 2);
    GvkHelper::create_image_set(_physicalDevice, _device, commandPool,
        { tgaHeader.width, tgaHeader.height,1 }, graphicsQueue, 1,
        VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        NULL, texture, textureSRV, textureMemory);
    /*-----------------------------------------------------------------------*/
    uint32_t textureDataSize = (tgaHeader.bitsPerPixel / 8);
    textureDataSize *= tgaHeader.width * tgaHeader.height;
    fileInterface.Read((char*)stagingMappedMemory, textureDataSize);
    GvkHelper::copy_buffer_to_image(_device, commandPool, graphicsQueue,
        stagingBuffer, *texture,
        { tgaHeader.width, tgaHeader.height, 1 });
    fileInterface.CloseFile();
    /*-----------------------------------------------------------------------*/
    GvkHelper::transition_image_layout(_device, commandPool, graphicsQueue,
        1, *texture, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    /*-----------------------------------------------------------------------*/
}
void RenderSystem::LoadDDSTexture(const char *_filePath, VkPhysicalDevice _physicalDevice, VkDevice _device, VkImage* texture, VkDeviceMemory* textureMemory, VkImageView* textureSRV)
{
    uint32_t textureDataSize;
    fileInterface.GetFileSize(_filePath, textureDataSize);
    fileInterface.OpenBinaryRead(_filePath);
    uint32_t magic;
    fileInterface.Read((char *)&magic, sizeof(uint32_t));
    DDSHeader ddsHeader;
    fileInterface.Read((char *)&ddsHeader, sizeof(DDSHeader));
    DDSHeaderDX10 ddsHeaderDX10;
    fileInterface.Read((char *)&ddsHeaderDX10, sizeof(DDSHeaderDX10));
    /*-----------------------------------------------------------------------*/
    GvkHelper::create_image_set(_physicalDevice, _device, commandPool,
        {ddsHeader.width, ddsHeader.height,1}, graphicsQueue, ddsHeader.mipCount,
        VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_BC7_UNORM_BLOCK, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        NULL, texture, textureSRV, textureMemory);
    /*-----------------------------------------------------------------------*/
    uint32_t textureDataOffset = sizeof(uint32_t) + sizeof(DDSHeader) + sizeof(DDSHeaderDX10);
    textureDataSize -= textureDataOffset;
    fileInterface.Read((char*)stagingMappedMemory, textureDataSize);
    GvkHelper::copy_buffer_to_image(_device, commandPool, graphicsQueue,
        stagingBuffer, *texture,
        {ddsHeader.width, ddsHeader.height, 1});
    fileInterface.CloseFile();
    /*-----------------------------------------------------------------------*/
    GvkHelper::transition_image_layout(_device, commandPool, graphicsQueue,
        1, *texture, VK_FORMAT_BC7_UNORM_BLOCK,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    /*-----------------------------------------------------------------------*/
}
void RenderSystem::LoadDDSCubemap(const char *_filePath, VkPhysicalDevice _physicalDevice, VkDevice _device, VkImage* texture, VkDeviceMemory* textureMemory, VkImageView* textureSRV)
{
    uint32_t textureDataSize;
    fileInterface.GetFileSize(_filePath, textureDataSize);
    fileInterface.OpenBinaryRead(_filePath);
    uint32_t magic;
    fileInterface.Read((char *)&magic, sizeof(uint32_t));
    DDSHeader ddsHeader;
    fileInterface.Read((char *)&ddsHeader, sizeof(DDSHeader));
    DDSHeaderDX10 ddsHeaderDX10;
    fileInterface.Read((char *)&ddsHeaderDX10, sizeof(DDSHeaderDX10));
    /*-----------------------------------------------------------------------*/
    VkImageCreateInfo image_create_info = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        NULL,
        VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_BC7_UNORM_BLOCK,
        { ddsHeader.width, ddsHeader.height, 1}, 1, 6,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
        VK_IMAGE_LAYOUT_UNDEFINED 
    };
    vkCreateImage(_device, &image_create_info, NULL, texture);
    /*-----------------------------------------------------------------------*/
    uint32_t textureDataOffset = sizeof(uint32_t) + sizeof(DDSHeader) + sizeof(DDSHeaderDX10);
    textureDataSize -= textureDataOffset;
    fileInterface.Read((char*)stagingMappedMemory, textureDataSize);
    fileInterface.CloseFile();
    /*-----------------------------------------------------------------------*/
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(_device, *texture, &memReqs);
    uint32_t memTypeIndex;
    GvkHelper::find_memory_type(_physicalDevice,
        memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memTypeIndex);
    VkMemoryAllocateInfo alloc_info = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        NULL,
        textureDataSize,
        memTypeIndex
    };
    vkAllocateMemory(_device, &alloc_info, NULL, textureMemory);
    vkBindImageMemory(_device, *texture, *textureMemory, 0);
    /*-----------------------------------------------------------------------*/
    VkImageViewCreateInfo image_view_create_info = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        NULL,
        0,
        *texture,
        VK_IMAGE_VIEW_TYPE_CUBE,
        VK_FORMAT_BC7_UNORM_BLOCK,
        { VK_COMPONENT_SWIZZLE_IDENTITY },
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 }
    };
    vkCreateImageView(_device, &image_view_create_info, NULL, textureSRV);
    /*-----------------------------------------------------------------------*/
    VkCommandBuffer cmd;
    GvkHelper::signal_command_start(_device, commandPool, &cmd);
    VkImageMemoryBarrier src_barrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        NULL,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        *texture,
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 }
    };
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &src_barrier);
    VkBufferImageCopy buffer_image_copy = {
        0, 0, 0,
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 6 },
        { 0, 0, 0},
        { ddsHeader.width, ddsHeader.height, 1 }
    };
    vkCmdCopyBufferToImage(cmd,
        stagingBuffer, *texture,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &buffer_image_copy);
    VkImageMemoryBarrier dst_barrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        NULL,
        0,
        0,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        *texture,
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 }
    };
    vkCmdPipelineBarrier(cmd,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &dst_barrier);
    GvkHelper::signal_command_end(_device, graphicsQueue, commandPool, &cmd);
    /*-----------------------------------------------------------------------*/
}
void RenderSystem::LoadH2BMesh(const char* _filePath, Mesh& _mesh, MeshBounds& _bounds)
{
    H2B::Parser parser;
    parser.Parse(_filePath);

    _mesh.vertexCount=parser.vertexCount;
    _mesh.indexCount=parser.indexCount;

    VkDeviceSize vertexWriteSize = sizeof(H2B::VERTEX) * _mesh.vertexCount;
    VkDeviceSize indexWriteSize = sizeof(uint32_t) * _mesh.indexCount;

    H2B::VERTEX* vertexData = (H2B::VERTEX*)stagingMappedMemory;
    uint32_t* indexData = (uint32_t*)ptr_offset(stagingMappedMemory, vertexWriteSize);

    _bounds.min = GW::MATH::GVECTORF{ 0, 0, 0, 1 };
    _bounds.max = GW::MATH::GVECTORF{ 0, 0, 0, 1 };
    for(int i = 0; i < _mesh.vertexCount; ++i)
    {
        vertexData[i] = parser.vertices[i];

        if(_bounds.min.z > vertexData[i].pos.x) _bounds.min.z = vertexData[i].pos.x;
        if(_bounds.min.x > vertexData[i].pos.y) _bounds.min.x = vertexData[i].pos.y;
        if(_bounds.min.y > vertexData[i].pos.z) _bounds.min.y = vertexData[i].pos.z;

        if(_bounds.max.z < vertexData[i].pos.x) _bounds.max.z = vertexData[i].pos.x;
        if(_bounds.max.x < vertexData[i].pos.y) _bounds.max.x = vertexData[i].pos.y;
        if(_bounds.max.y < vertexData[i].pos.z) _bounds.max.y = vertexData[i].pos.z;
    }
    memcpy(indexData, parser.indices.data(), indexWriteSize);

    VkCommandBuffer command_buffer;
    VkBufferCopy buffer_copy = {};

    GvkHelper::signal_command_start(device, commandPool, &command_buffer);

    buffer_copy.srcOffset = 0;
    buffer_copy.dstOffset = sizeof(H2B::VERTEX) * _mesh.vertexOffset;
    buffer_copy.size = vertexWriteSize;
    vkCmdCopyBuffer(command_buffer, stagingBuffer, meshVertexDataBuffer, 1, &buffer_copy);

    GvkHelper::signal_command_end(device, graphicsQueue, commandPool, &command_buffer);
    GvkHelper::signal_command_start(device, commandPool, &command_buffer);

    buffer_copy.srcOffset = vertexWriteSize;
    buffer_copy.dstOffset = sizeof(uint32_t) * _mesh.indexOffset;
    buffer_copy.size = vertexWriteSize;
    vkCmdCopyBuffer(command_buffer, stagingBuffer, meshIndexDataBuffer, 1, &buffer_copy);

    GvkHelper::signal_command_end(device, graphicsQueue, commandPool, &command_buffer);
}
#ifdef DEV_BUILD
void RenderSystem::UploadMeshBoundsDataToGPU(const MeshBounds &_bounds, uint32_t meshIndex)
{
    H2B::VECTOR *boundsVertexData = (H2B::VECTOR*)stagingMappedMemory;
    boundsVertexData[0] = { 0, _bounds.min.x, _bounds.min.y };
    boundsVertexData[1] = { 0, _bounds.max.x, _bounds.min.y };
    boundsVertexData[2] = { 0, _bounds.min.x, _bounds.max.y };
    boundsVertexData[3] = { 0, _bounds.max.x, _bounds.max.y };
    uint32_t* indexData = (uint32_t*)ptr_offset(stagingMappedMemory, sizeof(H2B::VECTOR) * 4);
    const uint32_t colliderIndices[] { 0, 1, 1, 3, 3, 2, 2, 0 };
    memcpy(indexData, colliderIndices, sizeof(colliderIndices));

    VkCommandBuffer command_buffer;
    VkBufferCopy buffer_copy = {};

    GvkHelper::signal_command_start(device, commandPool, &command_buffer);

    buffer_copy.srcOffset = 0;
    buffer_copy.dstOffset = sizeof(H2B::VECTOR) * 4 * meshIndex;
    buffer_copy.size = sizeof(H2B::VECTOR) * 4;
    vkCmdCopyBuffer(command_buffer, stagingBuffer, debugColliderVertexDataBuffer, 1, &buffer_copy);

    GvkHelper::signal_command_end(device, graphicsQueue, commandPool, &command_buffer);
    GvkHelper::signal_command_start(device, commandPool, &command_buffer);

    buffer_copy.srcOffset = sizeof(H2B::VECTOR) * 4;
    buffer_copy.dstOffset = sizeof(colliderIndices) * meshIndex;
    buffer_copy.size = sizeof(colliderIndices);
    vkCmdCopyBuffer(command_buffer, stagingBuffer, debugColliderIndexDataBuffer, 1, &buffer_copy);

    GvkHelper::signal_command_end(device, graphicsQueue, commandPool, &command_buffer);
}
#endif
#undef ptr_offset
// NOTE: It would be great if Gateware exposed these so that we can create our own
//       attachments to blit to the swapchain before presenting.
void RenderSystem::GetSurfaceFormat(VkPhysicalDevice _physicalDevice, VkSurfaceKHR _surface, VkSurfaceFormatKHR& out)
{
    uint32_t surfaceFormatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _surface, &surfaceFormatCount, VK_NULL_HANDLE);
    VkSurfaceFormatKHR* surfaceFormats = (VkSurfaceFormatKHR*)calloc(sizeof(VkSurfaceFormatKHR), surfaceFormatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _surface, &surfaceFormatCount, surfaceFormats);

    if (surfaceFormatCount == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED) {
        out = { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        free(surfaceFormats);
        return;
    }

    for (int i = 0; i < surfaceFormatCount; ++i)
    {
        if (surfaceFormats[i].format == VK_FORMAT_B8G8R8A8_UNORM && surfaceFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            out = { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
            free(surfaceFormats);
            return;
        }
    }

    out = surfaceFormats[0];
    free(surfaceFormats);
}
void RenderSystem::GetSurfaceExtent(VkPhysicalDevice _physicalDevice, VkSurfaceKHR _surface, VkExtent2D &out)
{
    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physicalDevice, _surface, &surface_capabilities);
    //If Capabilities's extent is not MAX, Set to those extents
    if (surface_capabilities.currentExtent.width != 0xFFFFFFFF)
        out = { surface_capabilities.currentExtent.width, surface_capabilities.currentExtent.height };
}
}