// Include access to the Gateware middleware API. (O.S. Abstraction Layer)
#define GATEWARE_ENABLE_CORE // All libraries need this
#define GATEWARE_ENABLE_SYSTEM // Many libs require system level libraries
#define GATEWARE_ENABLE_GRAPHICS // Enables all Graphics Libraries
#define GATEWARE_ENABLE_MATH // Enables all 3D Math Libraries
#define GATEWARE_ENABLE_MATH2D // Enables all 2D Math Libraries
#define GATEWARE_ENABLE_INPUT // Enables all Input Libraries
#define GATEWARE_ENABLE_AUDIO // Enables all Audio Libraries
// Ignore some GRAPHICS libraries we aren't going to use
#define GATEWARE_DISABLE_GDIRECTX11SURFACE 
#define GATEWARE_DISABLE_GDIRECTX12SURFACE 
#define GATEWARE_DISABLE_GRASTERSURFACE
#define GATEWARE_DISABLE_GOPENGLSURFACE
// With what we want & what we don't defined we can include the API
// DOC: gateware-main/documentation/html/index.html
#include "../gateware-main/Gateware.h"
// Popular and Fast ECS(Entity Component System) library.
// DOC: https://www.flecs.dev/flecs/index.html
#include "../flecs-3.1.4/flecs.h"
// Library for processing .ini files
#include "../inifile-cpp-master/include/inicpp.h"
#ifndef _WIN32
// used to compile shaders for Vulkan
#include "shaderc/shaderc.h" // needed for compiling shaders at runtime
#endif