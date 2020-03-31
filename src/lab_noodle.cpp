
#include "lab_noodle.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <entt/entt.hpp>
#include <LabSound/LabSound.h>
#include <LabSound/core/AudioNode.h>

#include "LabSoundInterface.h"
#include "nfd.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace lab {
namespace noodle {

    using std::map;
    using std::pair;
    using std::shared_ptr;
    using std::string;
    using std::unique_ptr;
    using std::unordered_map;
    using std::unordered_set;
    using std::vector;
    using namespace ImGui;

    string unique_name(string name)
    {
        static unordered_map<string, int> bases;
        static unordered_set<string> names;

        size_t pos = name.rfind("-");
        string base;

        // no dash, or leading dash, it's not a uniqued name
        if (pos == string::npos || pos == 0)
        {
            base = name;
            name += "-1";
        }
        else 
            base = name.substr(0, pos);

        // if base isn't already known, remember it, and return name
        auto i = bases.find(base);
        if (i == bases.end())
        {
            bases[base] = 1;
            names.insert(name);
            return name;
        }

        int id = i->second;
        string candidate = base + "-" + std::to_string(id);
        while (names.find(candidate) != names.end())
        {
            ++id;
            candidate = base + "-" + std::to_string(id);
        }
        bases[base] = id;
        names.insert(candidate);
        return candidate;
    }

    struct Name
    {
        string name;
    };

    struct Canvas
    {
        ImVec2 window_origin_offset_ws = { 0, 0 };
        ImVec2 origin_offset_ws = { 0,0 };
        float scale = 1.f;
    }
    g_canvas;

    //--------------------
    // Types

    // Audio Nodes and Noodles

    struct GraphNodeLayout 
    {
        static const float k_column_width;
        ImVec2 pos_cs = { 0,0 }, lr_cs = { 0,0 };
        int in_height = 0, mid_height = 0, out_height = 0;
        int column_count = 1;
    };
    const float GraphNodeLayout::k_column_width = 180.f;

    struct GraphPinLayout
    {
        static const float k_height;
        static const float k_width;

        ImVec2 node_origin_cs = { 0, 0 };
        float pos_y_cs = 0.f;
        float column_number = 0;
        ImVec2 ul_ws(Canvas& canvas) const
        {
            float x = column_number * GraphNodeLayout::k_column_width;
            ImVec2 res = (node_origin_cs + ImVec2{ x, pos_y_cs }) * canvas.scale;
            return res + canvas.origin_offset_ws + canvas.window_origin_offset_ws;
        }
        bool pin_contains_cs_point(Canvas& canvas, float x, float y) const
        {
            ImVec2 ul = (node_origin_cs + ImVec2{ column_number * GraphNodeLayout::k_column_width, pos_y_cs });
            ImVec2 lr = { ul.x + k_width, ul.y + k_height };
            return x >= ul.x && x <= lr.x && y >= ul.y && y <= lr.y;
        }
        bool label_contains_cs_point(Canvas& canvas, float x, float y) const
        {
            ImVec2 ul = (node_origin_cs + ImVec2{ column_number * GraphNodeLayout::k_column_width, pos_y_cs });
            ImVec2 lr = { ul.x + GraphNodeLayout::k_column_width, ul.y + k_height };
            ul.x += k_width;
            //printf("m(%0.1f, %0.1f) ul(%0.1f, %0.1f) lr(%01.f, %0.1f)\n", x, y, ul.x, ul.y, lr.x, lr.y);
            return x >= ul.x && x <= lr.x && y >= ul.y && y <= lr.y;
        }
    };
    const float GraphPinLayout::k_height = 20.f;
    const float GraphPinLayout::k_width = 20.f;

    lab::Sound::Provider _provider;

    //--------------------
    // Enumerations

    // ImGui channels for layering the graphics

    enum Channels {
        ChannelContent = 0, ChannelGrid = 1,
        ChannelNodes = 2,
        ChannelCount = 3
    };

    entt::entity g_ent_main_window;
    ImGuiID g_id_main_window;
    ImGuiID g_id_main_invis;
    ImU32 g_bg_color;
    float g_total_profile_duration = 1; // in microseconds

    struct MouseState
    {
        bool in_canvas = false;
        bool dragging = false;
        bool dragging_wire = false;
        bool dragging_node = false;
        bool interacting_with_canvas = false;
        bool click_initiated = false;
        bool click_ended = false;

        ImVec2 node_initial_pos_cs = { 0, 0 };
        ImVec2 initial_click_pos_ws = { 0, 0 };
        ImVec2 canvas_clickpos_cs = { 0, 0 };
        ImVec2 canvas_clicked_pixel_offset_ws = { 0, 0 };
        ImVec2 prevDrag = { 0, 0 };
        ImVec2 mouse_ws = { 0, 0 };
        ImVec2 mouse_cs = { 0, 0 };
    }
    g_mouse;

    struct EditState
    {
        entt::entity connection = entt::null;
        entt::entity pin = entt::null;
        entt::entity node = entt::null;

        entt::entity device_node = entt::null;

        float pin_float = 0;
        int   pin_int = 0;
        bool  pin_bool = false;
    }
    g_edit;


    struct HoverState
    {
        void reset(entt::entity en)
        {
            node_id = en;
            originating_pin_id = en;
            pin_id = en;
            pin_label_id = en;
            node_menu = false;
            bang = false;
            play = false;
        }

        entt::entity node_id = entt::null;
        entt::entity pin_id = entt::null;
        entt::entity pin_label_id = entt::null;
        entt::entity connection_id = entt::null;
        entt::entity originating_pin_id = entt::null;
        bool node_menu = false;
        bool bang = false;
        bool play = false;
    } 
    g_hover;



    enum class WorkType
    {
        Nop, CreateRuntimeContext, CreateNode, DeleteNode, SetParam,
        SetFloatSetting, SetIntSetting, SetBoolSetting, SetBusSetting,
        ConnectBusOutToBusIn, ConnectBusOutToParamIn,
        DisconnectInFromOut,
        Start, Bang,
    };


    struct Work
    {
        lab::Sound::Provider& provider;
        WorkType type = WorkType::Nop;
        std::string name;
        entt::entity input_node = entt::null;
        entt::entity output_node = entt::null;
        entt::entity param_pin = entt::null;
        entt::entity setting_pin = entt::null;
        entt::entity connection_id = entt::null;
        float float_value = 0.f;
        int int_value = 0;
        bool bool_value = false;
        ImVec2 canvas_pos = { 0, 0 };

        Work() = delete;
        ~Work() = default;

        explicit Work(lab::Sound::Provider& provider)
            : provider(provider)
        {
            output_node = input_node;
            param_pin = input_node;
            setting_pin = input_node;
            connection_id = input_node;
        }

