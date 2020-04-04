
#include "lab_noodle.h"

#include <LabSound/LabSound.h>
#include <LabSound/core/AudioNode.h>

#include "LabSoundInterface.h"
#include "lab_imgui_ext.hpp"

#include "nfd.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace lab {
namespace noodle {

    using namespace ImGui;

    static const ImColor node_background_fill = ImColor(10, 20, 30, 128);
    static const ImColor node_outline_hovered = ImColor(231, 92, 60); 
    static const ImColor node_outline_neutral = ImColor(192, 57, 43);

    static const ImColor icon_pin_flow = ImColor(241, 196, 15);
    static const ImColor icon_pin_param = ImColor(192, 57, 43);
    static const ImColor icon_pin_setting = ImColor(192, 57, 43);
    static const ImColor icon_pin_bus_out = ImColor(241, 196, 15);

    static const ImColor grid_line_color = ImColor(189, 195, 199, 128);
    static const ImColor grid_bg_color = ImColor(50, 50, 50, 255);

    static const ImColor noodle_bezier_hovered = ImColor(241, 196, 15, 255);
    static const ImColor noodle_bezier_neutral = ImColor(189, 195, 199, 255);
    static const ImColor noodle_bezier_cancel = ImColor(189, 50, 15, 255);

    static const ImColor text_highlighted = ImColor(231, 92, 60);

    static constexpr float node_border_radius = 4.f;
    static constexpr float style_padding_y = 16.f;    
    static constexpr float style_padding_x = 12.f;

    std::string unique_name(std::string name)
    {
        static std::unordered_map<std::string, int> bases;
        static std::unordered_set<std::string> names;

        size_t pos = name.rfind("-");
        std::string base;

        // no dash, or leading dash, it's not a uniqued name
        if (pos == std::string::npos || pos == 0)
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
        std::string candidate = base + "-" + std::to_string(id);
        while (names.find(candidate) != names.end())
        {
            ++id;
            candidate = base + "-" + std::to_string(id);
        }
        bases[base] = id;
        names.insert(candidate);
        return candidate;
    }

    struct Canvas
    {
        ImVec2 window_origin_offset_ws = { 0, 0 };
        ImVec2 origin_offset_ws = { 0, 0 };
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
        ChannelContent = 0,
        ChannelGrid    = 1,
        ChannelNodes   = 2,
        ChannelCount   = 3
    };

