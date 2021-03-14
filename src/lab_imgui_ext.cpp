
#include "lab_imgui_ext.hpp"

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

void imgui_fixed_window_begin(const char * name, float min_x, float min_y, float max_x, float max_y)
{
    ImGui::SetNextWindowPos({min_x, min_y});
    ImGui::SetNextWindowSize({max_x - min_x, max_y - min_y});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0, 0));
    bool result = ImGui::Begin(name, NULL, ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    assert(result);
}

void imgui_fixed_window_end()
{
    ImGui::End();
    ImGui::PopStyleVar(2);
}

std::vector<uint8_t> read_file_binary(const std::string & pathToFile)
{
    std::ifstream file(pathToFile, std::ios::binary);
    std::vector<uint8_t> fileBufferBytes;

    if (file.is_open())
    {
        file.seekg(0, std::ios::end);
        size_t sizeBytes = file.tellg();
        file.seekg(0, std::ios::beg);
        fileBufferBytes.resize(sizeBytes);
        if (file.read((char*)fileBufferBytes.data(), sizeBytes)) return fileBufferBytes;
    }
    else throw std::runtime_error("could not open binary ifstream to path " + pathToFile);
    return fileBufferBytes;
}

ImFont * append_audio_icon_font(const std::vector<uint8_t> & buffer)
{
    static std::vector<uint8_t> s_audioIconFontBuffer;

    ImGuiIO & io = ImGui::GetIO();
    s_audioIconFontBuffer = buffer;
    static const ImWchar icons_ranges[] = { ICON_MIN_FAD, ICON_MAX_FAD, 0 };
    ImFontConfig icons_config; 
    icons_config.MergeMode = true; 
    icons_config.PixelSnapH = true;
    icons_config.FontDataOwnedByAtlas = false;
    icons_config.GlyphOffset = {0, 5};
    auto g_audio_icon = io.Fonts->AddFontFromMemoryTTF((void *)s_audioIconFontBuffer.data(), (int)s_audioIconFontBuffer.size(), 20.f, &icons_config, icons_ranges);
    IM_ASSERT(g_audio_icon != NULL);
    return g_audio_icon;
}

// This Icon drawing is adapted from thedmd's imgui_node_editor blueprint sample because the icons are nice