        void eval()
        {
            entt::registry& registry = provider.registry();

            lab::Sound::Work work;

            switch (type)
            {
            case WorkType::CreateRuntimeContext:
            {
                lab::Sound::Work work;
                work.type = lab::Sound::WorkType::CreateRuntimeContext;
                g_edit.device_node = work.eval();
                registry.assign<GraphNodeLayout>(g_edit.device_node, GraphNodeLayout{ canvas_pos });
                registry.assign<Name>(g_edit.device_node, unique_name("Device"));
                break;
            }
            case WorkType::CreateNode:
            {
                lab::Sound::Work work;
                work.type = lab::Sound::WorkType::CreateNode;
                work.name = name;
                entt::entity new_node = work.eval();
                registry.assign<GraphNodeLayout>(new_node, GraphNodeLayout{ canvas_pos });
                registry.assign<Name>(new_node, unique_name(name));
                break;
            }
            case WorkType::SetParam:
            {
                lab::Sound::Work work;
                work.type = lab::Sound::WorkType::SetParam;
                work.pin_id = param_pin;
                work.float_value = float_value;
                work.eval();
                break;
            }
            case WorkType::SetFloatSetting:
            {
                lab::Sound::Work work;
                work.type = lab::Sound::WorkType::SetFloatSetting;
                work.pin_id = setting_pin;
                work.float_value = float_value;
                work.eval();
                break;
            }
            case WorkType::SetIntSetting:
            {
                lab::Sound::Work work;
                work.type = lab::Sound::WorkType::SetIntSetting;
                work.pin_id = setting_pin;
                work.int_value = int_value;
                work.eval();
                break;
            }
            case WorkType::SetBoolSetting:
            {
                lab::Sound::Work work;
                work.type = lab::Sound::WorkType::SetBoolSetting;
                work.pin_id = setting_pin;
                work.bool_value = bool_value;
                work.eval();
                break;
            }
            case WorkType::SetBusSetting:
            {
                lab::Sound::Work work;
                work.type = lab::Sound::WorkType::SetBusSetting;
                work.name = name;
                work.pin_id = setting_pin;
                work.eval();
                break;
            }
            case WorkType::ConnectBusOutToBusIn:
            {
                lab::Sound::Work work;
                work.type = lab::Sound::WorkType::ConnectBusOutToBusIn;
                work.input_node_id = input_node;
                work.output_node_id = output_node;
                work.eval();
                break;
            }
            case WorkType::ConnectBusOutToParamIn:
            {
                lab::Sound::Work work;
                work.type = lab::Sound::WorkType::ConnectBusOutToParamIn;
                work.pin_id = param_pin;
                work.output_node_id = output_node;
                work.eval();
                break;
            }
            case WorkType::DisconnectInFromOut:
            {
                lab::Sound::Work work;
                work.type = lab::Sound::WorkType::DisconnectInFromOut;
                work.connection_id = connection_id;
                work.eval();
                break;
            }
            case WorkType::DeleteNode:
            {
                lab::Sound::Work work;
                work.type = lab::Sound::WorkType::DeleteNode;
                work.input_node_id = input_node;
                work.output_node_id = output_node;
                work.eval();
                break;
            }
            case WorkType::Start:
            {
                lab::Sound::Work work;
                work.type = lab::Sound::WorkType::Start;
                work.input_node_id = input_node;
                work.eval();
                break;
            }
            case WorkType::Bang:
            {
                lab::Sound::Work work;
                work.type = lab::Sound::WorkType::Bang;
                work.input_node_id = input_node;
                work.eval();
                break;
            }
            }
        }
    };

    vector<Work> g_pending_work;



    bool NoodleMenu(lab::Sound::Provider& provider, ImVec2 canvas_pos)
    {
        static bool result = false;
        static ImGuiID id = ImGui::GetID(&result);
        result = false;
        if (ImGui::BeginPopupContextWindow())
        {
            ImGui::PushID(id);
            result = ImGui::BeginMenu("Create Node");
            if (result)
            {
                std::string pressed = "";
                char const* const* nodes = lab::AudioNodeNames();
                for (; *nodes != nullptr; ++nodes)
                {
                    std::string n(*nodes);
                    n += "###Create";
                    if (ImGui::MenuItem(n.c_str()))
                    {
                        pressed = *nodes;
                    }
                }
                ImGui::EndMenu();
                if (pressed.size() > 0)
                {
                    Work work(provider);
                    work.type = WorkType::CreateNode;
                    work.canvas_pos = canvas_pos;
                    work.name = pressed;
                    g_pending_work.push_back(work);
                }
            }
            ImGui::PopID();
            ImGui::EndPopup();
        }
        return result;
    }


    // This Icon drawing is adapted from thedmd's imgui_node_editor blueprint sample because the icons are nice

    enum class IconType { Flow, Circle, Square, Grid, RoundSquare, Diamond };

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


    static bool imgui_knob(const char* label, float* p_value, float v_min, float v_max)
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