    entt::entity g_ent_main_window;
    ImGuiID g_id_main_window;
    ImGuiID g_id_main_invis;
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
            valid_connection = true;
        }

        entt::entity node_id = entt::null;
        entt::entity pin_id = entt::null;
        entt::entity pin_label_id = entt::null;
        entt::entity connection_id = entt::null;
        entt::entity originating_pin_id = entt::null;
        bool node_menu = false;
        bool bang = false;
        bool play = false;
        bool valid_connection = true;
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
                /// @todo create the entity here, not in the eval, architecturally lab_noodle is responsible
                work.type = lab::Sound::WorkType::CreateRuntimeContext;
                g_edit.device_node = work.eval();
                registry.assign<Node>(g_edit.device_node, Node{});
                registry.assign<GraphNodeLayout>(g_edit.device_node, GraphNodeLayout{ canvas_pos });
                registry.assign<Name>(g_edit.device_node, unique_name("Device"));
                break;
            }
            case WorkType::CreateNode:
            {
                lab::Sound::Work work;
                /// @todo create the entity here, not in the eval, architecturally lab_noodle is responsible
                work.type = lab::Sound::WorkType::CreateNode;
                work.name = name;
                entt::entity new_node = work.eval();
                registry.assign<Node>(new_node, Node{});
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

    std::vector<Work> g_pending_work;



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
                char const* const* nodes = provider.node_names();
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

    void EditPin(lab::Sound::Provider& provider, entt::entity pin_id)
    {
        entt::registry& registry = provider.registry();
        if (!registry.valid(pin_id))
            return;

        Pin& pin = registry.get<Pin>(pin_id);
        if (!registry.valid(pin.node_id))
            return;

        Name& name = registry.get<Name>(pin.node_id);
        Name& pin_name = registry.get<Name>(pin_id);
        char buff[256];
        sprintf(buff, "%s:%s", name.name.c_str(), pin_name.name.c_str());

        ImGui::OpenPopup(buff);
        if (ImGui::BeginPopupModal(buff, nullptr, ImGuiWindowFlags_NoCollapse))
        {
            ImGui::TextUnformatted(pin_name.name.c_str());
            ImGui::Separator();

            bool accept = false;

            if (pin.dataType == Pin::DataType::Float)
            {
                if (ImGui::InputFloat("###EditPinParamFloat", &g_edit.pin_float,
                    0, 0, 5,
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsScientific))
                {
                    accept = true;
                }
            }
            else if (pin.dataType == Pin::DataType::Integer)
            {
                if (ImGui::InputInt("###EditPinParamInt", &g_edit.pin_int,
                    0, 0,
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsScientific))
                {
                    accept = true;
                }
            }
            else if (pin.dataType == Pin::DataType::Bool)
            {
                if (ImGui::Checkbox("###EditPinParamBool", &g_edit.pin_bool))
                {
                    accept = true;
                }
            }
            else if (pin.dataType == Pin::DataType::Enumeration)
            {
                int enum_idx = g_edit.pin_int;
                if (ImGui::BeginMenu(pin.names[enum_idx]))
                {
                    std::string pressed = "";
                    enum_idx = 0;
                    int clicked = -1;
                    for (char const* const* names_p = pin.names; *names_p != nullptr; ++names_p, ++enum_idx)
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
            else if (pin.dataType == Pin::DataType::Bus)
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

            if ((pin.dataType != Pin::DataType::Bus) && (accept || ImGui::Button("OK")))
            {
                Work work(provider);
                work.param_pin = pin_id;
                work.setting_pin = pin_id;
                buff[0] = '\0'; // clear the string

                if (pin.kind == Pin::Kind::Param)
                {
                    sprintf(buff, "%f", g_edit.pin_float);
                    work.type = WorkType::SetParam;
                    work.float_value = g_edit.pin_float;
                }
                else if (pin.dataType == Pin::DataType::Float)
                {
                    sprintf(buff, "%f", g_edit.pin_float);
                    work.type = WorkType::SetFloatSetting;
                    work.float_value = g_edit.pin_float;
                }
                else if (pin.dataType == Pin::DataType::Integer)
                {
                    sprintf(buff, "%d", g_edit.pin_int);
                    work.type = WorkType::SetIntSetting;
                    work.int_value = g_edit.pin_int;
                }
                else if (pin.dataType == Pin::DataType::Bool)
                {
                    sprintf(buff, "%s", g_edit.pin_bool ? "True" : "False");
                    work.type = WorkType::SetBoolSetting;
                    work.bool_value = g_edit.pin_bool;
                }
                else if (pin.dataType == Pin::DataType::Enumeration)
                {
                    if (pin.names)
                        sprintf(buff, "%s", pin.names[g_edit.pin_int]);

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
        if (!reg.any<lab::noodle::Connection>(connection))
        {
            printf("No GraphConnection for %d", connection);
            g_edit.connection = entt::null;
            return;
        }

        lab::noodle::Connection& conn = reg.get<lab::noodle::Connection>(connection);

        ImGui::OpenPopup("Connection");
        if (ImGui::BeginPopupModal("Connection", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
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
        if (!reg.valid(node) || !reg.any<std::shared_ptr<lab::AudioNode>>(node))
        {
            g_edit.node = entt::null;
            return;
        }

        std::shared_ptr<lab::AudioNode> node_it = reg.get<std::shared_ptr<lab::AudioNode>>(node);

        char buff[256];
        sprintf(buff, "%s Node", node_it->name());
        ImGui::OpenPopup(buff);

        if (ImGui::BeginPopupModal(buff, nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize))
        {
            ImGui::Dummy({256, style_padding_y});

            if (ImGui::Button("Delete", {ImGui::GetWindowContentRegionWidth(), 24}))
            {
                Work work(provider);
                work.type = WorkType::DeleteNode;
                work.input_node = node;
                g_pending_work.emplace_back(work);
                g_edit.node = entt::null;
            }
            if (ImGui::Button("Cancel", {ImGui::GetWindowContentRegionWidth(), 24}))
            {
                g_edit.node = entt::null;
            }

            ImGui::Dummy({256, style_padding_y});

            ImGui::EndPopup();
        }
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
            if (registry.any<std::vector<entt::entity>>(node_entity))
            {
                std::vector<entt::entity>& pins = registry.get<std::vector<entt::entity>>(node_entity);
                for (const entt::entity entity : pins)
                {
                    if (!registry.valid(entity))
                        continue;

                    Pin pin = registry.get<Pin>(entity);
                    if (!registry.valid(pin.node_id))
                        continue;

                    // lazily create the layouts on demand.
                    if (!registry.any<GraphPinLayout>(entity))
                        registry.assign<GraphPinLayout>(entity, GraphPinLayout{});

                    GraphPinLayout& pnl = registry.get<GraphPinLayout>(entity);
                    pnl.node_origin_cs = gnl.pos_cs;

                    switch (pin.kind)
                    {
                    case Pin::Kind::BusIn:
                        gnl.in_height += 1;
                        break;
                    case Pin::Kind::BusOut:
                        gnl.out_height += 1;
                        break;
                    case Pin::Kind::Param:
                        gnl.in_height += 1;
                        break;
                    case Pin::Kind::Setting:
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
            if (registry.any<std::vector<entt::entity>>(node_entity))
            {
                std::vector<entt::entity>& pins = registry.get<std::vector<entt::entity>>(node_entity);
                for (const entt::entity entity : pins)
                {
                    if (!registry.valid(entity))
                        continue;

                    Pin& pin = registry.get<Pin>(entity);
                    if (!registry.valid(pin.node_id))
                        continue;

                    GraphPinLayout& pnl = registry.get<GraphPinLayout>(entity);

                    switch (pin.kind)
                    {
                    case Pin::Kind::BusIn:
                        pnl.column_number = 0;
                        pnl.pos_y_cs = style_padding_y + GraphPinLayout::k_height * static_cast<float>(gnl.in_height);
                        gnl.in_height += 1;
                        break;
                    case Pin::Kind::BusOut:
                        pnl.column_number = static_cast<float>(gnl.column_count);
                        pnl.pos_y_cs = style_padding_y + GraphPinLayout::k_height * static_cast<float>(gnl.out_height);
                        gnl.out_height += 1;
                        break;
                    case Pin::Kind::Param:
                        pnl.column_number = 0;
                        pnl.pos_y_cs = style_padding_y + GraphPinLayout::k_height * static_cast<float>(gnl.in_height);
                        gnl.in_height += 1;
                        break;
                    case Pin::Kind::Setting:
                        pnl.column_number = 1;
                        pnl.pos_y_cs = style_padding_y + GraphPinLayout::k_height * static_cast<float>(gnl.mid_height);
                        gnl.mid_height += 1;
                        break;
                    }
                }
            }
        }
    }


    void noodle_bezier(ImVec2 & p0, ImVec2 & p1, ImVec2 & p2, ImVec2 & p3)
    {
        if (p0.x > p3.x)
            std::swap(p0, p3);

        ImVec2 pd = p0 - p3;
        float wiggle = std::min(fabsf(pd.x), std::min(64.f, sqrtf(pd.x * pd.x + pd.y * pd.y)) * g_canvas.scale);
        p1 = { p0.x + wiggle, p0.y };
        p2 = { p3.x - wiggle, p3.y };
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
                    Pin& pin = registry.get<Pin>(entity);
                    if (pin.kind == Pin::Kind::Setting)
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
                    Pin& pin = registry.get<Pin>(entity);
                    if (pin.kind == Pin::Kind::Setting || pin.kind == Pin::Kind::Param)
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
            for (const entt::entity entity : registry.view<lab::noodle::Node>())
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
                        std::shared_ptr<lab::AudioNode> a_node = registry.get<std::shared_ptr<lab::AudioNode>>(entity);
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
                for (const auto entity : registry.view<lab::noodle::Connection>())
                {
                    lab::noodle::Connection& connection = registry.get<lab::noodle::Connection>(entity);
                    entt::entity from_pin = connection.pin_from;
                    entt::entity to_pin = connection.pin_to;
                    if (!registry.valid(from_pin) || !registry.valid(to_pin))
                        continue;

                    GraphPinLayout& from_gpl = registry.get<GraphPinLayout>(from_pin);
                    GraphPinLayout& to_gpl = registry.get<GraphPinLayout>(to_pin);
                    ImVec2 from_pos = from_gpl.ul_ws(g_canvas) + ImVec2(style_padding_y, style_padding_x) * g_canvas.scale;
                    ImVec2 to_pos = to_gpl.ul_ws(g_canvas) + ImVec2(0, style_padding_x) * g_canvas.scale;

                    ImVec2 p0 = from_pos;
                    ImVec2 p3 = to_pos;
                    ImVec2 p1, p2;
                    noodle_bezier(p0, p1, p2, p3);
                    ImVec2 test = g_mouse.mouse_ws + g_canvas.window_origin_offset_ws;
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

        float height = edit_rect.Max.y - edit_rect.Min.y;
                        //- ImGui::GetTextLineHeightWithSpacing()   // space for the time bar
                        //- ImGui::GetTextLineHeightWithSpacing();  // space for horizontal scroller

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

            const float grid_step_x = 100.0f * g_canvas.scale;
            const float grid_step_y = 100.0f * g_canvas.scale;
            const ImVec2 grid_origin = ImGui::GetWindowPos();
            const ImVec2 grid_size = ImGui::GetWindowSize();

            drawList->AddRectFilled(grid_origin, grid_origin + grid_size, grid_bg_color);

            for (float x = fmodf(g_canvas.origin_offset_ws.x, grid_step_x); x < grid_size.x; x += grid_step_x)
            {
                drawList->AddLine(ImVec2(x, 0.0f) + grid_origin, ImVec2(x, grid_size.y) + grid_origin, grid_line_color);
            }
            for (float y = fmodf(g_canvas.origin_offset_ws.y, grid_step_y); y < grid_size.y; y += grid_step_y)
            {
                drawList->AddLine(ImVec2(0.0f, y) + grid_origin, ImVec2(grid_size.x, y) + grid_origin, grid_line_color);
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
            {
                update_hovers(_provider);
            }
        }

        entt::registry& registry = _provider.registry();

        /// @TODO consolidate the redundant logic here for testing connections
        if (g_mouse.dragging && g_mouse.dragging_wire)
        {
            // if dragging a wire, check for disallowed connections so they wire can turn red
            g_hover.valid_connection = true;
            if (g_hover.originating_pin_id != entt::null && g_hover.pin_id != entt::null && registry.valid(g_hover.originating_pin_id) && registry.valid(g_hover.pin_id))
            {
                GraphPinLayout& from_gpl = registry.get<GraphPinLayout>(g_hover.originating_pin_id);
                GraphPinLayout& to_gpl = registry.get<GraphPinLayout>(g_hover.pin_id);

                Pin from_pin = registry.get<Pin>(g_hover.originating_pin_id);
                Pin to_pin = registry.get<Pin>(g_hover.pin_id);

                if (from_pin.kind == Pin::Kind::BusIn || from_pin.kind == Pin::Kind::Param)
                {
                    std::swap(to_pin, from_pin);
                }

                // check if a valid connection is requested
                Pin::Kind to_kind = to_pin.kind;
                Pin::Kind from_kind = from_pin.kind;

                g_hover.valid_connection = !(to_kind == Pin::Kind::Setting || to_kind == Pin::Kind::BusOut ||
                    from_kind == Pin::Kind::BusIn || from_kind == Pin::Kind::Param ||
                    from_kind == Pin::Kind::Setting);

                // disallow connecting a node to itself
                g_hover.valid_connection &= from_pin.node_id != to_pin.node_id;
            }
        }
        else if (!g_mouse.dragging && g_mouse.dragging_wire)
        {
            g_hover.valid_connection = true;
            if (g_hover.originating_pin_id != entt::null && g_hover.pin_id != entt::null && registry.valid(g_hover.originating_pin_id) && registry.valid(g_hover.pin_id))
            {
                GraphPinLayout& from_gpl = registry.get<GraphPinLayout>(g_hover.originating_pin_id);
                GraphPinLayout& to_gpl = registry.get<GraphPinLayout>(g_hover.pin_id);

                Pin from_pin = registry.get<Pin>(g_hover.originating_pin_id);
                Pin to_pin = registry.get<Pin>(g_hover.pin_id);

                if (from_pin.kind == Pin::Kind::BusIn || from_pin.kind == Pin::Kind::Param)
                {
                    std::swap(to_pin, from_pin);
                    std::swap(g_hover.originating_pin_id, g_hover.pin_id);
                }

                // check if a valid connection is requested
                Pin::Kind to_kind = to_pin.kind;
                Pin::Kind from_kind = from_pin.kind;

                g_hover.valid_connection = !(to_kind == Pin::Kind::Setting || to_kind == Pin::Kind::BusOut ||
                    from_kind == Pin::Kind::BusIn || from_kind == Pin::Kind::Param ||
                    from_kind == Pin::Kind::Setting);

                // disallow connecting a node to itself
                g_hover.valid_connection &= from_pin.node_id != to_pin.node_id;

                if (!g_hover.valid_connection)
                {
                    printf("invalid connection request\n");
                }
                else
                {
                    if (to_kind == Pin::Kind::BusIn)
                    {
                        Work work(_provider);
                        work.type = WorkType::ConnectBusOutToBusIn;
                        work.input_node = to_pin.node_id;
                        work.output_node = from_pin.node_id;
                        g_pending_work.emplace_back(work);
                    }
                    else if (to_kind == Pin::Kind::Param)
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
                    Pin& pin = registry.get<Pin>(g_edit.pin);
                    if (pin.dataType == Pin::DataType::Float)
                    {
                        g_edit.pin_float = _provider.pin_float_value(g_edit.pin);
                    }
                    else if (pin.dataType == Pin::DataType::Integer || pin.dataType == Pin::DataType::Enumeration)
                    {
                        g_edit.pin_int = _provider.pin_int_value(g_edit.pin);
                    }
                    if (pin.dataType == Pin::DataType::Bool)
                    {
                        g_edit.pin_bool = _provider.pin_bool_value(g_edit.pin);
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
                        g_canvas.scale += std::copysign(0.25f, io.MouseWheel);
                        g_canvas.scale = std::max(g_canvas.scale, 0.25f);

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
        uint32_t text_color_highlighted = 0x00ffff;
        text_color |= (uint32_t)(255 * 2 * (g_canvas.scale - 0.5f)) << 24;
        text_color_highlighted |= (uint32_t)(255 * 2 * (g_canvas.scale - 0.5f)) << 24;

        ///////////////////////////////////////////
        //   Noodles Bezier Lines Curves Pulled  //
        ///////////////////////////////////////////

        drawList->ChannelsSetCurrent(ChannelNodes);

        for (const auto entity : registry.view<lab::noodle::Connection>())
        {
            lab::noodle::Connection& i = registry.get<lab::noodle::Connection>(entity);
            entt::entity from_pin = i.pin_from;
            entt::entity to_pin = i.pin_to;
            lab::Sound::AudioPin& from_it = registry.get<lab::Sound::AudioPin>(from_pin);
            lab::Sound::AudioPin& to_it = registry.get<lab::Sound::AudioPin>(to_pin);

            GraphPinLayout& from_gpl = registry.get<GraphPinLayout>(from_pin);
            GraphPinLayout& to_gpl = registry.get<GraphPinLayout>(to_pin);
            ImVec2 from_pos = from_gpl.ul_ws(g_canvas) + ImVec2(style_padding_y, style_padding_x) * g_canvas.scale;
            ImVec2 to_pos = to_gpl.ul_ws(g_canvas) + ImVec2(0, style_padding_x) * g_canvas.scale;

            ImVec2 p0 = from_pos;
            ImVec2 p3 = to_pos;
            ImVec2 p1, p2;
            noodle_bezier(p0, p1, p2, p3);
            ImU32 color = entity == g_hover.connection_id ? noodle_bezier_hovered : noodle_bezier_neutral;
            drawList->AddBezierCurve(p0, p1, p2, p3, color, 2.f);
        }

        if (g_mouse.dragging_wire)
        {
            GraphPinLayout& from_gpl = registry.get<GraphPinLayout>(g_hover.originating_pin_id);
            ImVec2 p0 = from_gpl.ul_ws(g_canvas) + ImVec2(style_padding_y, style_padding_x) * g_canvas.scale;
            ImVec2 p3 = g_mouse.mouse_ws + g_canvas.window_origin_offset_ws;
            ImVec2 p1, p2;
            noodle_bezier(p0, p1, p2, p3);
            ImU32 color = g_hover.valid_connection ? noodle_bezier_neutral : noodle_bezier_cancel;
            drawList->AddBezierCurve(p0, p1, p2, p3, color, 2.f);
        }

        ///////////////////////////////////////
        //   Node Body / Drawing / Profiler  //
        ///////////////////////////////////////

        std::shared_ptr<lab::AudioNode> dev_node;

        if (!registry.valid(g_edit.device_node))
            g_edit.device_node = entt::null;

        if (g_edit.device_node != entt::null)
            dev_node = registry.get<std::shared_ptr<lab::AudioNode>>(g_edit.device_node);

        g_total_profile_duration = dev_node ? dev_node->graphTime.microseconds.count() : 0;

        for (auto entity : registry.view<lab::noodle::Node>())
        {
            std::shared_ptr<lab::AudioNode> node = registry.get<std::shared_ptr<lab::AudioNode>>(entity);
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
            drawList->AddRectFilled(ul_ws, lr_ws, node_background_fill, node_border_radius);
            drawList->AddRect(ul_ws, lr_ws, (g_hover.node_id == entity) ? node_outline_hovered : node_outline_neutral, node_border_radius, 15, 2);

            if (config.show_profiler)
            {
                ImVec2 p1{ ul_ws.x, lr_ws.y };
                ImVec2 p2{ lr_ws.x, lr_ws.y + g_canvas.scale * style_padding_y };
                drawList->AddRect(p1, p2, ImColor(128, 255, 128, 255));
                p2.x = p1.x + (p2.x - p1.x) * node_profile_duration / g_total_profile_duration;
                drawList->AddRectFilled(p1, p2, ImColor(255, 255, 255, 128));
            }

            if (registry.any<NodeRender>(entity))
            {
                NodeRender& render = registry.get<NodeRender>(entity);
                if (render.render)
                    render.render(entity, { ul_ws.x, ul_ws.y }, { lr_ws.x, lr_ws.y }, g_canvas.scale, drawList);
            }

            ///////////////////////////////////////////
            //   Node Header / Banner / Top / Menu   //
            ///////////////////////////////////////////

            if (g_canvas.scale > 0.5f)
            {
                const float label_font_size = style_padding_y * g_canvas.scale;
                ImVec2 label_pos = ul_ws;
                label_pos.y -= 20 * g_canvas.scale;

                // UI elements
                if (node->isScheduledNode())
                {
                    auto label = std::string(ICON_FAD_PLAY);
                    drawList->AddText(NULL, label_font_size, label_pos, 
                        (g_hover.play && entity == g_hover.node_id) ? text_color_highlighted : text_color,
                        label.c_str(), label.c_str() + label.length());
                    label_pos.x += 20;
                }

                if (node->_scheduler._onStart)
                {
                    auto label = std::string(ICON_FAD_HARDCLIP);
                    drawList->AddText(NULL, label_font_size, label_pos, 
                        (g_hover.bang && entity == g_hover.node_id) ? text_color_highlighted : text_color,
                        label.c_str(), label.c_str() + label.length());
                    label_pos.x += 20;
                }

                // Name
                Name& name = registry.get<Name>(entity);
                label_pos.x += 5;
                drawList->AddText(io.FontDefault, label_font_size, label_pos,
                    (g_hover.node_menu && entity == g_hover.node_id) ? text_color_highlighted : text_color,
                    name.name.c_str(), name.name.c_str() + name.name.size());
            }

            ///////////////////////////////////////////
            //   Node Input Pins / Connection / Pin  //
            ///////////////////////////////////////////

            if (registry.any<std::vector<entt::entity>>(entity))
            {
                std::vector<entt::entity> & pins = _provider.pins(entity);

                for (entt::entity j : pins)
                {
                    Pin& pin_it = registry.get<Pin>(j);

                    IconType icon_type;
                    bool has_value = false;
                    uint32_t color;
                    switch (pin_it.kind)
                    {
                    case Pin::Kind::BusIn:
                        icon_type = IconType::Flow;
                        color = icon_pin_flow;
                        break;
                    case Pin::Kind::Param:
                        icon_type = IconType::Flow;
                        has_value = true;
                        color = icon_pin_param;
                        break;
                    case Pin::Kind::Setting:
                        icon_type = IconType::Grid;
                        has_value = true;
                        color = icon_pin_setting;
                        break;
                    case Pin::Kind::BusOut:
                        icon_type = IconType::Flow;
                        color = icon_pin_bus_out;
                        break;
                    }

                    GraphPinLayout& pin_gpl = registry.get<GraphPinLayout>(j);
                    ImVec2 pin_ul = pin_gpl.ul_ws(g_canvas);
                    uint32_t fill = (j == g_hover.pin_id || j == g_hover.originating_pin_id) ? 0xffffff : 0x000000;
                    fill |= (uint32_t)(128 + 128 * sinf(pulse * 8)) << 24;

                    DrawIcon(drawList, pin_ul, 
                        ImVec2{ pin_ul.x + GraphPinLayout::k_width * g_canvas.scale, pin_ul.y + GraphPinLayout::k_height * g_canvas.scale },
                        icon_type, false, color, fill);

                    // Only draw text if we can likely see it
                    if (g_canvas.scale > 0.5f)
                    {
                        float font_size = style_padding_y * g_canvas.scale;
                        ImVec2 label_pos = pin_ul;

                        label_pos.y += 2;

                        label_pos.x += 20 * g_canvas.scale;
                        drawList->AddText(NULL, font_size, label_pos, text_color,
                            pin_it.shortName.c_str(), pin_it.shortName.c_str() + pin_it.shortName.length());

                        if (has_value)
                        {
                            label_pos.x += 50 * g_canvas.scale;
                            drawList->AddText(NULL, font_size, label_pos, text_color,
                                pin_it.value_as_string.c_str(), pin_it.value_as_string.c_str() + pin_it.value_as_string.length());
                        }
                    }

                    pin_ul.y += 20 * g_canvas.scale;
                }
            }
        }

        // finish

        drawList->ChannelsMerge();
        ImGui::EndChild();

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
