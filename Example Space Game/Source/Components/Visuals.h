// define all ECS components related to drawing
#ifndef VISUALS_H
#define VISUALS_H

namespace TeamYellow
{
    struct Skybox
    {
        uint32_t skyBoxTextureID;
        // Stores Sin and Cos values for the angle the skybox is rotated by
        GW::MATH2D::GVECTOR2F value;
    };
    struct StaticMeshComponent
    {
        uint32_t meshID;
    };
    struct UICanvas
    {
        uint32_t    fontID;
        uint32_t    spriteAtlasID;
        uint32_t    depth;
        bool        isVisible;
    };
    struct UIChildOf {};
    struct UIRect
    {
        float x, y;
        float width, height;
    };
    struct UISprite
    {
        GW::MATH::GVECTORF srcRect;
    };
    struct UIText
    {
        GW::MATH::GVECTORF fontColor;
        GW::MATH::GVECTORF outlineColor;
        float outlineWidth;
        float fontSize;
        char text[248];
    };

    // level tags
    struct Foreground {};
    struct Background {};
    struct Gameobject {};
    struct Floor {};
};

#endif