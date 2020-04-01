#pragma once

#ifndef lab_imgui_ext_hpp
#define lab_imgui_ext_hpp

#include <string>
#include <vector>
#include <fstream>
#include <iostream>

#include <imgui.h>
#include <imgui_internal.h>
#include "IconsFontaudio.h"

namespace imgui_fonts
{
    unsigned int getCousineRegularCompressedSize();
    const unsigned int * getCousineRegularCompressedData();
}
ImFont * append_audio_icon_font(const std::vector<uint8_t> & buffer);

void imgui_fixed_window_begin(const char * name, float min_x, float min_y, float max_x, float max_y);
void imgui_fixed_window_end();

std::vector<uint8_t> read_file_binary(const std::string & pathToFile);

enum class IconType { Flow, Circle, Square, Grid, RoundSquare, Diamond };
void DrawIcon(ImDrawList* drawList, const ImVec2& ul, const ImVec2& lr, IconType type, bool filled, ImU32 color, ImU32 innerColor);

bool imgui_knob(const char* label, float* p_value, float v_min, float v_max);

#endif lab_imgui_ext_hpp
