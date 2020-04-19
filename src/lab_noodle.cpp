
#include "lab_noodle.h"

#include <LabSound/LabSound.h>
#include <LabSound/core/AudioNode.h>

#include "LabSoundInterface.h"
#include "lab_imgui_ext.hpp"
#include "legit_profiler.hpp"

#include "nfd.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace lab {
namespace noodle {

    using namespace ImGui;

    static const ImColor node_background_fill = ImColor(10, 20, 30, 128);
    static const ImColor node_outline_hovered = ImColor(231, 102, 72); 
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
        float  scale = 1.f;
    };

    struct CanvasNode
    {
        explicit CanvasNode() = default;
        ~CanvasNode() = default;

        Canvas canvas;
        std::unordered_set<entt::entity> nodes;
        std::unordered_set<entt::entity> groups;
    };

    // ImGui channels for layering the graphics
    enum class Channel : int {
        Content = 0,
        Grid = 1,
        Groups = 2,
        Nodes = 3,
        Count = 4
    };

    struct GraphNodeLayout
    {
        static const float k_column_width;
        Channel channel = Channel::Nodes;
        ImVec2 ul_cs = { 0, 0 };
        ImVec2 lr_cs = { 0, 0 };
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
    };

    struct Work;
    struct EditState
    {
        void edit_pin(lab::noodle::Provider& provider, CanvasNode& root, entt::entity pin_id, std::vector<Work>& pending_work);
        void edit_connection(lab::noodle::Provider& provider, CanvasNode& root, entt::entity connection, std::vector<Work>& pending_work);
        void edit_node(Provider& provider, CanvasNode& root, entt::entity node, std::vector<Work>& pending_work);

        entt::entity selected_connection = entt::null;
        entt::entity selected_pin = entt::null;
        entt::entity selected_node = entt::null;

        entt::entity _device_node = entt::null;

        float pin_float = 0;
        int   pin_int = 0;
        bool  pin_bool = false;

        void incr_work_epoch()
        {
            ++_work_epoch;
        }
        void reset_epochs()
        {
            _save_epoch = 1;
            _work_epoch = 1;
        }
        void clear_epochs()
        {
            _save_epoch = 0;
            _work_epoch = 0;
        }
        void unify_epochs()
        {
            _save_epoch = _work_epoch;
        }
        bool need_saving() const
        {
            return _save_epoch != _work_epoch;
        }

    private:
        int _save_epoch = 0; // zero is reserved for empty
        int _work_epoch = 0; // zero is reserved for empty
    };

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
    };


    enum class WorkType
    {
        Nop, 
        ClearScene, 
        CreateRuntimeContext, 
        CreateGroup,
        CreateNode, DeleteNode, 
        SetParam,
        SetFloatSetting, SetIntSetting, SetBoolSetting, SetBusSetting,
        ConnectBusOutToBusIn, ConnectBusOutToParamIn,
        DisconnectInFromOut,
        Start, Bang,
        ResetSaveWorkEpoch
    };

    struct Work
    {
        Provider& provider;
        CanvasNode& root;
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

        explicit Work(Provider& provider, CanvasNode& root)
            : provider(provider), root(root)
        {
            output_node = input_node;
            param_pin = input_node;
            setting_pin = input_node;
            connection_id = input_node;
        }

        void eval(EditState& edit)
        {
            entt::registry& registry = provider.registry();
            switch (type)
            {
            case WorkType::ResetSaveWorkEpoch:
                edit.reset_epochs();
                break;

            case WorkType::CreateRuntimeContext:
            {
                edit._device_node = provider.registry().create();
                registry.assign<Node>(edit._device_node, Node("Device"));
                provider.create_runtime_context(edit._device_node);
                registry.assign<GraphNodeLayout>(edit._device_node, GraphNodeLayout{ Channel::Nodes, { canvas_pos.x, canvas_pos.y } });
                registry.assign<Name>(edit._device_node, unique_name("Device"));
                root.nodes.insert(edit._device_node);
                edit.incr_work_epoch();
                break;
            }
            case WorkType::CreateNode:
            {
                entt::entity new_node = provider.registry().create();
                registry.assign<Node>(new_node, Node(name));
                provider.node_create(name, new_node);
                registry.assign<GraphNodeLayout>(new_node, GraphNodeLayout{ Channel::Nodes, { canvas_pos.x, canvas_pos.y } });
                registry.assign<Name>(new_node, unique_name(name));
                root.nodes.insert(new_node);
                edit.incr_work_epoch();
                break;
            }
            case WorkType::CreateGroup:
            {
                entt::entity new_node = provider.registry().create();
                registry.assign<Node>(new_node, Node(name));
                registry.assign<GraphNodeLayout>(new_node, GraphNodeLayout{ Channel::Groups, { canvas_pos.x, canvas_pos.y } });
                registry.assign<Name>(new_node, unique_name(name));
                root.groups.insert(new_node);
                edit.incr_work_epoch();
                break;
            }
            case WorkType::SetParam:
            {
                provider.pin_set_float_value(param_pin, float_value);
                edit.incr_work_epoch();
                break;
            }
            case WorkType::SetFloatSetting:
            {
                provider.pin_set_float_value(setting_pin, float_value);
                edit.incr_work_epoch();
                break;
            }
            case WorkType::SetIntSetting:
            {
                provider.pin_set_int_value(setting_pin, int_value);
                edit.incr_work_epoch();
                break;
            }
            case WorkType::SetBoolSetting:
            {
                provider.pin_set_int_value(setting_pin, bool_value);
                edit.incr_work_epoch();
                break;
            }
            case WorkType::SetBusSetting:
            {
                provider.pin_set_bus_from_file(setting_pin, name);
                edit.incr_work_epoch();
                break;
            }
            case WorkType::ConnectBusOutToBusIn:
            {
                provider.connect_bus_out_to_bus_in(output_node, input_node);
                edit.incr_work_epoch();
                break;
            }
            case WorkType::ConnectBusOutToParamIn:
            {
                provider.connect_bus_out_to_param_in(output_node, param_pin);
                edit.incr_work_epoch();
                break;
            }
            case WorkType::DisconnectInFromOut:
            {
                provider.disconnect(connection_id);
                edit.incr_work_epoch();
                break;
            }
            case WorkType::DeleteNode:
            {
                provider.node_delete(input_node);
                edit.incr_work_epoch();
                break;
            }
            case WorkType::Start:
            {
                provider.node_start_stop(input_node, 0.f);
                break;
            }
            case WorkType::Bang:
            {
                provider.node_bang(input_node);
                break;
            }
            case WorkType::ClearScene:
            {
                edit.clear_epochs();
            }
            }
        }
    };

    struct Context::State
    {
        State() : profiler_graph(100)
        {
        }

        ~State() = default;

        void init(Provider& provider);
        void update_mouse_state(Provider& provider);
        void update_hovers(Provider& provider);
        bool context_menu(Provider& provider, ImVec2 canvas_pos);
        void run(Provider& provider, bool show_profiler, bool show_debug);

        legit::ProfilerGraph profiler_graph;
        CanvasNode root;
        MouseState mouse;
        EditState edit;
        HoverState hover;
        std::vector<Work> pending_work;
        std::vector<legit::ProfilerTask> profiler_data;

        float total_profile_duration = 1; // in microseconds
        entt::entity ent_main_window = entt::null;
        ImGuiID main_window_id = 0;
        ImGuiID graph_interactive_region_id = 0;
    };

    bool Context::State::context_menu(Provider& provider, ImVec2 canvas_pos)
    {
        static bool result = false;
        static ImGuiID id = ImGui::GetID(&result);
        result = false;
        if (ImGui::BeginPopupContextWindow())
        {
            ImGui::PushID(id);
            if (ImGui::MenuItem("Create Group Node"))
            {
                Work work(provider, root);
                work.type = WorkType::CreateGroup;
                work.canvas_pos = canvas_pos;
                work.name = "Group";
                pending_work.push_back(work);
            }
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
                    Work work(provider, root);
                    work.type = WorkType::CreateNode;
                    work.canvas_pos = canvas_pos;
                    work.name = pressed;
                    pending_work.push_back(work);
                }
            }
            ImGui::PopID();
            ImGui::EndPopup();
        }
        return result;
    }

    void EditState::edit_pin(lab::noodle::Provider& provider, CanvasNode& root, entt::entity pin_id, std::vector<Work>& pending_work)
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
                if (ImGui::InputFloat("###EditPinParamFloat", &pin_float,
                    0, 0, 5,
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsScientific))
                {
                    accept = true;
                }
            }
            else if (pin.dataType == Pin::DataType::Integer)
            {
                if (ImGui::InputInt("###EditPinParamInt", &pin_int,
                    0, 0,
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsScientific))
                {
                    accept = true;
                }
            }
            else if (pin.dataType == Pin::DataType::Bool)
            {
                if (ImGui::Checkbox("###EditPinParamBool", &pin_bool))
                {
                    accept = true;
                }
            }
            else if (pin.dataType == Pin::DataType::Enumeration)
            {
                int enum_idx = pin_int;
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
                        pin_int = clicked;
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
                        Work work(provider, root);
                        work.setting_pin = pin_id;
                        work.name.assign(file);
                        work.type = WorkType::SetBusSetting;
                        pending_work.emplace_back(work);
                        selected_pin = entt::null;

                        std::string path(file);
                        size_t o = path.rfind('/');
                        if (o != std::string::npos)
                            path = path.substr(++o);
                        else
                        {
                            o = path.rfind('\\');
                            if (o != std::string::npos)
                                path = path.substr(++o);
                        }
                        pin.value_as_string = path;
                    }
                }
            }

            if ((pin.dataType != Pin::DataType::Bus) && (accept || ImGui::Button("OK")))
            {
                Work work(provider, root);
                work.param_pin = pin_id;
                work.setting_pin = pin_id;
                buff[0] = '\0'; // clear the string

                if (pin.kind == Pin::Kind::Param)
                {
                    sprintf(buff, "%f", pin_float);
                    work.type = WorkType::SetParam;
                    work.float_value = pin_float;
                }
                else if (pin.dataType == Pin::DataType::Float)
                {
                    sprintf(buff, "%f", pin_float);
                    work.type = WorkType::SetFloatSetting;
                    work.float_value = pin_float;
                }
                else if (pin.dataType == Pin::DataType::Integer)
                {
                    sprintf(buff, "%d", pin_int);
                    work.type = WorkType::SetIntSetting;
                    work.int_value = pin_int;
                }
                else if (pin.dataType == Pin::DataType::Bool)
                {
                    sprintf(buff, "%s", pin_bool ? "True" : "False");
                    work.type = WorkType::SetBoolSetting;
                    work.bool_value = pin_bool;
                }
                else if (pin.dataType == Pin::DataType::Enumeration)
                {
                    if (pin.names)
                        sprintf(buff, "%s", pin.names[pin_int]);

                    work.type = WorkType::SetIntSetting;
                    work.int_value = pin_int;
                }

                pin.value_as_string.assign(buff);

                pending_work.emplace_back(work);
                selected_pin = entt::null;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                selected_pin = entt::null;

            ImGui::EndPopup();
        }
    }

    void EditState::edit_connection(lab::noodle::Provider& provider, CanvasNode& root, entt::entity connection, std::vector<Work>& pending_work)
    {
        entt::registry& reg = provider.registry();
        if (!reg.valid(connection))
        {
            selected_connection = entt::null;
            return;
        }
        if (!reg.any<lab::noodle::Connection>(connection))
        {
            printf("No GraphConnection for %d", connection);
            selected_connection = entt::null;
            return;
        }

        lab::noodle::Connection& conn = reg.get<lab::noodle::Connection>(connection);

        ImGui::OpenPopup("Connection");
        if (ImGui::BeginPopupModal("Connection", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
        {
            if (ImGui::Button("Delete"))
            {
                Work work(provider, root);
                work.type = WorkType::DisconnectInFromOut;
                work.connection_id = connection;
                pending_work.emplace_back(work);
                selected_connection = entt::null;
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                selected_connection = entt::null;

            ImGui::EndPopup();
        }
    }

    void EditState::edit_node(Provider& provider, CanvasNode& root, entt::entity node, std::vector<Work>& pending_work)
    {
        entt::registry& reg = provider.registry();
        if (!reg.valid(node))
        {
            selected_node = entt::null;
            return;
        }

        Name& name = reg.get<Name>(node);

        char buff[256];
        sprintf(buff, "%s Node", name.name.c_str());
        ImGui::OpenPopup(buff);

        if (ImGui::BeginPopupModal(buff, nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize))
        {
            ImGui::Dummy({256, style_padding_y});

            if (ImGui::Button("Delete", {ImGui::GetWindowContentRegionWidth(), 24}))
            {
                Work work(provider, root);
                work.type = WorkType::DeleteNode;
                work.input_node = node;
                pending_work.emplace_back(work);
                selected_node = entt::null;
            }
            if (ImGui::Button("Cancel", {ImGui::GetWindowContentRegionWidth(), 24}))
            {
                selected_node = entt::null;
            }

            ImGui::Dummy({256, style_padding_y});

            ImGui::EndPopup();
        }
    }


    // GraphEditor stuff

    void Context::State::init(Provider& provider)
    {
        static bool once = true;
        if (!once)
            return;

        once = false;
        ent_main_window = provider.registry().create();
        main_window_id = ImGui::GetID(&ent_main_window);
        graph_interactive_region_id = ImGui::GetID(&graph_interactive_region_id);
        hover.reset(entt::null);

        profiler_data.resize(1000);

        ImGuiWindow* win = ImGui::GetCurrentWindow();
        ImRect edit_rect = win->ContentRegionRect;
        float y = (edit_rect.Max.y + edit_rect.Min.y) * 0.5f - 64;
        Work work(provider, root);
        work.type = WorkType::CreateRuntimeContext;
        work.canvas_pos = ImVec2{ edit_rect.Max.x - 300, y };
        pending_work.push_back(work);
        work.type = WorkType::ResetSaveWorkEpoch; // reset so that quitting immediately doesn't prompt a save
        pending_work.push_back(work);
    }


    void Context::State::update_mouse_state(Provider& provider)
    {
        //---------------------------------------------------------------------
        // determine hovered, dragging, pressed, and released, as well as
        // window local coordinate and canvas local coordinate

        ImGuiIO& io = ImGui::GetIO();
        ImGuiWindow* win = ImGui::GetCurrentWindow();
        ImRect edit_rect = win->ContentRegionRect;

        mouse.click_ended = false;
        mouse.click_initiated = false;
        mouse.in_canvas = false;
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
        {
            ImGui::SetCursorScreenPos(edit_rect.Min);
            ImGui::PushID(graph_interactive_region_id);
            bool result = ImGui::InvisibleButton("Noodle", edit_rect.GetSize());
            ImGui::PopID();
            if (result)
            {
                //printf("button released\n");
                mouse.dragging_node = false;
                mouse.click_ended = true;
            }
            mouse.in_canvas = ImGui::IsItemHovered();

            if (mouse.in_canvas)
            {
                mouse.mouse_ws = io.MousePos - ImGui::GetCurrentWindow()->Pos;
                mouse.mouse_cs = (mouse.mouse_ws - root.canvas.origin_offset_ws) / root.canvas.scale;

                if (io.MouseDown[0] && io.MouseDownOwned[0])
                {
                    if (!mouse.dragging)
                    {
                        //printf("button clicked\n");
                        mouse.click_initiated = true;
                        mouse.initial_click_pos_ws = io.MousePos;
                        mouse.canvas_clickpos_cs = mouse.mouse_cs;
                        mouse.canvas_clicked_pixel_offset_ws = root.canvas.origin_offset_ws;
                    }

                    mouse.dragging = true;
                }
                else
                    mouse.dragging = false;
            }
            else
                mouse.dragging = false;
        }
        else
            mouse.dragging = false;

        if (!mouse.dragging)
            hover.node_id = entt::null;
    }

    void lay_out_pins(Provider& provider)
    {
        entt::registry& registry = provider.registry();

        // may the counting begin

        for (auto node_entity : registry.view<Node>())
        {
            GraphNodeLayout& gnl = registry.get<GraphNodeLayout>(node_entity);
            if (!registry.valid(node_entity))
                continue;

            gnl.in_height = 0;
            gnl.mid_height = 0;
            gnl.out_height = 0;
            gnl.column_count = 1;

            ImVec2 node_pos = gnl.ul_cs;

            Node& node = registry.get<Node>(node_entity);

            // calculate column heights
            for (const entt::entity entity : node.pins)
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
                pnl.node_origin_cs = node_pos;

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

            gnl.column_count += gnl.mid_height > 0 ? 1 : 0;

            int height = gnl.in_height > gnl.mid_height ? gnl.in_height : gnl.mid_height;
            if (gnl.out_height > height)
                height = gnl.out_height;

            float width = GraphNodeLayout::k_column_width * gnl.column_count;
            gnl.lr_cs = node_pos + ImVec2{ width, GraphPinLayout::k_height * (1.5f + (float)height) };

            gnl.in_height = 0;
            gnl.mid_height = 0;
            gnl.out_height = 0;

            // assign columns
            for (const entt::entity entity : node.pins)
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


    void noodle_bezier(ImVec2 & p0, ImVec2 & p1, ImVec2 & p2, ImVec2 & p3, float scale)
    {
        if (p0.x > p3.x)
            std::swap(p0, p3);

        ImVec2 pd = p0 - p3;
        float wiggle = std::min(fabsf(pd.x), std::min(64.f, sqrtf(pd.x * pd.x + pd.y * pd.y)) * scale);
        p1 = { p0.x + wiggle, p0.y };
        p2 = { p3.x - wiggle, p3.y };
    }


    void Context::State::update_hovers(Provider& provider)
    {
        entt::registry& registry = provider.registry();

        //bool currently_hovered = _hover.node_id != entt::null;

        // refresh highlights if dragging a wire, or if a node is not being dragged
        bool find_highlights = mouse.dragging_wire || !mouse.dragging;
        
        if (find_highlights)
        {
            hover.bang = false;
            hover.play = false;
            hover.node_menu = false;
            hover.node_id = entt::null;
            hover.pin_id = entt::null;
            hover.pin_label_id = entt::null;
            hover.connection_id = entt::null;
            float mouse_x_cs = mouse.mouse_cs.x;
            float mouse_y_cs = mouse.mouse_cs.y;

            // check all pins
            for (const auto entity : registry.view<lab::noodle::Pin>())
            {
                if (!registry.valid(entity))
                {
                    // @TODO slate entity for demolition
                    continue;
                }

                GraphPinLayout& pnl = registry.get<GraphPinLayout>(entity);
                if (pnl.pin_contains_cs_point(root.canvas, mouse_x_cs, mouse_y_cs))
                {
                    Pin& pin = registry.get<Pin>(entity);
                    if (pin.kind == Pin::Kind::Setting)
                    {
                        hover.pin_id = entt::null;
                    }
                    else
                    {
                        hover.pin_id = entity;
                        hover.pin_label_id = entt::null;
                        hover.node_id = pin.node_id;
                    }
                }
                else if (pnl.label_contains_cs_point(root.canvas, mouse_x_cs, mouse_y_cs))
                {
                    Pin& pin = registry.get<Pin>(entity);
                    if (pin.kind == Pin::Kind::Setting || pin.kind == Pin::Kind::Param)
                    {
                        hover.pin_id = entt::null;
                        hover.pin_label_id = entity;
                        hover.node_id = pin.node_id;
                    }
                    else
                    {
                        hover.pin_label_id = entt::null;
                    }
                }
            }

            // test all nodes
            for (const entt::entity entity : registry.view<lab::noodle::Node>())
            {
                GraphNodeLayout& gnl = registry.get<GraphNodeLayout>(entity);
                ImVec2 ul = gnl.ul_cs;
                ImVec2 lr = gnl.lr_cs;
                if (mouse_x_cs >= ul.x && mouse_x_cs <= (lr.x + GraphPinLayout::k_width) && mouse_y_cs >= (ul.y - 20) && mouse_y_cs <= lr.y)
                {
                    ImVec2 pin_ul;

                    if (mouse_y_cs < ul.y)
                    {
                        float testx = ul.x + 20;
                        bool play = false;
                        bool bang = false;

                        auto& node = registry.get<Node>(entity);

                        // in banner
                        if (mouse_x_cs < testx && node.play_controller)
                        {
                            hover.play = true;
                            play = true;
                        }

                        if (node.play_controller)
                            testx += 20;

                        if (!play && mouse_x_cs < testx && node.bang_controller)
                        {
                            hover.bang = true;
                            bang = true;
                        }

                        if (!play && !bang)
                        {
                            hover.node_menu = true;
                        }
                    }

                    hover.node_id = entity;
                    break;
                }
            }

            // test all connections
            if (hover.node_id == entt::null)
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
                    ImVec2 from_pos = from_gpl.ul_ws(root.canvas) + ImVec2(style_padding_y, style_padding_x) * root.canvas.scale;
                    ImVec2 to_pos = to_gpl.ul_ws(root.canvas) + ImVec2(0, style_padding_x) * root.canvas.scale;

                    ImVec2 p0 = from_pos;
                    ImVec2 p3 = to_pos;
                    ImVec2 p1, p2;
                    noodle_bezier(p0, p1, p2, p3, root.canvas.scale);
                    ImVec2 test = mouse.mouse_ws + root.canvas.window_origin_offset_ws;
                    ImVec2 closest = ImBezierClosestPointCasteljau(p0, p1, p2, p3, test, 10);
                    
                    ImVec2 delta = test - closest;
                    float d = delta.x * delta.x + delta.y * delta.y;
                    if (d < 100)
                    {
                        hover.connection_id = entity;
                        break;
                    }
                }
            }
        }
    }


    void Context::State::run(Provider& provider, bool show_profiler, bool show_debug)
    {
        int profile_idx = 0;

        init(provider);

        ImGui::BeginChild("###Noodles");

        //---------------------------------------------------------------------
        // ensure coordinate systems are initialized properly

        ImGuiIO& io = ImGui::GetIO();
        ImGuiWindow* win = ImGui::GetCurrentWindow();
        ImRect edit_rect = win->ContentRegionRect;
        root.canvas.window_origin_offset_ws = ImGui::GetWindowPos() + ImGui::GetWindowContentRegionMin();

        //---------------------------------------------------------------------
        // ensure node sizes are up to date

        lay_out_pins(provider);

        //---------------------------------------------------------------------
        // Create a canvas

        float height = edit_rect.Max.y - edit_rect.Min.y;
        //- ImGui::GetTextLineHeightWithSpacing()   // space for the time bar
        //- ImGui::GetTextLineHeightWithSpacing();  // space for horizontal scroller

        bool rv = ImGui::BeginChild(main_window_id, ImVec2(0, height), false,
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse);

        //---------------------------------------------------------------------
        // draw the grid on the canvas

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->ChannelsSplit((int) Channel::Count);
        drawList->ChannelsSetCurrent((int) Channel::Content);
        {
            drawList->ChannelsSetCurrent((int) Channel::Grid);

            const float grid_step_x = 100.0f * root.canvas.scale;
            const float grid_step_y = 100.0f * root.canvas.scale;
            const ImVec2 grid_origin = ImGui::GetWindowPos();
            const ImVec2 grid_size = ImGui::GetWindowSize();

            drawList->AddRectFilled(grid_origin, grid_origin + grid_size, grid_bg_color);

            for (float x = fmodf(root.canvas.origin_offset_ws.x, grid_step_x); x < grid_size.x; x += grid_step_x)
            {
                drawList->AddLine(ImVec2(x, 0.0f) + grid_origin, ImVec2(x, grid_size.y) + grid_origin, grid_line_color);
            }
            for (float y = fmodf(root.canvas.origin_offset_ws.y, grid_step_y); y < grid_size.y; y += grid_step_y)
            {
                drawList->AddLine(ImVec2(0.0f, y) + grid_origin, ImVec2(grid_size.x, y) + grid_origin, grid_line_color);
            }
        }
        drawList->ChannelsSetCurrent((int) Channel::Content);

        //---------------------------------------------------------------------
        // run the Imgui portion of the UI

        update_mouse_state(provider);

        if (mouse.dragging || mouse.dragging_wire || mouse.dragging_node)
        {
            if (mouse.dragging_wire)
                update_hovers(provider);
        }
        else
        {
            bool imgui_business = context_menu(provider, mouse.mouse_cs);
            if (imgui_business)
            {
                mouse.dragging = false;
                hover.node_id = entt::null;
                hover.reset(entt::null);
                edit.selected_pin = entt::null;
            }
            else if (edit.selected_pin != entt::null)
            {
                ImGui::SetNextWindowPos(mouse.initial_click_pos_ws);
                edit.edit_pin(provider, root, edit.selected_pin, pending_work);
                mouse.dragging = false;
                hover.node_id = entt::null;
                hover.reset(entt::null);
            }
            else if (edit.selected_connection != entt::null)
            {
                ImGui::SetNextWindowPos(mouse.initial_click_pos_ws);
                edit.edit_connection(provider, root, edit.selected_connection, pending_work);
                mouse.dragging = false;
                hover.node_id = entt::null;
                hover.reset(entt::null);
            }
            else if (edit.selected_node != entt::null)
            {
                ImGui::SetNextWindowPos(mouse.initial_click_pos_ws);
                edit.edit_node(provider, root, edit.selected_node, pending_work);
                mouse.dragging = false;
            }
            else
            {
                update_hovers(provider);
            }
        }

        entt::registry& registry = provider.registry();

        /// @TODO consolidate the redundant logic here for testing connections
        if (mouse.dragging && mouse.dragging_wire)
        {
            // if dragging a wire, check for disallowed connections so they wire can turn red
            hover.valid_connection = true;
            if (hover.originating_pin_id != entt::null && hover.pin_id != entt::null && 
                registry.valid(hover.originating_pin_id) && registry.valid(hover.pin_id))
            {
                GraphPinLayout& from_gpl = registry.get<GraphPinLayout>(hover.originating_pin_id);
                GraphPinLayout& to_gpl = registry.get<GraphPinLayout>(hover.pin_id);

                Pin from_pin = registry.get<Pin>(hover.originating_pin_id);
                Pin to_pin = registry.get<Pin>(hover.pin_id);

                if (from_pin.kind == Pin::Kind::BusIn || from_pin.kind == Pin::Kind::Param)
                {
                    std::swap(to_pin, from_pin);
                }

                // check if a valid connection is requested
                Pin::Kind to_kind = to_pin.kind;
                Pin::Kind from_kind = from_pin.kind;

                hover.valid_connection = !(to_kind == Pin::Kind::Setting || to_kind == Pin::Kind::BusOut ||
                    from_kind == Pin::Kind::BusIn || from_kind == Pin::Kind::Param ||
                    from_kind == Pin::Kind::Setting);

                // disallow connecting a node to itself
                hover.valid_connection &= from_pin.node_id != to_pin.node_id;
            }
        }
        else if (!mouse.dragging && mouse.dragging_wire)
        {
            hover.valid_connection = true;
            if (hover.originating_pin_id != entt::null && hover.pin_id != entt::null && 
                registry.valid(hover.originating_pin_id) && registry.valid(hover.pin_id))
            {
                GraphPinLayout& from_gpl = registry.get<GraphPinLayout>(hover.originating_pin_id);
                GraphPinLayout& to_gpl = registry.get<GraphPinLayout>(hover.pin_id);

                Pin from_pin = registry.get<Pin>(hover.originating_pin_id);
                Pin to_pin = registry.get<Pin>(hover.pin_id);

                if (from_pin.kind == Pin::Kind::BusIn || from_pin.kind == Pin::Kind::Param)
                {
                    std::swap(to_pin, from_pin);
                    std::swap(hover.originating_pin_id, hover.pin_id);
                }

                // check if a valid connection is requested
                Pin::Kind to_kind = to_pin.kind;
                Pin::Kind from_kind = from_pin.kind;

                hover.valid_connection = !(to_kind == Pin::Kind::Setting || to_kind == Pin::Kind::BusOut ||
                    from_kind == Pin::Kind::BusIn || from_kind == Pin::Kind::Param ||
                    from_kind == Pin::Kind::Setting);

                // disallow connecting a node to itself
               hover.valid_connection &= from_pin.node_id != to_pin.node_id;

                if (!hover.valid_connection)
                {
                    printf("invalid connection request\n");
                }
                else
                {
                    if (to_kind == Pin::Kind::BusIn)
                    {
                        Work work(provider, root);
                        work.type = WorkType::ConnectBusOutToBusIn;
                        work.input_node = to_pin.node_id;
                        work.output_node = from_pin.node_id;
                        pending_work.emplace_back(work);
                    }
                    else if (to_kind == Pin::Kind::Param)
                    {
                        Work work(provider, root);
                        work.type = WorkType::ConnectBusOutToParamIn;
                        work.input_node = to_pin.node_id;
                        work.output_node = from_pin.node_id;
                        work.param_pin = to_pin.pin_id;
                        pending_work.emplace_back(work);
                    }
                }
            }
            mouse.dragging_wire = false;
            hover.originating_pin_id = entt::null;
        }
        else if (mouse.click_ended)
        {
            if (hover.connection_id != entt::null)
            {
                edit.selected_connection = hover.connection_id;
            }
            else if (hover.node_menu)
            {
                edit.selected_node = hover.node_id;
            }
        }

        mouse.interacting_with_canvas = hover.node_id == entt::null && !mouse.dragging_wire;
        if (!mouse.interacting_with_canvas)
        {
            // nodes and wires
            if (mouse.click_initiated)
            {
                if (hover.bang)
                {
                    Work work(provider, root);
                    work.type = WorkType::Bang;
                    work.input_node = hover.node_id;
                    pending_work.push_back(work);
                }
                if (hover.play)
                {
                    Work work(provider, root);
                    work.type = WorkType::Start;
                    work.input_node = hover.node_id;
                    pending_work.push_back(work);
                }
                if (hover.pin_id != entt::null)
                {
                    mouse.dragging_wire = true;
                    mouse.dragging_node = false;
                    mouse.node_initial_pos_cs = mouse.mouse_cs;
                    if (hover.originating_pin_id == entt::null)
                        hover.originating_pin_id = hover.pin_id;
                }
                else if (hover.pin_label_id != entt::null)
                {
                    // set mode to edit the value of the hovered pin
                    edit.selected_pin = hover.pin_label_id;
                    Pin& pin = registry.get<Pin>(edit.selected_pin);
                    if (pin.dataType == Pin::DataType::Float)
                    {
                        edit.pin_float = provider.pin_float_value(edit.selected_pin);
                    }
                    else if (pin.dataType == Pin::DataType::Integer || pin.dataType == Pin::DataType::Enumeration)
                    {
                        edit.pin_int = provider.pin_int_value(edit.selected_pin);
                    }
                    if (pin.dataType == Pin::DataType::Bool)
                    {
                        edit.pin_bool = provider.pin_bool_value(edit.selected_pin);
                    }
                }
                else
                {
                    mouse.dragging_wire = false;
                    mouse.dragging_node = true;

                    GraphNodeLayout& gnl = registry.get<GraphNodeLayout>(hover.node_id);
                    mouse.node_initial_pos_cs = gnl.ul_cs;
                }
            }

            if (mouse.dragging_node)
            {
                ImVec2 delta = mouse.mouse_cs - mouse.canvas_clickpos_cs;

                GraphNodeLayout& gnl = registry.get<GraphNodeLayout>(hover.node_id);
                ImVec2 sz = gnl.lr_cs - gnl.ul_cs;
                ImVec2 new_pos = mouse.node_initial_pos_cs + delta;
                gnl.ul_cs = new_pos;
                gnl.lr_cs = new_pos + sz;

                /// @TODO force the color to be highlighting
            }
        }
        else
        {
            // if the interaction is with the canvas itself, offset and scale the canvas
            if (!mouse.dragging_wire)
            {
                if (mouse.dragging)
                {
                    if (fabsf(io.MouseDelta.x) > 0.f || fabsf(io.MouseDelta.y) > 0.f)
                    {
                        // pull the pivot around
                        root.canvas.origin_offset_ws = mouse.canvas_clicked_pixel_offset_ws - mouse.initial_click_pos_ws + io.MousePos;
                    }
                }
                else if (mouse.in_canvas)
                {
                    if (fabsf(io.MouseWheel) > 0.f)
                    {
                        // scale using where the mouse is currently hovered as the pivot
                        float prev_scale = root.canvas.scale;
                        root.canvas.scale += std::copysign(0.25f, io.MouseWheel);
                        root.canvas.scale = std::max(root.canvas.scale, 0.25f);

                        // solve for off2
                        // (mouse - off1) / scale1 = (mouse - off2) / scale2 
                        root.canvas.origin_offset_ws = mouse.mouse_ws - (mouse.mouse_ws - root.canvas.origin_offset_ws) * (root.canvas.scale / prev_scale);
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
        text_color |= (uint32_t)(255 * 2 * (root.canvas.scale - 0.5f)) << 24;
        text_color_highlighted |= (uint32_t)(255 * 2 * (root.canvas.scale - 0.5f)) << 24;

        ///////////////////////////////////////////
        //   Noodles Bezier Lines Curves Pulled  //
        ///////////////////////////////////////////

        drawList->ChannelsSetCurrent((int) Channel::Nodes);

        for (const auto entity : registry.view<lab::noodle::Connection>())
        {
            lab::noodle::Connection& i = registry.get<lab::noodle::Connection>(entity);
            entt::entity from_pin = i.pin_from;
            entt::entity to_pin = i.pin_to;

            GraphPinLayout& from_gpl = registry.get<GraphPinLayout>(from_pin);
            GraphPinLayout& to_gpl = registry.get<GraphPinLayout>(to_pin);
            ImVec2 from_pos = from_gpl.ul_ws(root.canvas) + ImVec2(style_padding_y, style_padding_x) * root.canvas.scale;
            ImVec2 to_pos = to_gpl.ul_ws(root.canvas) + ImVec2(0, style_padding_x) * root.canvas.scale;

            ImVec2 p0 = from_pos;
            ImVec2 p3 = to_pos;
            ImVec2 p1, p2;
            noodle_bezier(p0, p1, p2, p3, root.canvas.scale);
            ImU32 color = entity == hover.connection_id ? noodle_bezier_hovered : noodle_bezier_neutral;
            drawList->AddBezierCurve(p0, p1, p2, p3, color, 2.f);
        }

        if (mouse.dragging_wire)
        {
            GraphPinLayout& from_gpl = registry.get<GraphPinLayout>(hover.originating_pin_id);
            ImVec2 p0 = from_gpl.ul_ws(root.canvas) + ImVec2(style_padding_y, style_padding_x) * root.canvas.scale;
            ImVec2 p3 = mouse.mouse_ws + root.canvas.window_origin_offset_ws;
            ImVec2 p1, p2;
            noodle_bezier(p0, p1, p2, p3, root.canvas.scale);
            ImU32 color = hover.valid_connection ? noodle_bezier_neutral : noodle_bezier_cancel;
            drawList->AddBezierCurve(p0, p1, p2, p3, color, 2.f);
        }

        ///////////////////////////////////////
        //   Node Body / Drawing / Profiler  //
        ///////////////////////////////////////

        if (!registry.valid(edit._device_node))
            edit._device_node = entt::null;

        total_profile_duration = provider.node_get_timing(edit._device_node);

        for (auto entity : registry.view<lab::noodle::Node>())
        {
            float node_profile_duration = provider.node_get_self_timing(entity);
            node_profile_duration = std::abs(node_profile_duration); /// @TODO, the destination node doesn't yet have a totalTime, so abs is a hack in the nonce

            Name& name = registry.get<Name>(entity);
            profiler_data[profile_idx].color = legit::colors[((profile_idx + 4 * profile_idx) & 0xf)]; // shuffle the colors so like colors are not together
            profiler_data[profile_idx].name = name.name;
            profiler_data[profile_idx].startTime = (profile_idx > 0) ? profiler_data[profile_idx - 1].endTime : 0;
            profiler_data[profile_idx].endTime = profiler_data[profile_idx].startTime + provider.node_get_self_timing(edit._device_node);
            profile_idx = (profile_idx + 1) % profiler_data.size();

            GraphNodeLayout& gnl = registry.get<GraphNodeLayout>(entity);
            drawList->ChannelsSetCurrent((int)gnl.channel);

            ImVec2 ul_ws = gnl.ul_cs;
            ImVec2 lr_ws = gnl.lr_cs;
            ul_ws = root.canvas.window_origin_offset_ws + ul_ws * root.canvas.scale + root.canvas.origin_offset_ws;
            lr_ws = root.canvas.window_origin_offset_ws + lr_ws * root.canvas.scale + root.canvas.origin_offset_ws;

            // draw node
            drawList->AddRectFilled(ul_ws, lr_ws, node_background_fill, node_border_radius);
            drawList->AddRect(ul_ws, lr_ws, (hover.node_id == entity) ? node_outline_hovered : node_outline_neutral, node_border_radius, 15, 2);

            if (show_profiler)
            {
                ImVec2 p1{ ul_ws.x, lr_ws.y };
                ImVec2 p2{ lr_ws.x, lr_ws.y + root.canvas.scale * style_padding_y };
                drawList->AddRect(p1, p2, ImColor(128, 255, 128, 255));
                p2.x = p1.x + (p2.x - p1.x) * node_profile_duration / total_profile_duration;
                drawList->AddRectFilled(p1, p2, ImColor(255, 255, 255, 128));
            }

            if (registry.any<NodeRender>(entity))
            {
                NodeRender& render = registry.get<NodeRender>(entity);
                if (render.render)
                    render.render(entity, { ul_ws.x, ul_ws.y }, { lr_ws.x, lr_ws.y }, root.canvas.scale, drawList);
            }

            ///////////////////////////////////////////
            //   Node Header / Banner / Top / Menu   //
            ///////////////////////////////////////////

            if (root.canvas.scale > 0.5f)
            {
                const float label_font_size = style_padding_y * root.canvas.scale;
                ImVec2 label_pos = ul_ws;
                label_pos.y -= 20 * root.canvas.scale;

                auto& node = registry.get<Node>(entity);

                // UI elements
                if (node.play_controller)
                {
                    auto label = std::string(ICON_FAD_PLAY);
                    drawList->AddText(NULL, label_font_size, label_pos,
                        (hover.play && entity == hover.node_id) ? text_color_highlighted : text_color,
                        label.c_str(), label.c_str() + label.length());
                    label_pos.x += 20;
                }

                if (node.bang_controller)
                {
                    auto label = std::string(ICON_FAD_HARDCLIP);
                    drawList->AddText(NULL, label_font_size, label_pos,
                        (hover.bang && entity == hover.node_id) ? text_color_highlighted : text_color,
                        label.c_str(), label.c_str() + label.length());
                    label_pos.x += 20;
                }

                // Name
                Name& name = registry.get<Name>(entity);
                label_pos.x += 5;
                drawList->AddText(io.FontDefault, label_font_size, label_pos,
                    (hover.node_menu && entity == hover.node_id) ? text_color_highlighted : text_color,
                    name.name.c_str(), name.name.c_str() + name.name.size());
            }

            ///////////////////////////////////////////
            //   Node Input Pins / Connection / Pin  //
            ///////////////////////////////////////////

            Node& node = registry.get<Node>(entity);
            for (entt::entity j : node.pins)
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
                ImVec2 pin_ul = pin_gpl.ul_ws(root.canvas);
                uint32_t fill = (j == hover.pin_id || j == hover.originating_pin_id) ? 0xffffff : 0x000000;
                fill |= (uint32_t)(128 + 128 * sinf(pulse * 8)) << 24;

                DrawIcon(drawList, pin_ul,
                    ImVec2{ pin_ul.x + GraphPinLayout::k_width * root.canvas.scale, pin_ul.y + GraphPinLayout::k_height * root.canvas.scale },
                    icon_type, false, color, fill);

                // Only draw text if we can likely see it
                if (root.canvas.scale > 0.5f)
                {
                    float font_size = style_padding_y * root.canvas.scale;
                    ImVec2 label_pos = pin_ul;

                    label_pos.y += 2;

                    label_pos.x += 20 * root.canvas.scale;
                    drawList->AddText(NULL, font_size, label_pos, text_color,
                        pin_it.shortName.c_str(), pin_it.shortName.c_str() + pin_it.shortName.length());

                    if (has_value)
                    {
                        label_pos.x += 50 * root.canvas.scale;
                        drawList->AddText(NULL, font_size, label_pos, text_color,
                            pin_it.value_as_string.c_str(), pin_it.value_as_string.c_str() + pin_it.value_as_string.length());
                    }
                }

                pin_ul.y += 20 * root.canvas.scale;
            }
        }

        // finish

        drawList->ChannelsMerge();
        ImGui::EndChild();

        if (show_debug)
        {
            ImGui::Begin("Debug Information");
            ImGui::TextUnformatted("Mouse");
            ImGui::Text("canvas    pos (%d, %d)", (int)mouse.mouse_cs.x, (int)mouse.mouse_cs.y);
            ImGui::Text("drawlist  pos (%d, %d)", (int)mouse.mouse_ws.x, (int)mouse.mouse_ws.y);
            ImGui::Text("LMB %s%s%s", mouse.click_initiated ? "*" : "-", mouse.dragging ? "*" : "-", mouse.click_ended ? "*" : "-");
            ImGui::Text("canvas interaction: %s", mouse.interacting_with_canvas ? "*" : ".");
            ImGui::Text("wire dragging: %s", mouse.dragging_wire ? "*" : ".");

            ImGui::Separator();
            ImGui::TextUnformatted("Canvas");
            ImVec2 off = root.canvas.window_origin_offset_ws;
            ImGui::Text("canvas window offset (%d, %d)", (int)off.x, (int)off.y);
            off = root.canvas.origin_offset_ws;
            ImGui::Text("canvas origin offset (%d, %d)", (int)off.x, (int)off.y);

            ImGui::Separator();
            ImGui::TextUnformatted("Hover");
            ImGui::Text("node hovered: %s", hover.node_id != entt::null ? "*" : ".");
            ImGui::Text("originating pin id: %d", hover.originating_pin_id);
            ImGui::Text("hovered pin id: %d", hover.pin_id);
            ImGui::Text("hovered pin label: %d", hover.pin_label_id);
            ImGui::Text("hovered connection: %d", hover.connection_id);
            ImGui::Text("hovered node menu: %s", hover.node_menu ? "*" : ".");

            ImGui::Separator();
            ImGui::TextUnformatted("Edit");
            ImGui::Text("edit connection: %d", edit.selected_connection);
            ImGui::Separator();
            ImGui::Text("quantum time: %f uS", total_profile_duration * 1e6f);

            ImGui::End();
        }

        if (show_profiler)
        {
            ImGui::Begin("Profiler");
            profiler_graph.LoadFrameData(&profiler_data[0], profile_idx);
            profiler_graph.RenderTimings(400, 300, 200, 0.000005f, 0);// std::max(0, profile_idx - 100));
            ImGui::End();
        }
        ImGui::EndChild();

        for (Work& work : pending_work)
            work.eval(edit);

        pending_work.clear();
    }


    Context::Context(Provider& p)
        : provider(p), _s(new State)
    {
        _s->init(p);
    }

    Context::~Context()
    {
        delete _s;
    }


    bool Context::run()
    {
        _s->run(provider, show_profiler, show_debug);
        return true;
    }

    void Context::save(const std::string& path)
    {
        /// @TODO this is a prototype file format, meant to debug actually writing valuable data
        /// The format could be something else entirely, this routine should be treated more
        /// like a prototype template that can be duplicated for a new format.

        // Note: this code uses \n because std::endl has other behaviors
        using lab::noodle::Name;
        using lab::noodle::Pin;
        entt::registry& reg = provider.registry();
        std::ofstream file(path, std::ios::binary);
        file << "#!LabSoundGraphToy\n";
        file << "# " << path << "\n";
        for (auto node_entity : reg.view<lab::noodle::Node>())
        {
            if (!reg.valid(node_entity))
                continue;

            lab::noodle::Node& node = reg.get<lab::noodle::Node>(node_entity);
            lab::noodle::Name& name = reg.get<lab::noodle::Name>(node_entity);
            file << "node: " << node.kind << " name: " << name.name << "\n";

            if (reg.any<GraphNodeLayout>(node_entity))
            {
                GraphNodeLayout& gnl = reg.get<GraphNodeLayout>(node_entity);
                file << " pos: " << gnl.ul_cs.x << " " << gnl.ul_cs.y << "\n";
            }

            for (const entt::entity entity : node.pins)
            {
                if (!reg.valid(entity))
                    continue;

                Pin pin = reg.get<Pin>(entity);
                if (!reg.valid(pin.node_id))
                    continue;

                Name& name = reg.get<Name>(entity);
                switch (pin.kind)
                {
                case Pin::Kind::BusIn:
                case Pin::Kind::BusOut:
                    break;

                case Pin::Kind::Param:
                    file << " param: " << name.name << " " << pin.value_as_string << "\n";
                    break;
                case Pin::Kind::Setting:
                    file << " setting: " << name.name << " ";
                    switch (pin.dataType)
                    {
                    case Pin::DataType::None: file << "None "; break;
                    case Pin::DataType::Bus: file << "Bus "; break;
                    case Pin::DataType::Bool: file << "Bool "; break;
                    case Pin::DataType::Integer: file << "Integer "; break;
                    case Pin::DataType::Enumeration: file << "Enumeration "; break;
                    case Pin::DataType::Float: file << "Float "; break;
                    case Pin::DataType::String: file << "String "; break;
                    }
                    file << pin.value_as_string << "\n";
                    break;
                }
            }
        }

        for (const auto entity : reg.view<lab::noodle::Connection>())
        {
            lab::noodle::Connection& connection = reg.get<lab::noodle::Connection>(entity);
            entt::entity from_pin = connection.pin_from;
            entt::entity to_pin = connection.pin_to;
            if (!reg.valid(from_pin) || !reg.valid(to_pin))
                continue;

            using lab::noodle::Name;
            Name& from_pin_name = reg.get<Name>(from_pin);
            Name& to_pin_name = reg.get<Name>(from_pin);
            Name& from_node_name = reg.get<Name>(connection.node_from);
            Name& to_node_name = reg.get<Name>(connection.node_to);

            file << " + " << from_node_name.name << ":" << from_pin_name.name <<
                " -> " << to_node_name.name << ":" << to_node_name.name << "\n";
        }

        file.flush();
        _s->edit.unify_epochs();
    }

    void Context::load(const std::string& path)
    {
        bool load_succeeded = true;
        if (load_succeeded)
        {
            _s->edit.reset_epochs();
        }
    }

    bool Context::needs_saving() const
    {
        return _s->edit.need_saving();
    }

    void Context::clear_all()
    {
        Work work(provider, _s->root);
        work.type = WorkType::ClearScene;
        _s->pending_work.emplace_back(work);
    }


}} // lab::noodle