void DrawIcon(ImDrawList* drawList, const ImVec2& ul, const ImVec2& lr, IconType type, bool filled, ImU32 color, ImU32 innerColor)
{
    struct rectf {
        float x, y, w, h;
        float center_x() const { return x + w * 0.5f; }
        float center_y() const { return y + h * 0.5f; }
    };

    rectf rect = { ul.x, ul.y, lr.x - ul.x, lr.y - ul.y };
    const float outline_scale = rect.w / 24.0f;
    const int extra_segments = static_cast<int>(2 * outline_scale); // for full circle

    if (type == IconType::Flow)
    {
        const auto origin_scale = rect.w / 24.0f;

        const auto offset_x = 1.0f * origin_scale;
        const auto offset_y = 0.0f * origin_scale;
        const auto margin = (filled ? 2.0f : 2.0f) * origin_scale;
        const auto rounding = 0.1f * origin_scale;
        const auto tip_round = 0.7f; // percentage of triangle edge (for tip)
                                     //const auto edge_round = 0.7f; // percentage of triangle edge (for corner)
        const rectf canvas{
            rect.x + margin + offset_x,
            rect.y + margin + offset_y,
            rect.w - margin * 2.0f,
            rect.h - margin * 2.0f };

        const auto left = canvas.x + canvas.w * 0.5f * 0.3f;
        const auto right = canvas.x + canvas.w - canvas.w * 0.5f * 0.3f;
        const auto top = canvas.y + canvas.h * 0.5f * 0.2f;
        const auto bottom = canvas.y + canvas.h - canvas.h * 0.5f * 0.2f;
        const auto center_y = (top + bottom) * 0.5f;
        //const auto angle = AX_PI * 0.5f * 0.5f * 0.5f;

        const auto tip_top = ImVec2(canvas.x + canvas.w * 0.5f, top);
        const auto tip_right = ImVec2(right, center_y);
        const auto tip_bottom = ImVec2(canvas.x + canvas.w * 0.5f, bottom);

        drawList->PathLineTo(ImVec2(left, top) + ImVec2(0, rounding));
        drawList->PathBezierCurveTo(
            ImVec2(left, top),
            ImVec2(left, top),
            ImVec2(left, top) + ImVec2(rounding, 0));
        drawList->PathLineTo(tip_top);
        drawList->PathLineTo(tip_top + (tip_right - tip_top) * tip_round);
        drawList->PathBezierCurveTo(
            tip_right,
            tip_right,
            tip_bottom + (tip_right - tip_bottom) * tip_round);
        drawList->PathLineTo(tip_bottom);
        drawList->PathLineTo(ImVec2(left, bottom) + ImVec2(rounding, 0));
        drawList->PathBezierCurveTo(
            ImVec2(left, bottom),
            ImVec2(left, bottom),
            ImVec2(left, bottom) - ImVec2(0, rounding));

        if (!filled)
        {
            if (innerColor & 0xFF000000)
                drawList->AddConvexPolyFilled(drawList->_Path.Data, drawList->_Path.Size, innerColor);

            drawList->PathStroke(color, true, 2.0f * outline_scale);
        }
        else
            drawList->PathFillConvex(color);
    }
    else
    {
        auto triangleStart = rect.center_x() + 0.32f * rect.w;

        rect.x -= static_cast<int>(rect.w * 0.25f * 0.25f);

        if (type == IconType::Circle)
        {
            const ImVec2 c{ rect.center_x(), rect.center_y() };

            if (!filled)
            {
                const auto r = 0.5f * rect.w / 2.0f - 0.5f;

                if (innerColor & 0xFF000000)
                    drawList->AddCircleFilled(c, r, innerColor, 12 + extra_segments);
                drawList->AddCircle(c, r, color, 12 + extra_segments, 2.0f * outline_scale);
            }
            else
                drawList->AddCircleFilled(c, 0.5f * rect.w / 2.0f, color, 12 + extra_segments);
        }
        else if (type == IconType::Square)
        {
            if (filled)
            {
                const auto r = 0.5f * rect.w / 2.0f;
                const auto p0 = ImVec2{ rect.center_x(), rect.center_y() } -ImVec2(r, r);
                const auto p1 = ImVec2{ rect.center_x(), rect.center_y() } +ImVec2(r, r);

                drawList->AddRectFilled(p0, p1, color, 0, 15 + extra_segments);
            }
            else
            {
                const auto r = 0.5f * rect.w / 2.0f - 0.5f;
                const auto p0 = ImVec2{ rect.center_x(), rect.center_y() } -ImVec2(r, r);
                const auto p1 = ImVec2{ rect.center_x(), rect.center_y() } +ImVec2(r, r);

                if (innerColor & 0xFF000000)
                    drawList->AddRectFilled(p0, p1, innerColor, 0, 15 + extra_segments);

                drawList->AddRect(p0, p1, color, 0, 15 + extra_segments, 2.0f * outline_scale);
            }
        }
        else if (type == IconType::Grid)
        {
            const auto r = 0.5f * rect.w / 2.0f;
            const auto w = ceilf(r / 3.0f);

            const auto baseTl = ImVec2(floorf(rect.center_x() - w * 2.5f), floorf(rect.center_y() - w * 2.5f));
            const auto baseBr = ImVec2(floorf(baseTl.x + w), floorf(baseTl.y + w));

            auto tl = baseTl;
            auto br = baseBr;
            for (int i = 0; i < 3; ++i)
            {
                tl.x = baseTl.x;
                br.x = baseBr.x;
                drawList->AddRectFilled(tl, br, color);
                tl.x += w * 2;
                br.x += w * 2;
                if (i != 1 || filled)
                    drawList->AddRectFilled(tl, br, color);
                tl.x += w * 2;
                br.x += w * 2;
                drawList->AddRectFilled(tl, br, color);

                tl.y += w * 2;
                br.y += w * 2;
            }

            triangleStart = br.x + w + 1.0f / 24.0f * rect.w;
        }
        else if (type == IconType::RoundSquare)
        {
            if (filled)
            {
                const auto r = 0.5f * rect.w / 2.0f;
                const auto cr = r * 0.5f;
                const auto p0 = ImVec2{ rect.center_x(), rect.center_y() } -ImVec2(r, r);
                const auto p1 = ImVec2{ rect.center_x(), rect.center_y() } +ImVec2(r, r);

                drawList->AddRectFilled(p0, p1, color, cr, 15);
            }
            else
            {
                const auto r = 0.5f * rect.w / 2.0f - 0.5f;
                const auto cr = r * 0.5f;
                const auto p0 = ImVec2{ rect.center_x(), rect.center_y() } -ImVec2(r, r);
                const auto p1 = ImVec2{ rect.center_x(), rect.center_y() } +ImVec2(r, r);

                if (innerColor & 0xFF000000)
                    drawList->AddRectFilled(p0, p1, innerColor, cr, 15);

                drawList->AddRect(p0, p1, color, cr, 15, 2.0f * outline_scale);
            }
        }
        else if (type == IconType::Diamond)
        {
            if (filled)
            {
                const auto r = 0.607f * rect.w / 2.0f;
                const auto c = ImVec2{ rect.center_x(), rect.center_y() };

                drawList->PathLineTo(c + ImVec2(0, -r));
                drawList->PathLineTo(c + ImVec2(r, 0));
                drawList->PathLineTo(c + ImVec2(0, r));
                drawList->PathLineTo(c + ImVec2(-r, 0));
                drawList->PathFillConvex(color);
            }
            else
            {
                const auto r = 0.607f * rect.w / 2.0f - 0.5f;
                const auto c = ImVec2{ rect.center_x(), rect.center_y() };

                drawList->PathLineTo(c + ImVec2(0, -r));
                drawList->PathLineTo(c + ImVec2(r, 0));
                drawList->PathLineTo(c + ImVec2(0, r));
                drawList->PathLineTo(c + ImVec2(-r, 0));

                if (innerColor & 0xFF000000)
                    drawList->AddConvexPolyFilled(drawList->_Path.Data, drawList->_Path.Size, innerColor);

                drawList->PathStroke(color, true, 2.0f * outline_scale);
            }
        }
        else
        {
            const auto triangleTip = triangleStart + rect.w * (0.45f - 0.32f);


            drawList->AddTriangleFilled(
                ImVec2(ceilf(triangleTip), rect.center_y() * 0.5f),
                ImVec2(triangleStart, rect.center_y() + 0.15f * rect.h),
                ImVec2(triangleStart, rect.center_y() - 0.15f * rect.h),
                color);
        }
    }
}

