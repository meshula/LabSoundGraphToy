
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
    src/lab_imgui_ext.cpp
    src/lab_imgui_ext.hpp
    src/lab_noodle.cpp
    src/lab_noodle.h
    src/legit_profiler.hpp
    src/meshula_lab.hpp
    src/IconsFontaudio.h
    src/ImguiFontCousineRegular.cpp
    src/LabSoundInterface.cpp
    src/LabSoundInterface.h
    src/MidiNode.cpp
    src/MidiNode.hpp
    src/OSCMsg.hpp
    src/OSCNode.hpp
    src/OSCNode.cpp
    src/queue_spsc.hpp
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
# LabMidi
#-------------------------------------------------------------------------------

add_subdirectory("third/LabMidi")

#-------------------------------------------------------------------------------
# TinyOSC
#-------------------------------------------------------------------------------

add_subdirectory("third/tinyosc")

#-------------------------------------------------------------------------------
# rapidjson
#-------------------------------------------------------------------------------

set(RAPIDJSON_INCL third/rapidjson/include)

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
    third/imgui/imgui_tables.cpp
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
    USE_DBG_UI
)

if(APPLE)
    set_source_files_properties(src/sokol.cpp PROPERTIES COMPILE_FLAGS "-x objective-c++ -fobjc-arc")
endif()

install(
    TARGETS sokol
    EXPORT sokolTargets
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

set_target_properties(LabSoundGraphToy PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY bin)

target_compile_definitions(LabSoundGraphToy PRIVATE
    ${ST_GFX_DEFS}
    IMGUI_DEFINE_MATH_OPERATORS
    ${PLATFORM_DEFS}
    SOKOL_WIN32_FORCE_MAIN
)

target_include_directories(LabSoundGraphToy SYSTEM
    PRIVATE third/imgui
    PRIVATE third/LabSound/include
    PRIVATE ${RAPIDJSON_INCL}
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

# In order to use the LabSound Registry, and LabSound as a static library,
# whole archive needs to be specified in order not to deadstrip the static
# initializers.
# Unfortunately, the developers of cmake have yet to agree that a
# whole archive flag is in order, and so this terrible contraption is
# necessary.
#
if(MSVC)
    target_link_libraries(LabSoundGraphToy
        ${PLATFORM_LIBS}
        imgui
        libnyquist
        samplerate
        Lab::Midi
        Tiny::OSC
        sokol    
        -WHOLEARCHIVE:$<TARGET_FILE:Lab::Sound>
        )
elseif(CMAKE_COMPILER_IS_GNUCXX)
    target_link_libraries(LabSoundGraphToy
        ${PLATFORM_LIBS}
        imgui
        libnyquist
        samplerate
        Lab::Midi
        Tiny::OSC
        sokol
        -Wl,--whole-archive $<TARGET_FILE:Lab::Sound> -Wl,--no-whole-archive
        )
elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    target_link_libraries(LabSoundGraphToy
        ${PLATFORM_LIBS}
        imgui
        libnyquist
        samplerate
        Lab::Midi
        Tiny::OSC
        sokol
        Lab::Sound
        )
else()
    target_link_libraries(LabSoundGraphToy
        ${PLATFORM_LIBS}
        imgui
        libnyquist
        samplerate
        Lab::Sound
        Lab::Midi
        Tiny::OSC
        sokol
        )
endif()


#-------------------------------------------------------------------------------
# Installer
#-------------------------------------------------------------------------------

install(
    TARGETS LabSoundGraphToy
    BUNDLE DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)

install(
    FILES
        resources/fontaudio.ttf
        resources/preview.png
    DESTINATION bin/LabSoundGraphToy_rsrc/)

install(
    FILES
        resources/fontaudio.ttf
        resources/preview.png
    DESTINATION ${CMAKE_BINARY_DIR}/bin/Debug/LabSoundGraphToy_rsrc/)
install(
    FILES
        resources/fontaudio.ttf
        resources/preview.png
    DESTINATION ${CMAKE_BINARY_DIR}/bin/Release/LabSoundGraphToy_rsrc/)