    void EditPin(lab::Sound::Provider& provider, entt::entity pin_id)
    {
        entt::registry& registry = provider.registry();
        if (!registry.valid(pin_id))
            return;

        lab::Sound::AudioPin& pin = registry.get<lab::Sound::AudioPin>(pin_id);
        if (!registry.valid(pin.node_id))
            return;

        lab::Sound::AudioNode& node = registry.get<lab::Sound::AudioNode>(pin.node_id);

        char buff[256];
        sprintf(buff, "%s:%s", node.name.c_str(), pin.name.c_str());

        ImGui::OpenPopup(buff);
        if (ImGui::BeginPopupModal(buff, nullptr, ImGuiWindowFlags_NoCollapse))
        {

            bool accept = false;
            char const* const* names = nullptr;

            lab::AudioSetting::Type type = lab::AudioSetting::Type::None;
            if (pin.kind == lab::Sound::AudioPin::Kind::Setting)
                type = pin.setting->type();

            if (pin.kind == lab::Sound::AudioPin::Kind::Param || type == lab::AudioSetting::Type::Float)
            {
                if (ImGui::InputFloat("###EditPinParamFloat", &g_edit.pin_float,
                    0, 0, 5,
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsScientific))
                {
                    accept = true;
                }
            }
            else if (type == lab::AudioSetting::Type::Integer)
            {
                if (ImGui::InputInt("###EditPinParamInt", &g_edit.pin_int,
                    0, 0,
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsScientific))
                {
                    accept = true;
                }
            }
            else if (type == lab::AudioSetting::Type::Bool)
            {
                if (ImGui::Checkbox("###EditPinParamBool", &g_edit.pin_bool))
                {
                    accept = true;
                }
            }
            else if (type == lab::AudioSetting::Type::Enumeration)
            {
                names = pin.setting->enums();
                int enum_idx = g_edit.pin_int;
                if (ImGui::BeginMenu(names[enum_idx]))
                {
                    std::string pressed = "";
                    enum_idx = 0;
                    int clicked = -1;
                    for (char const* const* names_p = names; *names_p != nullptr; ++names_p, ++enum_idx)
                    {
                        if (ImGui::MenuItem(*names_p))
                        {
                            clicked = enum_idx;
                        }
                    }
                    ImGui::EndMenu();
                    if (clicked >= 0)
                    {
                        g_edit.pin_int = clicked;
                        accept = true;
                    }
                }
            }
            else if (type == lab::AudioSetting::Type::Bus)
            {
                if (ImGui::Button("Load Audio File..."))
                {
                    const char* file = noc_file_dialog_open(NOC_FILE_DIALOG_OPEN, "*.*", ".", "*.*");
                    if (file)
                    {
                        Work work(provider);
                        work.setting_pin = pin_id;
                        work.name.assign(file);
                        work.type = WorkType::SetBusSetting;
                        g_pending_work.emplace_back(work);
                        g_edit.pin = entt::null;
                    }
                }
            }

            if ((type != lab::AudioSetting::Type::Bus) && (accept || ImGui::Button("OK")))
            {
                Work work(provider);
                work.param_pin = pin_id;
                work.setting_pin = pin_id;
                buff[0] = '\0'; // clear the string

                if (pin.kind == lab::Sound::AudioPin::Kind::Param)
                {
                    sprintf(buff, "%f", g_edit.pin_float);
                    work.type = WorkType::SetParam;
                    work.float_value = g_edit.pin_float;
                }
                else if (type == lab::AudioSetting::Type::Float)
                {
                    sprintf(buff, "%f", g_edit.pin_float);
                    work.type = WorkType::SetFloatSetting;
                    work.float_value = g_edit.pin_float;
                }
                else if (type == lab::AudioSetting::Type::Integer)
                {
                    sprintf(buff, "%d", g_edit.pin_int);
                    work.type = WorkType::SetIntSetting;
                    work.int_value = g_edit.pin_int;
                }
                else if (type == lab::AudioSetting::Type::Bool)
                {
                    sprintf(buff, "%s", g_edit.pin_bool ? "True" : "False");
                    work.type = WorkType::SetBoolSetting;
                    work.bool_value = g_edit.pin_bool;
                }
                else if (type == lab::AudioSetting::Type::Enumeration)
                {
                    sprintf(buff, "%s", names[g_edit.pin_int]);
                    work.type = WorkType::SetIntSetting;
                    work.int_value = g_edit.pin_int;
                }

                pin.value_as_string.assign(buff);

                g_pending_work.emplace_back(work);
                g_edit.pin = entt::null;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                g_edit.pin = entt::null;

            ImGui::EndPopup();
        }
    }

    void EditConnection(lab::Sound::Provider& provider, entt::entity connection)
    {
        entt::registry& reg = provider.registry();
        if (!reg.valid(connection))
        {
            g_edit.connection = entt::null;
            return;
        }
        if (!reg.any<lab::Sound::Connection>(connection))
        {
            printf("No GraphConnection for %d", connection);
            g_edit.connection = entt::null;
            return;
        }

        lab::Sound::Connection& conn = reg.get<lab::Sound::Connection>(connection);

        ImGui::OpenPopup("Connection");
        if (ImGui::BeginPopupModal("Connection", nullptr, ImGuiWindowFlags_NoCollapse))
        {
            if (ImGui::Button("Delete"))
            {
                Work work(provider);
                work.type = WorkType::DisconnectInFromOut;
                work.connection_id = connection;
                g_pending_work.emplace_back(work);
                g_edit.connection = entt::null;
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                g_edit.connection = entt::null;

            ImGui::EndPopup();
        }
    }

    void EditNode(lab::Sound::Provider& provider, entt::entity node)
    {
        entt::registry& reg = provider.registry();
        if (!reg.valid(node) || !reg.any<shared_ptr<lab::AudioNode>>(node))
        {
            g_edit.node = entt::null;
            return;
        }

        shared_ptr<lab::AudioNode> node_it = reg.get<shared_ptr<lab::AudioNode>>(node);

        char buff[256];
        sprintf(buff, "%s Node", node_it->name());
        ImGui::OpenPopup(buff);
        if (ImGui::BeginPopupModal(buff, nullptr, ImGuiWindowFlags_NoCollapse))
        {
            if (ImGui::Button("Delete"))
            {
                Work work(provider);
                work.type = WorkType::DeleteNode;
                work.input_node = node;
                g_pending_work.emplace_back(work);
                g_edit.node = entt::null;
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                g_edit.node = entt::null;

            ImGui::EndPopup();
        }
    }





    void DrawSpectrum(lab::Sound::Provider& provider, std::shared_ptr<lab::AudioNode> audio_node, ImVec2 ul_ws, ImVec2 lr_ws, float scale, ImDrawList* drawList)
    {
        float rounding = 3;
        ul_ws.x += 5 * scale; ul_ws.y += 5 * scale;
        lr_ws.x = ul_ws.x + (lr_ws.x - ul_ws.x) * 0.5f;
        lr_ws.y -= 5 * scale;
        drawList->AddRect(ul_ws, lr_ws, ImColor(255, 128, 0, 255), rounding, 15, 2);

        int left = static_cast<int>(ul_ws.x + 2 * scale);
        int right = static_cast<int>(lr_ws.x - 2 * scale);
        int pixel_width = right - left;
        lab::AnalyserNode* node = dynamic_cast<lab::AnalyserNode*>(audio_node.get());
        static vector<uint8_t> bins;
        if (bins.size() != pixel_width)
            bins.resize(pixel_width);

        // fetch the byte frequency data because it is normalized vs the analyser's min/maxDecibels.
        node->getByteFrequencyData(bins, true);

        float base = lr_ws.y - 2 * scale;
        float height = lr_ws.y - ul_ws.y - 4 * scale;
        drawList->PathClear();
        for (int x = 0; x < pixel_width; ++x)
        {
            drawList->PathLineTo(ImVec2(x + float(left), base - height * bins[x] / 255.0f));
        }
        drawList->PathStroke(ImColor(255, 255, 0, 255), false, 2);
    }





    // GraphEditor stuff

    void init(lab::Sound::Provider& provider)
    {
        static bool once = true;
        if (!once)
            return;

        once = false;
        g_ent_main_window = provider.registry().create();
        g_id_main_window = ImGui::GetID(&g_ent_main_window);
        g_bg_color = ImColor(0.2f, 0.2f, 0.2f, 1.0f);
        g_id_main_invis = ImGui::GetID(&g_id_main_invis);
        g_hover.reset(entt::null);
        g_edit.pin = entt::null;
        g_edit.connection = entt::null;
        g_edit.node = entt::null;

        ImGuiWindow* win = ImGui::GetCurrentWindow();
        ImRect edit_rect = win->ContentRegionRect;
        float y = (edit_rect.Max.y + edit_rect.Min.y) * 0.5f - 64;
        Work work(provider);
        work.type = WorkType::CreateRuntimeContext;
        work.name = "AudioDestination";
        work.canvas_pos = ImVec2{ edit_rect.Max.x - 300, y };
        g_pending_work.push_back(work);
    }

    void update_mouse_state(lab::Sound::Provider& provider)
    {
        //---------------------------------------------------------------------
        // determine hovered, dragging, pressed, and released, as well as
        // window local coordinate and canvas local coordinate

        ImGuiIO& io = ImGui::GetIO();
        ImGuiWindow* win = ImGui::GetCurrentWindow();
        ImRect edit_rect = win->ContentRegionRect;

        g_mouse.click_ended = false;
        g_mouse.click_initiated = false;
        g_mouse.in_canvas = false;
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
        {
            ImGui::SetCursorScreenPos(edit_rect.Min);
            ImGui::PushID(g_id_main_invis);
            bool result = ImGui::InvisibleButton("Noodle", edit_rect.GetSize());
            ImGui::PopID();
            if (result)
            {
                //printf("button released\n");
                g_mouse.dragging_node = false;
                g_mouse.click_ended = true;
            }
            g_mouse.in_canvas = ImGui::IsItemHovered();

            if (g_mouse.in_canvas)
            {
                g_mouse.mouse_ws = io.MousePos - ImGui::GetCurrentWindow()->Pos;
                g_mouse.mouse_cs = (g_mouse.mouse_ws - g_canvas.origin_offset_ws) / g_canvas.scale;

                if (io.MouseDown[0] && io.MouseDownOwned[0])
                {
                    if (!g_mouse.dragging)
                    {
                        //printf("button clicked\n");
                        g_mouse.click_initiated = true;
                        g_mouse.initial_click_pos_ws = io.MousePos;
                        g_mouse.canvas_clickpos_cs = g_mouse.mouse_cs;
                        g_mouse.canvas_clicked_pixel_offset_ws = g_canvas.origin_offset_ws;
                    }

                    g_mouse.dragging = true;
                }
                else
                    g_mouse.dragging = false;
            }
            else
                g_mouse.dragging = false;
        }
        else
            g_mouse.dragging = false;

        if (!g_mouse.dragging)
            g_hover.node_id = entt::null;
    }

    void lay_out_pins(lab::Sound::Provider& provider)
    {
        entt::registry& registry = provider.registry();

        // may the counting begin

        for (auto node_entity : registry.view<GraphNodeLayout>())
        {
            GraphNodeLayout& gnl = registry.get<GraphNodeLayout>(node_entity);
            if (!registry.valid(node_entity))
                continue;

            gnl.in_height = 0;
            gnl.mid_height = 0;
            gnl.out_height = 0;
            gnl.column_count = 1;

            // calculate column heights
            if (registry.any<vector<entt::entity>>(node_entity))
            {
                vector<entt::entity>& pins = registry.get<vector<entt::entity>>(node_entity);
                for (const entt::entity entity : pins)
                {
                    if (!registry.valid(entity))
                        continue;

                    lab::Sound::AudioPin& pin = registry.get<lab::Sound::AudioPin>(entity);
                    if (!registry.valid(pin.node_id))
                        continue;

                    // lazily create the layouts on demand.
                    if (!registry.any<GraphPinLayout>(entity))
                        registry.assign<GraphPinLayout>(entity, GraphPinLayout{});

                    GraphPinLayout& pnl = registry.get<GraphPinLayout>(entity);
                    pnl.node_origin_cs = gnl.pos_cs;

                    switch (pin.kind)
                    {
                    case lab::Sound::AudioPin::Kind::BusIn:
                        gnl.in_height += 1;
                        break;
                    case lab::Sound::AudioPin::Kind::BusOut:
                        gnl.out_height += 1;
                        break;
                    case lab::Sound::AudioPin::Kind::Param:
                        gnl.in_height += 1;
                        break;
                    case lab::Sound::AudioPin::Kind::Setting:
                        gnl.mid_height += 1;
                        break;
                    }
                }
            }

            gnl.column_count += gnl.mid_height > 0 ? 1 : 0;

            int height = gnl.in_height > gnl.mid_height ? gnl.in_height : gnl.mid_height;
            if (gnl.out_height > height)
                height = gnl.out_height;

            float width = GraphNodeLayout::k_column_width * gnl.column_count;
            gnl.lr_cs = gnl.pos_cs + ImVec2{ width, GraphPinLayout::k_height * (1.5f + (float)height) };

            gnl.in_height = 0;
            gnl.mid_height = 0;
            gnl.out_height = 0;

            // assign columns
            if (registry.any<vector<entt::entity>>(node_entity))
            {
                vector<entt::entity>& pins = registry.get<vector<entt::entity>>(node_entity);
                for (const entt::entity entity : pins)
                {
                    if (!registry.valid(entity))
                        continue;

                    lab::Sound::AudioPin& pin = registry.get<lab::Sound::AudioPin>(entity);
                    if (!registry.valid(pin.node_id))
                        continue;

                    GraphPinLayout& pnl = registry.get<GraphPinLayout>(entity);

                    switch (pin.kind)
                    {
                    case lab::Sound::AudioPin::Kind::BusIn:
                        pnl.column_number = 0;
                        pnl.pos_y_cs = 16 + GraphPinLayout::k_height * static_cast<float>(gnl.in_height);
                        gnl.in_height += 1;
                        break;
                    case lab::Sound::AudioPin::Kind::BusOut:
                        pnl.column_number = static_cast<float>(gnl.column_count);
                        pnl.pos_y_cs = 16 + GraphPinLayout::k_height * static_cast<float>(gnl.out_height);
                        gnl.out_height += 1;
                        break;
                    case lab::Sound::AudioPin::Kind::Param:
                        pnl.column_number = 0;
                        pnl.pos_y_cs = 16 + GraphPinLayout::k_height * static_cast<float>(gnl.in_height);
                        gnl.in_height += 1;
                        break;
                    case lab::Sound::AudioPin::Kind::Setting:
                        pnl.column_number = 1;
                        pnl.pos_y_cs = 16 + GraphPinLayout::k_height * static_cast<float>(gnl.mid_height);
                        gnl.mid_height += 1;
                        break;
                    }
                }
            }
        }
    }


    void update_hovers(lab::Sound::Provider& provider)
    {
        entt::registry& registry = provider.registry();

        //bool currently_hovered = g_hover.node_id != entt::null;

        // refresh highlights if dragging a wire, or if a node is not being dragged
        bool find_highlights = g_mouse.dragging_wire || !g_mouse.dragging;
        
        if (find_highlights)
        {
            g_hover.bang = false;
            g_hover.play = false;
            g_hover.node_menu = false;
            g_edit.node = entt::null;
            g_hover.node_id = entt::null;
            g_hover.pin_id = entt::null;
            g_hover.pin_label_id = entt::null;
            g_hover.connection_id = entt::null;
            float mouse_x_cs = g_mouse.mouse_cs.x;
            float mouse_y_cs = g_mouse.mouse_cs.y;

            // check all pins
            for (const auto entity : registry.view<lab::Sound::AudioPin>())
            {
                if (!registry.valid(entity))
                {
                    // @TODO slate entity for demolition
                    continue;
                }

                GraphPinLayout& pnl = registry.get<GraphPinLayout>(entity);
                if (pnl.pin_contains_cs_point(g_canvas, mouse_x_cs, mouse_y_cs))
                {
                    lab::Sound::AudioPin& pin = registry.get<lab::Sound::AudioPin>(entity);
                    if (pin.kind == lab::Sound::AudioPin::Kind::Setting)
                    {
                        g_hover.pin_id = entt::null;
                    }
                    else
                    {
                        g_hover.pin_id = entity;
                        g_hover.pin_label_id = entt::null;
                        g_hover.node_id = pin.node_id;
                    }
                }
                else if (pnl.label_contains_cs_point(g_canvas, mouse_x_cs, mouse_y_cs))
                {
                    lab::Sound::AudioPin& pin = registry.get<lab::Sound::AudioPin>(entity);
                    if (pin.kind == lab::Sound::AudioPin::Kind::Setting || pin.kind == lab::Sound::AudioPin::Kind::Param)
                    {
                        g_hover.pin_id = entt::null;
                        g_hover.pin_label_id = entity;
                        g_hover.node_id = pin.node_id;
                    }
                    else
                    {
                        g_hover.pin_label_id = entt::null;
                    }
                }
            }

            // test all nodes
            for (const entt::entity entity : registry.view<lab::Sound::AudioNode>())
            {
                GraphNodeLayout& gnl = registry.get<GraphNodeLayout>(entity);
                ImVec2 ul = gnl.pos_cs;
                ImVec2 lr = gnl.lr_cs;
                if (mouse_x_cs >= ul.x && mouse_x_cs <= (lr.x + GraphPinLayout::k_width) && mouse_y_cs >= (ul.y - 20) && mouse_y_cs <= lr.y)
                {
                    ImVec2 pin_ul;

                    if (mouse_y_cs < ul.y)
                    {
                        float testx = ul.x + 20;
                        bool play = false;
                        bool bang = false;
                        shared_ptr<lab::AudioNode> a_node = registry.get<shared_ptr<lab::AudioNode>>(entity);
                        if (!a_node)
                            continue;

                        bool scheduled = a_node->isScheduledNode();

                        // in banner
                        if (mouse_x_cs < testx && scheduled)
                        {
                            g_hover.play = true;
                            play = true;
                        }

                        if (scheduled)
                            testx += 20;

                        if (!play && mouse_x_cs < testx && a_node->_scheduler._onStart)
                        {
                            g_hover.bang = true;
                            bang = true;
                        }

                        if (!play && !bang)
                        {
                            g_hover.node_menu = true;
                        }
                    }

                    g_hover.node_id = entity;
                    break;
                }
            }

            // test all connections
            if (g_hover.node_id == entt::null)
            {
                // no node or node furniture hovered, check connections
                for (const auto entity : registry.view<lab::Sound::Connection>())
                {
                    lab::Sound::Connection& connection = registry.get<lab::Sound::Connection>(entity);
                    entt::entity from_pin = connection.pin_from;
                    entt::entity to_pin = connection.pin_to;
                    if (!registry.valid(from_pin) || !registry.valid(to_pin))
                        continue;

                    GraphPinLayout& from_gpl = registry.get<GraphPinLayout>(from_pin);
                    GraphPinLayout& to_gpl = registry.get<GraphPinLayout>(to_pin);
                    ImVec2 from_pos = from_gpl.ul_ws(g_canvas) + ImVec2(16, 10) * g_canvas.scale;
                    ImVec2 to_pos = to_gpl.ul_ws(g_canvas) + ImVec2(0, 10) * g_canvas.scale;

                    ImVec2 p0 = from_pos;
                    ImVec2 p3 = to_pos;
                    ImVec2 pd = p0 - p3;
                    float wiggle = std::min(64.f, sqrtf(pd.x * pd.x + pd.y * pd.y)) * g_canvas.scale;
                    ImVec2 p1 = { p0.x + wiggle, p0.y };
                    ImVec2 p2 = { p3.x - wiggle, p3.y };

                    ImVec2 test = g_mouse.mouse_ws + g_canvas.window_origin_offset_ws;
                    //printf("p0(%01.f, %0.1f) p3(%0.1f, %0.1f) m(%01.f, %01.f)\n", p0.x, p0.y, p3.x, p3.y, test.x, test.y);
                    ImVec2 closest = ImBezierClosestPointCasteljau(p0, p1, p2, p3, test, 10);
                    
                    ImVec2 delta = test - closest;
                    float d = delta.x * delta.x + delta.y * delta.y;
                    if (d < 100)
                    {
                        g_hover.connection_id = entity;
                        break;
                    }
                }
            }
        }
    }



    bool run_noodles(RunConfig& config)
    {
        // if the Command is to delete everything, do it first.
        if (config.command == Command::Open)
        {
            const char* file = noc_file_dialog_open(NOC_FILE_DIALOG_OPEN, "*.ls", ".", "*.*");
            if (file)
            {
            }
        }
        if (config.command == Command::Save || (/* file ready to save && */ config.command == Command::New))
        {
            // offer to save
        }
        if (config.command == Command::New)
        {
            // delete everything
        }

        ImGui::BeginChild("###Noodles");
        struct RunWork
        {
            ~RunWork()
            {
                for (Work& work : g_pending_work)
                    work.eval();

                g_pending_work.clear();
                ImGui::EndChild();
            }
        } run_work;

        init(_provider);

        //---------------------------------------------------------------------
        // ensure coordinate systems are initialized properly

        ImGuiIO& io = ImGui::GetIO();
        ImGuiWindow* win = ImGui::GetCurrentWindow();
        ImRect edit_rect = win->ContentRegionRect;
        g_canvas.window_origin_offset_ws = ImGui::GetWindowPos() + ImGui::GetWindowContentRegionMin();

        //---------------------------------------------------------------------
        // ensure node sizes are up to date

        lay_out_pins(_provider);

        //---------------------------------------------------------------------
        // Create a canvas

        float height = edit_rect.Max.y - edit_rect.Min.y
                        - ImGui::GetTextLineHeightWithSpacing()   // space for the time bar
                        - ImGui::GetTextLineHeightWithSpacing();  // space for horizontal scroller

        bool rv = ImGui::BeginChild(g_id_main_window, ImVec2(0, height), false,
                        ImGuiWindowFlags_NoBringToFrontOnFocus |
                        ImGuiWindowFlags_NoMove |
                        ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoScrollWithMouse);

        //---------------------------------------------------------------------
        // draw the grid on the canvas

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->ChannelsSplit(ChannelCount);
        drawList->ChannelsSetCurrent(ChannelContent);
        {
            drawList->ChannelsSetCurrent(ChannelGrid);
            ImU32 grid_color = ImColor(1.f, std::max(1.f, g_canvas.scale * g_canvas.scale), 0.f, 0.25f * g_canvas.scale);
            float GRID_SX = 128.0f * g_canvas.scale;
            float GRID_SY = 128.0f * g_canvas.scale;
            ImVec2 VIEW_POS = ImGui::GetWindowPos();
            ImVec2 VIEW_SIZE = ImGui::GetWindowSize();

            drawList->AddRectFilled(VIEW_POS, VIEW_POS + VIEW_SIZE, g_bg_color);

            for (float x = fmodf(g_canvas.origin_offset_ws.x, GRID_SX); x < VIEW_SIZE.x; x += GRID_SX)
            {
                drawList->AddLine(ImVec2(x, 0.0f) + VIEW_POS, ImVec2(x, VIEW_SIZE.y) + VIEW_POS, grid_color);
            }
            for (float y = fmodf(g_canvas.origin_offset_ws.y, GRID_SY); y < VIEW_SIZE.y; y += GRID_SY)
            {
                drawList->AddLine(ImVec2(0.0f, y) + VIEW_POS, ImVec2(VIEW_SIZE.x, y) + VIEW_POS, grid_color);
            }
        }
        drawList->ChannelsSetCurrent(ChannelContent);

        //---------------------------------------------------------------------
        // run the Imgui portion of the UI

        update_mouse_state(_provider);

        if (g_mouse.dragging || g_mouse.dragging_wire || g_mouse.dragging_node)
        {
            if (g_mouse.dragging_wire)
                update_hovers(_provider);
        }
        else
        {
            bool imgui_business = NoodleMenu(_provider, g_mouse.mouse_cs);
            if (imgui_business)
            {
                g_mouse.dragging = false;
                g_hover.node_id = entt::null;
                g_hover.reset(entt::null);
                g_edit.pin = entt::null;
            }
            else if (g_edit.pin != entt::null)
            {
                ImGui::SetNextWindowPos(g_mouse.initial_click_pos_ws);
                EditPin(_provider, g_edit.pin);
                g_mouse.dragging = false;
                g_hover.node_id = entt::null;
                g_hover.reset(entt::null);
            }
            else if (g_edit.connection != entt::null)
            {
                ImGui::SetNextWindowPos(g_mouse.initial_click_pos_ws);
                EditConnection(_provider, g_edit.connection);
                g_mouse.dragging = false;
                g_hover.node_id = entt::null;
                g_hover.reset(entt::null);
            }
            else if (g_edit.node != entt::null)
            {
                ImGui::SetNextWindowPos(g_mouse.initial_click_pos_ws);
                EditNode(_provider, g_edit.node);
                g_mouse.dragging = false;
            }
            else
                update_hovers(_provider);
        }

        entt::registry& registry = _provider.registry();

        if (!g_mouse.dragging && g_mouse.dragging_wire)
        {
            if (g_hover.originating_pin_id != entt::null && g_hover.pin_id != entt::null && registry.valid(g_hover.originating_pin_id) && registry.valid(g_hover.pin_id))
            {
                GraphPinLayout& from_gpl = registry.get<GraphPinLayout>(g_hover.originating_pin_id);
                GraphPinLayout& to_gpl = registry.get<GraphPinLayout>(g_hover.pin_id);

                lab::Sound::AudioPin from_pin = registry.get<lab::Sound::AudioPin>(g_hover.originating_pin_id);
                lab::Sound::AudioPin to_pin = registry.get<lab::Sound::AudioPin>(g_hover.pin_id);

                /// @TODO this logic can be duplicated into hover to turn the wire red for disallowed connections
                // ensure to is the connection destination
                if (from_pin.kind == lab::Sound::AudioPin::Kind::BusIn || from_pin.kind == lab::Sound::AudioPin::Kind::Param)
                {
                    std::swap(to_pin, from_pin);
                    std::swap(g_hover.originating_pin_id, g_hover.pin_id);
                }

                // check if a valid connection is requested
                lab::Sound::AudioPin::Kind to_kind = to_pin.kind;
                lab::Sound::AudioPin::Kind from_kind = from_pin.kind;
                if (to_kind == lab::Sound::AudioPin::Kind::Setting || to_kind == lab::Sound::AudioPin::Kind::BusOut ||
                    from_kind == lab::Sound::AudioPin::Kind::BusIn || from_kind == lab::Sound::AudioPin::Kind::Param || from_kind == lab::Sound::AudioPin::Kind::Setting)
                {
                    printf("invalid connection request\n");
                }
                else
                {
                    if (to_kind == lab::Sound::AudioPin::Kind::BusIn)
                    {
                        Work work(_provider);
                        work.type = WorkType::ConnectBusOutToBusIn;
                        work.input_node = to_pin.node_id;
                        work.output_node = from_pin.node_id;
                        g_pending_work.emplace_back(work);
                    }
                    else if (to_kind == lab::Sound::AudioPin::Kind::Param)
                    {
                        Work work(_provider);
                        work.type = WorkType::ConnectBusOutToParamIn;
                        work.input_node = to_pin.node_id;
                        work.output_node = from_pin.node_id;
                        work.param_pin = to_pin.pin_id;
                        g_pending_work.emplace_back(work);
                    }
                }
            }
            g_mouse.dragging_wire = false;
            g_hover.originating_pin_id = entt::null;
        }
        else if (g_mouse.click_ended)
        {
            if (g_hover.connection_id != entt::null)
            {
                g_edit.connection = g_hover.connection_id;
            }
            else if (g_hover.node_menu)
            {
                g_edit.node = g_hover.node_id;
            }
        }

        g_mouse.interacting_with_canvas = g_hover.node_id == entt::null && !g_mouse.dragging_wire;
        if (!g_mouse.interacting_with_canvas)
        {
            // nodes and wires
            if (g_mouse.click_initiated)
            {
                if (g_hover.bang)
                {
                    Work work(_provider);
                    work.type = WorkType::Bang;
                    work.input_node = g_hover.node_id;
                    g_pending_work.push_back(work);
                }
                if (g_hover.play)
                {
                    Work work(_provider);
                    work.type = WorkType::Start;
                    work.input_node = g_hover.node_id;
                    g_pending_work.push_back(work);
                }
                if (g_hover.pin_id != entt::null)
                {
                    g_mouse.dragging_wire = true;
                    g_mouse.dragging_node = false;
                    g_mouse.node_initial_pos_cs = g_mouse.mouse_cs;
                    if (g_hover.originating_pin_id == entt::null)
                        g_hover.originating_pin_id = g_hover.pin_id;
                }
                else if (g_hover.pin_label_id != entt::null)
                {
                    // set mode to edit the value of the hovered pin
                    g_edit.pin = g_hover.pin_label_id;
                    lab::Sound::AudioPin& pin = registry.get<lab::Sound::AudioPin>(g_edit.pin);
                    if (pin.kind == lab::Sound::AudioPin::Kind::Param)
                    {
                        g_edit.pin_float = pin.param->value();
                    }
                    else if (pin.kind == lab::Sound::AudioPin::Kind::Setting)
                    {
                        auto type = pin.setting->type();
                        if (type == lab::AudioSetting::Type::Float)
                        {
                            g_edit.pin_float = pin.setting->valueFloat();
                        }
                        else if (type == lab::AudioSetting::Type::Integer || type == lab::AudioSetting::Type::Enumeration)
                        {
                            g_edit.pin_int = pin.setting->valueUint32();
                        }
                        else if (type == lab::AudioSetting::Type::Bool)
                        {
                            g_edit.pin_bool = pin.setting->valueBool();
                        }
                    }
                }
                else
                {
                    g_mouse.dragging_wire = false;
                    g_mouse.dragging_node = true;

                    GraphNodeLayout& gnl = registry.get<GraphNodeLayout>(g_hover.node_id);
                    g_mouse.node_initial_pos_cs = gnl.pos_cs;
                }
            }

            if (g_mouse.dragging_node)
            {
                ImVec2 delta = g_mouse.mouse_cs - g_mouse.canvas_clickpos_cs;

                GraphNodeLayout& gnl = registry.get<GraphNodeLayout>(g_hover.node_id);
                ImVec2 sz = gnl.lr_cs - gnl.pos_cs;
                gnl.pos_cs = g_mouse.node_initial_pos_cs + delta;
                gnl.lr_cs = gnl.pos_cs + sz;

                /// @TODO force the color to be highlighting
            }
        }
        else
        {
            // if the interaction is with the canvas itself, offset and scale the canvas
            if (!g_mouse.dragging_wire)
            {
                if (g_mouse.dragging)
                {
                    if (fabsf(io.MouseDelta.x) > 0.f || fabsf(io.MouseDelta.y) > 0.f)
                    {
                        // pull the pivot around
                        g_canvas.origin_offset_ws = g_mouse.canvas_clicked_pixel_offset_ws - g_mouse.initial_click_pos_ws + io.MousePos;
                    }
                }
                else if (g_mouse.in_canvas)
                {
                    if (fabsf(io.MouseWheel) > 0.f)
                    {
                        // scale using where the mouse is currently hovered as the pivot
                        float prev_scale = g_canvas.scale;
                        g_canvas.scale += io.MouseWheel * 0.1f;
                        g_canvas.scale = g_canvas.scale < 1.f / 16.f ? 1.f / 16.f : g_canvas.scale;
                        g_canvas.scale = g_canvas.scale > 1.f ? 1.f : g_canvas.scale;

                        // solve for off2
                        // (mouse - off1) / scale1 = (mouse - off2) / scale2 
                        g_canvas.origin_offset_ws = g_mouse.mouse_ws - (g_mouse.mouse_ws - g_canvas.origin_offset_ws) * (g_canvas.scale / prev_scale);
                    }
                }
            }
        }

        //---------------------------------------------------------------------
        // draw graph

        static float pulse = 0.f;
        pulse += io.DeltaTime;
        if (pulse > 6.28f)
            pulse -= 6.28f;

        uint32_t text_color = 0xffffff;
        text_color |= (uint32_t)(255 * 2 * (g_canvas.scale - 0.5f)) << 24;

        // draw pulled noodle

        drawList->ChannelsSetCurrent(ChannelNodes);

        for (const auto entity : registry.view<lab::Sound::Connection>())
        {
            lab::Sound::Connection& i = registry.get<lab::Sound::Connection>(entity);
            entt::entity from_pin = i.pin_from;
            entt::entity to_pin = i.pin_to;
            lab::Sound::AudioPin& from_it = registry.get<lab::Sound::AudioPin>(from_pin);
            lab::Sound::AudioPin& to_it = registry.get<lab::Sound::AudioPin>(to_pin);

            GraphPinLayout& from_gpl = registry.get<GraphPinLayout>(from_pin);
            GraphPinLayout& to_gpl = registry.get<GraphPinLayout>(to_pin);
            ImVec2 from_pos = from_gpl.ul_ws(g_canvas) + ImVec2(16, 10) * g_canvas.scale;
            ImVec2 to_pos = to_gpl.ul_ws(g_canvas) + ImVec2(0, 10) * g_canvas.scale;

            ImVec2 p0 = from_pos;
            ImVec2 p3 = to_pos;
            ImVec2 pd = p0 - p3;
            float wiggle = std::min(64.f, sqrtf(pd.x * pd.x + pd.y * pd.y)) * g_canvas.scale;
            ImVec2 p1 = { p0.x + wiggle, p0.y };
            ImVec2 p2 = { p3.x - wiggle, p3.y };

            ImU32 color = entity == g_hover.connection_id ? 0xff00ffff : 0xffffffff;
            drawList->AddBezierCurve(p0, p1, p2, p3, color, 2.f);
        }

        if (g_mouse.dragging_wire)
        {
            GraphPinLayout& from_gpl = registry.get<GraphPinLayout>(g_hover.originating_pin_id);
            ImVec2 p0 = from_gpl.ul_ws(g_canvas) + ImVec2(16, 10) * g_canvas.scale;
            ImVec2 p3 = g_mouse.mouse_ws + g_canvas.window_origin_offset_ws;
            ImVec2 pd = p0 - p3;
            float wiggle = std::min(64.f, sqrtf(pd.x * pd.x + pd.y * pd.y)) * g_canvas.scale;
            ImVec2 p1 = { p0.x + wiggle, p0.y };
            ImVec2 p2 = { p3.x - wiggle, p3.y };
            drawList->AddBezierCurve(p0, p1, p2, p3, 0xffffffff, 2.f);
        }

        // draw nodes
        shared_ptr<lab::AudioNode> dev_node;
        if (g_edit.device_node != entt::null)
            dev_node = registry.get<shared_ptr<lab::AudioNode>>(g_edit.device_node);
        g_total_profile_duration = dev_node ? dev_node->graphTime.microseconds.count() : 0;

        for (auto entity : registry.view<lab::Sound::AudioNode>())
        {
            shared_ptr<lab::AudioNode> node = registry.get<shared_ptr<lab::AudioNode>>(entity);
            if (!node)
            {
                /// @TODO dead pointer, add this entity to an entity reaping list
                continue;
            }

            float node_profile_duration = node->totalTime.microseconds.count() - node->graphTime.microseconds.count();
            node_profile_duration = std::abs(node_profile_duration); /// @TODO, the destination node doesn't yet have a totalTime, so abs is a hack in the nonce

            GraphNodeLayout& gnl = registry.get<GraphNodeLayout>(entity);
            ImVec2 ul_ws = gnl.pos_cs;
            ImVec2 lr_ws = gnl.lr_cs;
            ul_ws = g_canvas.window_origin_offset_ws + ul_ws * g_canvas.scale + g_canvas.origin_offset_ws;
            lr_ws = g_canvas.window_origin_offset_ws + lr_ws * g_canvas.scale + g_canvas.origin_offset_ws;

            // draw node

            const float rounding = 10;
            drawList->AddRectFilled(ul_ws, lr_ws, ImColor(10, 20, 30, 128), rounding);
            drawList->AddRect(ul_ws, lr_ws, ImColor(255, g_hover.node_id == entity ? 255 : 0, 0, 255), rounding, 15, 2);

            if (config.show_profiler)
            {
                ImVec2 p1{ ul_ws.x, lr_ws.y };
                ImVec2 p2{ lr_ws.x, lr_ws.y + g_canvas.scale * 16 };
                drawList->AddRect(p1, p2, ImColor(128, 255, 128, 255));
                p2.x = p1.x + (p2.x - p1.x) * node_profile_duration / g_total_profile_duration;
                drawList->AddRectFilled(p1, p2, ImColor(255, 255, 255, 128));
            }

            if (!strncmp(node->name(), "Analyser", 8))
            {
                DrawSpectrum(_provider, node, ul_ws, lr_ws, g_canvas.scale, drawList);
            }

            // node banner

            if (g_canvas.scale > 0.5f)
            {
                float font_size = 16 * g_canvas.scale;
                ImVec2 label_pos = ul_ws;
                label_pos.y -= 20 * g_canvas.scale;

                if (node->isScheduledNode())
                {
                    const char* label = ">";
                    drawList->AddText(io.FontDefault, font_size, label_pos, text_color, label, label+1);
                    label_pos.x += 20;
                }
                if (node->_scheduler._onStart)
                {
                    const char* label = "!";
                    drawList->AddText(io.FontDefault, font_size, label_pos, text_color, label, label + 1);
                    label_pos.x += 20;
                }

                Name& name = registry.get<Name>(entity);
                drawList->AddText(io.FontDefault, font_size, label_pos, text_color, 
                                  name.name.c_str(), name.name.c_str() + name.name.size());
            }

            if (registry.any<vector<entt::entity>>(entity))
            {
                vector<entt::entity>& pins = _provider.pins(entity);

                // draw input pins

                for (entt::entity j : pins)
                {
                    lab::Sound::AudioPin& pin_it = registry.get<lab::Sound::AudioPin>(j);

                    lab::noodle::IconType icon_type;
                    bool has_value = false;
                    uint32_t color;
                    switch (pin_it.kind)
                    {
                    case lab::Sound::AudioPin::Kind::BusIn:
                        icon_type = lab::noodle::IconType::Flow;
                        color = 0xffffffff;
                        break;
                    case lab::Sound::AudioPin::Kind::Param:
                        icon_type = lab::noodle::IconType::Flow;
                        has_value = true;
                        color = 0xff00ff00;
                        break;
                    case lab::Sound::AudioPin::Kind::Setting:
                        icon_type = lab::noodle::IconType::Grid;
                        has_value = true;
                        color = 0xff00ff00;
                        break;
                    case lab::Sound::AudioPin::Kind::BusOut:
                        icon_type = lab::noodle::IconType::Flow;
                        color = 0xffffffff;
                        break;
                    }

                    GraphPinLayout& pin_gpl = registry.get<GraphPinLayout>(j);
                    ImVec2 pin_ul = pin_gpl.ul_ws(g_canvas);
                    uint32_t fill = (j == g_hover.pin_id || j == g_hover.originating_pin_id) ? 0xffffff : 0x000000;
                    fill |= (uint32_t)(128 + 128 * sinf(pulse * 8)) << 24;
                    DrawIcon(drawList, pin_ul, 
                        ImVec2{ pin_ul.x + GraphPinLayout::k_width * g_canvas.scale, pin_ul.y + GraphPinLayout::k_height * g_canvas.scale },
                        icon_type, false, color, fill);

                    if (g_canvas.scale > 0.5f)
                    {
                        float font_size = 16 * g_canvas.scale;
                        ImVec2 label_pos = pin_ul;
                        label_pos.x += 20 * g_canvas.scale;
                        drawList->AddText(io.FontDefault, font_size, label_pos, text_color,
                            pin_it.shortName.c_str(), pin_it.shortName.c_str() + pin_it.shortName.length());

                        if (has_value)
                        {
                            label_pos.x += 50 * g_canvas.scale;
                            drawList->AddText(io.FontDefault, font_size, label_pos, text_color,
                                pin_it.value_as_string.c_str(), pin_it.value_as_string.c_str() + pin_it.value_as_string.length());
                        }
                    }

                    pin_ul.y += 20 * g_canvas.scale;
                }
            }
        }

        // finish

        drawList->ChannelsMerge();
        EndChild();

        if (config.show_debug)
        {
            ImGui::Begin("Debug Information");
            ImGui::TextUnformatted("Mouse");
            ImGui::Text("canvas    pos (%d, %d)", (int) g_mouse.mouse_cs.x, (int) g_mouse.mouse_cs.y);
            ImGui::Text("drawlist  pos (%d, %d)", (int) g_mouse.mouse_ws.x, (int) g_mouse.mouse_ws.y);
            ImGui::Text("LMB %s%s%s", g_mouse.click_initiated ? "*" : "-", g_mouse.dragging ? "*" : "-", g_mouse.click_ended ? "*" : "-");
            ImGui::Text("canvas interaction: %s", g_mouse.interacting_with_canvas ? "*" : ".");
            ImGui::Text("wire dragging: %s", g_mouse.dragging_wire ? "*" : ".");

            ImGui::Separator();
            ImGui::TextUnformatted("Canvas");
            ImVec2 off = g_canvas.window_origin_offset_ws;
            ImGui::Text("canvas window offset (%d, %d)", (int) off.x, (int) off.y);
            off = g_canvas.origin_offset_ws;
            ImGui::Text("canvas origin offset (%d, %d)", (int) off.x, (int) off.y);

            ImGui::Separator();
            ImGui::TextUnformatted("Hover");
            ImGui::Text("node hovered: %s", g_hover.node_id != entt::null ? "*" : ".");
            ImGui::Text("originating pin id: %d", g_hover.originating_pin_id);
            ImGui::Text("hovered pin id: %d", g_hover.pin_id);
            ImGui::Text("hovered pin label: %d", g_hover.pin_label_id);
            ImGui::Text("hovered connection: %d", g_hover.connection_id);
            ImGui::Text("hovered node menu: %s", g_hover.node_menu ? "*" : ".");

            ImGui::Separator();
            ImGui::TextUnformatted("Edit");
            ImGui::Text("edit connection: %d", g_edit.connection);
            ImGui::Separator();
            ImGui::Text("quantum time: %f uS", g_total_profile_duration);

            ImGui::End();
        }

        return true;
    }

}} // lab::noodle