bool imgui_knob(const char* label, float* p_value, float v_min, float v_max)
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();

    float radius_outer = 20.0f;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 center = ImVec2(pos.x + radius_outer, pos.y + radius_outer);
    float line_height = ImGui::GetTextLineHeight();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    float ANGLE_MIN = 3.141592f * 0.75f;
    float ANGLE_MAX = 3.141592f * 2.25f;

    ImGui::InvisibleButton(label, ImVec2(radius_outer * 2, radius_outer * 2 + line_height + style.ItemInnerSpacing.y));
    bool value_changed = false;
    bool is_active = ImGui::IsItemActive();
    bool is_hovered = ImGui::IsItemActive();
    if (is_active && io.MouseDelta.x != 0.0f)
    {
        float step = (v_max - v_min) / 200.0f;
        *p_value += io.MouseDelta.x * step;
        if (*p_value < v_min) *p_value = v_min;
        if (*p_value > v_max) *p_value = v_max;
        value_changed = true;
    }

    float t = (*p_value - v_min) / (v_max - v_min);
    float angle = ANGLE_MIN + (ANGLE_MAX - ANGLE_MIN) * t;
    float angle_cos = cosf(angle), angle_sin = sinf(angle);
    float radius_inner = radius_outer * 0.40f;
    draw_list->AddCircleFilled(center, radius_outer, ImGui::GetColorU32(ImGuiCol_FrameBg), 16);
    draw_list->AddLine(ImVec2(center.x + angle_cos * radius_inner, center.y + angle_sin * radius_inner), ImVec2(center.x + angle_cos * (radius_outer - 2), center.y + angle_sin * (radius_outer - 2)), ImGui::GetColorU32(ImGuiCol_SliderGrabActive), 2.0f);
    draw_list->AddCircleFilled(center, radius_inner, ImGui::GetColorU32(is_active ? ImGuiCol_FrameBgActive : is_hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), 16);
    draw_list->AddText(ImVec2(pos.x, pos.y + radius_outer * 2 + style.ItemInnerSpacing.y), ImGui::GetColorU32(ImGuiCol_Text), label);

    if (is_active || is_hovered)
    {
        ImGui::SetNextWindowPos(ImVec2(pos.x - style.WindowPadding.x, pos.y - line_height - style.ItemInnerSpacing.y - style.WindowPadding.y));
        ImGui::BeginTooltip();
        ImGui::Text("%.3f", *p_value);
        ImGui::EndTooltip();
    }

    return value_changed;
}

