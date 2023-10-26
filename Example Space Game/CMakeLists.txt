cmake_minimum_required(VERSION 3.16)

project(SpaceDasher)

# Great way to reduce compile times for large files like Gateware.h 
# https://edgarluque.com/blog/cmake-precompiled-headers/
set(PRE_COMPILED
    ./Source/precompiled.h 
)

# Create list of source files.
# CMake generally discourages the use of GLOB. (it can make doing more complex operations fail)
# For students the conveinece of not editing the CMake constantly is probably more important.  
file(
	GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS
	./Source/*.h
	./Source/*.cpp
    ./Shaders/vs/*.hlsl
	./Shaders/ps/*.hlsl
	./gateware-main/Gateware.h
	./flecs-3.1.4/flecs.h
    ./flecs-3.1.4/flecs.c
	./inifile-cpp-master/include/inicpp.h
	./defaults.ini # get some syntax highlighting
)
# The FLECS library core is written in C. If it tries to use the C++ pre-compiled header it will not compile.
set_source_files_properties(./flecs-3.1.4/flecs.c PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
# Example: You can use this to remove files so later you can add them individually if needed.
#list(FILTER SOURCE_FILES EXCLUDE REGEX ".*main.*.cpp$")

# currently using unicode in some libraries on win32 but will change soon
ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)

if (WIN32)
	# by default CMake selects "ALL_BUILD" as the startup project 
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} 
		 PROPERTY VS_STARTUP_PROJECT SpaceDasher)
	
	set(CMAKE_VS_SDK_EXECUTABLE_DIRECTORIES $ENV{VULKAN_SDK}/Bin;$(ExecutablePath))

	add_executable (SpaceDasher ${SOURCE_FILES})
	
	source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${SOURCE_FILES})
	target_include_directories(SpaceDasher PUBLIC $ENV{VULKAN_SDK}/Include/)
	target_link_directories(SpaceDasher PUBLIC $ENV{VULKAN_SDK}/Lib/)
	target_compile_definitions(SpaceDasher PUBLIC _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)
	target_compile_definitions(SpaceDasher PUBLIC DEV_BUILD)
	# shaderc_combined.lib in Vulkan requires this for debug & release (runtime shader compiling)
	#target_compile_options(SpaceDasher PRIVATE "/MD")
	# IMPORTANT! Below is the OLD way of setting the compiler options it does NOT work with pre-compiled headers!
	#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MD") // DO NOT USE
endif(WIN32)

if(UNIX AND NOT APPLE)
	# libshaderc_combined.a is required for runtime shader compiling
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -lX11 -lshaderc_combined")
    find_package(X11)
	find_package(Vulkan REQUIRED)
    link_libraries(${X11_LIBRARIES})
    include_directories(${X11_INCLUDE_DIR})
    include_directories(${Vulkan_INCLUDE_DIR}) 
	#link_directories(${Vulkan_LIBRARY}) this is currently not working
	link_libraries(${Vulkan_LIBRARIES})
	# the path is (properly)hardcoded because "${Vulkan_LIBRARY}" currently does not 
	# return a proper path on MacOS (it has the .dynlib appended)
    link_libraries(/usr/lib/x86_64-linux-gnu/libshaderc_combined.a)
    add_executable (SpaceDasher ${SOURCE_FILES})
    source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${SOURCE_FILES})
	set(CMAKE_VS_SDK_EXECUTABLE_DIRECTORIES $ENV{VULKAN_SDK}/Bin;$(ExecutablePath))
endif(UNIX AND NOT APPLE)

if(APPLE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fmodules -fcxx-modules")
	set(Architecture ${CMAKE_OSX_ARCHITECTURES})
	find_package(Vulkan REQUIRED)
	include_directories(${Vulkan_INCLUDE_DIR}) 
	#link_directories(${Vulkan_LIBRARY}) this is currently not working
	link_libraries(${Vulkan_LIBRARIES})
	# libshaderc_combined.a is required for runtime shader compiling
	# the path is (properly)hardcoded because "${Vulkan_LIBRARY}" currently does not 
	# return a proper path on MacOS (it has the .dynlib appended)
	link_libraries(/usr/local/lib/libshaderc_combined.a)
	add_executable (SpaceDasher source/main.mm)
	set(CMAKE_VS_SDK_EXECUTABLE_DIRECTORIES $ENV{VULKAN_SDK}/Bin;$(ExecutablePath))
endif(APPLE)

# using some c++17 filesystem features to grab file timestamps
# most code other code in the project only requires c++11
# we could remove this if we had to, but most compilers can do 17 these days
target_compile_features(SpaceDasher PUBLIC cxx_std_17)

# adding gateware.h and other librairies as a precompiled headers to reduce compile times
target_precompile_headers(SpaceDasher PRIVATE ${PRE_COMPILED})

file(
	GLOB_RECURSE SHADER_VS_FILES CONFIGURE_DEPENDS
    ./Shaders/vs/*.hlsl
)
set_source_files_properties( ${SHADER_VS_FILES} PROPERTIES
        VS_SHADER_TYPE Vertex
        VS_SHADER_MODEL 6.6
        VS_SHADER_ENTRYPOINT main
		VS_SHADER_FLAGS "-spirv"
		VS_SHADER_OBJECT_FILE_NAME "$(ProjectDir)/Shaders/%(Filename).spv"
)

file(
	GLOB_RECURSE SHADER_PS_FILES CONFIGURE_DEPENDS
    ./Shaders/ps/*.hlsl
)
set_source_files_properties( ${SHADER_PS_FILES} PROPERTIES
        VS_SHADER_TYPE Pixel
        VS_SHADER_MODEL 6.6
        VS_SHADER_ENTRYPOINT main
		VS_SHADER_FLAGS "-spirv"
		VS_SHADER_OBJECT_FILE_NAME "$(ProjectDir)/Shaders/%(Filename).spv"
)
