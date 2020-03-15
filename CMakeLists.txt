
cmake_minimum_required(VERSION 3.11)
project(LabSoundGraphToy VERSION 0.1.0 LANGUAGES C CXX)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(third/LabSound)

if (WIN32)
    set (PLATFORM_DEFS
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()

set(PLAYGROUND_SRC
    src/main.cpp
    src/lab_noodle.cpp
)

if(APPLE)
set(ST_GFX_DEFS
    IMGUI_IMPL_API=
    SOKOL_METAL
)
else()
set(ST_GFX_DEFS
    IMGUI_IMPL_API=
    SOKOL_GLCORE33
)
endif()

#-------------------------------------------------------------------------------
# imgui
#-------------------------------------------------------------------------------

if (WIN32)
    set(IMGUI_EXTRA third/ImGui.natvis)
endif()

set(IMGUI_SRC
    third/imgui/imgui.cpp
    third/imgui/imgui.h
    third/imgui/imgui_demo.cpp
    third/imgui/imgui_draw.cpp
    third/imgui/imgui_internal.h
    third/imgui/imgui_widgets.cpp
    ${IMGUI_EXTRA}
)

add_library(imgui STATIC ${IMGUI_SRC})
target_include_directories(imgui SYSTEM
    PUBLIC third/imgui)
target_compile_definitions(imgui PRIVATE
    ${ST_GFX_DEFS}
    IMGUI_DEFINE_MATH_OPERATORS
    ${PLATFORM_DEFS}
)

#-------------------------------------------------------------------------------
# sokol
#-------------------------------------------------------------------------------

set(SOKOL_SRC 
    src/sokol.cpp)

set(SOKOL_HEADERS 
    third/sokol/sokol_app.h
    third/sokol/sokol_args.h
    third/sokol/sokol_audio.h
    third/sokol/sokol_fetch.h
    third/sokol/sokol_gfx.h
    third/sokol/sokol_time.h
    third/sokol/util/sokol_fontstash.h
    third/sokol/util/sokol_gfx_imgui.h
    third/sokol/util/sokol_gl.h
    third/sokol/util/sokol_imgui.h
)
add_library(sokol STATIC ${SOKOL_SRC} ${SOKOL_HEADERS})
target_include_directories(sokol SYSTEM 
    PUBLIC third/sokol third/sokol/util src)
target_link_libraries(sokol imgui)
add_dependencies(sokol imgui)
target_compile_definitions(sokol PRIVATE
    ${ST_GFX_DEFS}
    IMGUI_DEFINE_MATH_OPERATORS
    ${PLATFORM_DEFS}
)

if(APPLE)
    set_source_files_properties(src/sokol.cpp PROPERTIES COMPILE_FLAGS "-x objective-c++ -fobjc-arc")
endif()

install(
    TARGETS sokol
    EXPORT pxrTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/sokol
)

#-------------------------------------------------------------------------------
# LabSoundGraphToy
#-------------------------------------------------------------------------------

set(PLAYGROUND_SHADERS
#    simple_cube instancing billboard_instances
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated_src)
foreach(SHADER ${PLAYGROUND_SHADERS})
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated_src/${SHADER}_glsl.h
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/third/sokol-tools-bin/bin/win32/sokol-shdc.exe --input ${CMAKE_CURRENT_SOURCE_DIR}/src/${SHADER}.glsl --output ${CMAKE_CURRENT_BINARY_DIR}/generated_src/${SHADER}_glsl.h --slang glsl330:hlsl5:metal_macos
        COMMENT "Running sokol-shdc for ${SHADER}.glsl"
    )
    list(APPEND PLAYGROUND_SHADERS_SRC ${CMAKE_CURRENT_BINARY_DIR}/generated_src/${SHADER}_glsl.h)
endforeach()
add_custom_target(ProcessPlaygroundShaderFiles ALL DEPENDS ${PLAYGROUND_SHADERS_SRC})

if(APPLE)
    set(NFD src/nfd.h src/nfd.mm)
else()
    set(NFD src/nfd.h src/nfd.cc)
endif()

add_executable(LabSoundGraphToy
    ${NFD}
    ${PLAYGROUND_SRC}
    ${ST_GFX_SRC}
    ${PLAYGROUND_SHADERS_SRC}
)
add_dependencies(LabSoundGraphToy ProcessPlaygroundShaderFiles)

target_compile_definitions(LabSoundGraphToy PRIVATE
    ${ST_GFX_DEFS}
    IMGUI_DEFINE_MATH_OPERATORS
    ${PLATFORM_DEFS}
    SOKOL_WIN32_FORCE_MAIN
)

target_include_directories(LabSoundGraphToy SYSTEM
    PRIVATE third/imgui
    PRIVATE third/entt/single_include)

target_include_directories(LabSoundGraphToy
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated_src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE third/imgui/examples)

set_property(TARGET LabSoundGraphToy PROPERTY CXX_STANDARD 17)
set_property(TARGET LabSoundGraphToy PROPERTY CXX_STANDARD_REQUIRED ON)

if (WIN32)
    set(PLATFORM_LIBS ws2_32 Iphlpapi.lib opengl32.lib)
elseif (APPLE)
#    if(CMAKE_OSX_SYSROOT MATCHES ".*iphoneos.*")
#        set(DARWIN_LIBS
#            "-framework AudioToolbox"
#            "-framework Accelerate"
#            "-framework CoreAudio")
#    else()
        set(PLATFORM_LIBS
            "-framework AudioToolbox"
            "-framework AudioUnit"
            "-framework Accelerate"
            "-framework Cocoa"
            "-framework CoreAudio"
            "-framework Metal"
            "-framework MetalKit"
            "-framework QuartzCore"
            )
#    endif()
endif()

target_link_libraries(LabSoundGraphToy
    ${PLATFORM_LIBS}
    imgui
    Lab::Sound
    sokol)
        
#-------------------------------------------------------------------------------
# Installer
#-------------------------------------------------------------------------------

install(TARGETS LabSoundGraphToy DESTINATION bin)