bool imgui_splitter(bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size)
{
    using namespace ImGui;
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiID id = window->GetID("##Splitter");
    ImRect bb;
    bb.Min = window->DC.CursorPos + (split_vertically ? ImVec2(*size1, 0.0f) : ImVec2(0.0f, *size1));
    bb.Max = bb.Min + CalcItemSize(split_vertically ? ImVec2(thickness, splitter_long_axis_size) : ImVec2(splitter_long_axis_size, thickness), 0.0f, 0.0f);
    window->DrawList->AddRect(bb.Min, bb.Max, IM_COL32(255,255,0,255));

    return ImGui::SplitterBehavior(bb, id, split_vertically ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2, min_size1, min_size2, 0.0f);
}

// Piano display: https://github.com/shric/midi
// The MIT License (MIT)
// Copyright (c) 2020 Chris Young

class Piano {
    int key_states[256] = {0};
public:
    void up(int key);
    void draw(bool *show);
    void down(int key, int velocity);
    std::vector<int> current_notes();
};


static bool has_black(int key) {
    return (!((key - 1) % 7 == 0 || (key - 1) % 7 == 3) && key != 51);
}

void Piano::up(int key) {
    key_states[key] = 0;
}

void Piano::down(int key, int velocity) {
    key_states[key] = velocity;

}

void Piano::draw(bool *show) {
    ImU32 Black = IM_COL32(0, 0, 0, 255);
    ImU32 White = IM_COL32(255, 255, 255, 255);
    ImU32 Red = IM_COL32(255,0,0,255);
    ImGui::Begin("Keyboard", show);
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    int width = 20;
    int cur_key = 21;
    for (int key = 0; key < 52; key++) {
        ImU32 col = White;
        if (key_states[cur_key]) {
            col = Red;
        }
        draw_list->AddRectFilled(
                ImVec2(p.x + key * width, p.y),
                ImVec2(p.x + key * width + width, p.y + 120),
                col, 0, ImDrawCornerFlags_All);
        draw_list->AddRect(
                ImVec2(p.x + key * width, p.y),
                ImVec2(p.x + key * width + width, p.y + 120),
                Black, 0, ImDrawCornerFlags_All);
        cur_key++;
        if (has_black(key)) {
            cur_key++;
        }
    }
    cur_key = 22;
    for (int key = 0; key < 52; key++) {
        if (has_black(key)) {
            ImU32 col = Black;
            if (key_states[cur_key]) {
                col = Red;
            }
            draw_list->AddRectFilled(
                    ImVec2(p.x + key * width + width * 3 / 4, p.y),
                    ImVec2(p.x + key * width + width * 5 / 4 + 1, p.y + 80),
                    col, 0, ImDrawCornerFlags_All);
            draw_list->AddRect(
                    ImVec2(p.x + key * width + width * 3 / 4, p.y),
                    ImVec2(p.x + key * width + width * 5 / 4 + 1, p.y + 80),
                    Black, 0, ImDrawCornerFlags_All);

            cur_key += 2;
        } else {
            cur_key++;
        }
    }
    ImGui::End();
}

std::vector<int> Piano::current_notes() {
    std::vector<int> result{};
    for (int i = 0; i < 256; i++) {
        if (key_states[i]) {
            result.push_back(i);
        }
    }
    return result;
}
