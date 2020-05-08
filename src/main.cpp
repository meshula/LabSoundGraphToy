#define SOKOL_TRACE_HOOKS

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "imgui.h"
#include "sokol_imgui.h"
#include "sokol_gfx_imgui.h"

#include "lab_imgui_ext.hpp"
#include "LabSoundInterface.h"
#include "lab_noodle.h"

#include "nfd.h"

#include <tinyosc.hpp>
#include <tinyosc-net.hpp>

#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "OSCMsg.hpp"

std::unordered_map<std::string, int> _addr_map;
std::unordered_map<int, int> _id_argc_map;
int _next_addr = 0;

polymer::spsc_queue<OSCMsg> * _osc_queue = nullptr;
namespace {
    bool join_osc = false;
}

void open_udp_server()
{
    osc_net_address_t server_addr;
    osc_net_get_address(&server_addr, nullptr, 8000);

    osc_net_socket_t server_socket;
    if (osc_net_udp_socket_open(&server_socket, server_addr, true))
    {
        std::cout << "osc_net_udp_socket_open osc_net_err: " << osc_net_get_error() << std::endl;
    }
    else
    {
        std::cout << "OSC server started, will listen to packets on UDP port " << 8000 << std::endl;

        tinyosc::osc_packet_reader packet_reader;
        tinyosc::osc_packet_writer packet_writer;

        while (!join_osc)
        {
            std::vector<uint8_t> recv_byte_buffer(1024 * 128);

            osc_net_address_t sender;
            if (auto bytes = osc_net_udp_socket_receive(&server_socket, &sender, recv_byte_buffer.data(), (int) recv_byte_buffer.size(), 30))
            {
                packet_reader.initialize_from_ptr(recv_byte_buffer.data(), bytes);
                tinyosc::osc_message* msg;

                while (packet_reader.check_error() && (msg = packet_reader.pop_message()) != 0)
                {
                    OSCMsg osc_msg;
                    auto addr = msg->get_address_pattern();
                    int addr_id;
                    auto it = _addr_map.find(addr);
                    if (it == _addr_map.end())
                    {
                        addr_id = ++_next_addr;
                        _addr_map[addr] = addr_id;
                        auto tags = msg->get_type_tags();
                        if (tags == "f")
                            osc_msg.argc = 1;
                        else if (tags == "ff")
                            osc_msg.argc = 2;
                        else if (tags == "fff")
                            osc_msg.argc = 3;
                        else
                            osc_msg.argc = 0;
                        _id_argc_map[addr_id] = osc_msg.argc;
                        it = _addr_map.find(addr);
                        osc_msg.addr = it->first.c_str();
                    }
                    else
                    {
                        addr_id = it->second;
                        osc_msg.addr = it->first.c_str();
                        osc_msg.argc = _id_argc_map[addr_id];
                    }

                    osc_msg.addr_id = addr_id;

                    auto& match = msg->match_complete(addr);
                    for (int i = 0; i < osc_msg.argc; ++i)
                    {
                        float x;
                        match = match.pop_float(x);
                        if (i < 4)
                            osc_msg.data[i] = x;
                    }
                    match.check_no_more_args();

                    _osc_queue->produce(std::move(osc_msg));
                }
            }
        }
    }
}

enum class Command
{
    None,
    New,
    Open,
    Save,
    Quit
};

static uint64_t last_time = 0;
static sg_pass_action pass_action;
sg_imgui_t sg_imgui;

#define MSAA_SAMPLES (8)

std::string g_app_path;
ImFont * g_roboto = nullptr;
ImFont * g_cousine = nullptr;
ImFont * g_audio_icon = nullptr;

std::thread* osc_service_thread = nullptr;

void init(void) {
    _osc_queue = new polymer::spsc_queue<OSCMsg>();
    osc_net_init();
    osc_service_thread = new std::thread([]() {
        open_udp_server();
        });

    // setup sokol-gfx, sokol-time and sokol-imgui
    sg_desc desc = { };
    desc.mtl_device = sapp_metal_get_device();
    desc.mtl_renderpass_descriptor_cb = sapp_metal_get_renderpass_descriptor;
    desc.mtl_drawable_cb = sapp_metal_get_drawable;
    desc.d3d11_device = sapp_d3d11_get_device();
    desc.d3d11_device_context = sapp_d3d11_get_device_context();
    desc.d3d11_render_target_view_cb = sapp_d3d11_get_render_target_view;
    desc.d3d11_depth_stencil_view_cb = sapp_d3d11_get_depth_stencil_view;
    desc.gl_force_gles2 = sapp_gles2();
    sg_setup(&desc);
    stm_setup();

    // setup debug inspection header(s)
    sg_imgui_init(&sg_imgui);

    // use sokol-imgui with all default-options (we're not doing
    // multi-sampled rendering or using non-default pixel formats)
    simgui_desc_t simgui_desc = { };
    simgui_setup(&simgui_desc);

    // initial clear color
    pass_action.colors[0].action = SG_ACTION_CLEAR;
    pass_action.colors[0].val[0] = 0.0f;
    pass_action.colors[0].val[1] = 0.5f;
    pass_action.colors[0].val[2] = 0.7f;
    pass_action.colors[0].val[3] = 1.0f;

    ImGuiIO & io = ImGui::GetIO();

    ImFontConfig config;
    config.OversampleH = 6;
    config.OversampleV = 6;
    g_cousine = io.Fonts->AddFontFromMemoryCompressedTTF(imgui_fonts::getCousineRegularCompressedData(), imgui_fonts::getCousineRegularCompressedSize(), 15.0f, &config);
    assert(g_cousine != nullptr);

    const auto font_audio_ttf = read_file_binary("../resources/fontaudio.ttf");
    g_audio_icon = append_audio_icon_font(font_audio_ttf);

    // Upload new font texture atlas
    unsigned char* font_pixels;
    int font_width, font_height;
    io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);
    sg_image_desc img_desc = { };
    img_desc.width = font_width;
    img_desc.height = font_height;
    img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    img_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    img_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    img_desc.min_filter = SG_FILTER_LINEAR;
    img_desc.mag_filter = SG_FILTER_LINEAR;
    img_desc.content.subimage[0][0].ptr = font_pixels;
    img_desc.content.subimage[0][0].size = font_width * font_height * 4;
    io.Fonts->TexID = (ImTextureID)(uintptr_t) sg_make_image(&img_desc).id;

    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding = 0.f;
    style.ChildRounding = 0.f;
    style.FrameRounding = 0.f;
    style.PopupRounding = 0.f;
    style.ScrollbarRounding = 0.f;
    style.GrabRounding = 0.f;
    style.TabRounding = 0.f;
    style.CurveTessellationTol = 0.01f;
    style.AntiAliasedFill = true;
}

void frame()
{
    const int width = sapp_width();
    const int height = sapp_height();
    const float w = (float)sapp_width();
    const float h = (float)sapp_height();
    const double delta_time = stm_sec(stm_laptime(&last_time));

    sg_begin_default_pass(&pass_action, width, height);

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({ 0,0 });
    ImGui::SetNextWindowSize(io.DisplaySize);

    simgui_new_frame(width, height, delta_time);

    ImGui::PushFont(g_cousine);

    imgui_fixed_window_begin("GraphToyCanvas", 0.f, 20.f, static_cast<float>(width), static_cast<float>(height));

    static LabSoundProvider provider;
    static lab::noodle::Context config(provider);
    static bool show_demo = false;
    OSCMsg osc_msg;
    while (_osc_queue->consume(osc_msg))
    {
        provider.add_osc_addr(osc_msg.addr, osc_msg.addr_id, osc_msg.argc, osc_msg.data);
    }

    static Command command = Command::None;
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File")) 
        {
            bool save = false;
            bool new_file = false;
            bool load = false;
            bool quit = false;
            ImGui::MenuItem("New", 0, &new_file);
            if (new_file)
                command = Command::New;
            ImGui::MenuItem("Open", 0, &load);
            if (load)
                command = Command::Open;
            ImGui::MenuItem("Save", 0, &save);
            if (save)
                command = Command::Save;
            ImGui::MenuItem("Quit", 0, &quit);
            if (quit)
                command = Command::Quit;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Debug"))
        {
            ImGui::Checkbox("Show Profiler", &config.show_profiler);
            ImGui::Checkbox("Show Graph Canvas values", &config.show_debug);
            ImGui::Checkbox("Show ImGui demo", &show_demo);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    switch (command)
    {
    case Command::Quit:
        if (config.needs_saving())
        {
            ImGui::OpenPopup("Quit");
            if (ImGui::BeginPopupModal("Quit", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
            {
                if (ImGui::Button("Save and Quit"))
                {
                    const char* file = noc_file_dialog_open(NOC_FILE_DIALOG_SAVE, "*.ls", ".", "*.*");
                    if (file)
                    {
                        config.save(file);
                        sapp_request_quit();
                    }
                    command = Command::None;
                }

                if (ImGui::Button("Quit without Saving"))
                {
                    sapp_request_quit();
                    command = Command::None;
                }

                if (ImGui::Button("Cancel"))
                {
                    command = Command::None;
                }

                ImGui::EndPopup();
            }
        }
        else
        {
            sapp_request_quit();
            command = Command::None;
        }
        break;

    case Command::New:
        if (config.needs_saving())
        {
            ImGui::OpenPopup("New");
            if (ImGui::BeginPopupModal("New", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
            {
                if (ImGui::Button("Save work in progress"))
                {
                    const char* file = noc_file_dialog_open(NOC_FILE_DIALOG_SAVE, "*.ls", ".", "*.*");
                    if (file)
                    {
                        config.save(file);
                        config.clear_all();
                    }
                    command = Command::None;
                }

                if (ImGui::Button("Clear all"))
                {
                    config.clear_all();
                    command = Command::None;
                }

                if (ImGui::Button("Cancel"))
                {
                    command = Command::None;
                }

                ImGui::EndPopup();
            }
        }
        else
        {
            config.clear_all();
            command = Command::None;
        }
        break;

    case Command::Save:
    {
        const char* file = noc_file_dialog_open(NOC_FILE_DIALOG_SAVE, "*.ls", ".", "*.*");
        if (file)
        {
            config.save(file);
        }
        command = Command::None;
        break;
    }

    case Command::Open:
        if (config.needs_saving())
        {
            ImGui::OpenPopup("Open");
            if (ImGui::BeginPopupModal("Open", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
            {
                if (ImGui::Button("Save work in progress?"))
                {
                    const char* file = noc_file_dialog_open(NOC_FILE_DIALOG_SAVE, "*.ls", ".", "*.*");
                    if (file)
                    {
                        config.save(file);
                        config.clear_all();
                    }
                    command = Command::None;
                }

                if (ImGui::Button("Open"))
                {
                    command = Command::None;
                }

                if (ImGui::Button("Cancel"))
                {
                    command = Command::None;
                }

                ImGui::EndPopup();
            }
        }
        else
        {
            command = Command::None;
        }
        const char* file = noc_file_dialog_open(NOC_FILE_DIALOG_OPEN, "*.ls", ".", "*.*");
        if (file)
        {
            config.clear_all();
            config.load(file);
        }
        command = Command::None;
        break;
    }

    config.run();

    imgui_fixed_window_end();

    if (show_demo)
        ImGui::ShowDemoWindow();

    ImGui::PopFont();

    sg_imgui_draw(&sg_imgui);

    // the sokol_gfx draw pass
    simgui_render();
    sg_end_pass();
    sg_commit();
}

void cleanup(void) 
{
    if (osc_service_thread)
    {
        join_osc = true;
        if (osc_service_thread->joinable())
            osc_service_thread->join();
        delete osc_service_thread;
    }
    delete _osc_queue;
    osc_net_shutdown();

    sg_imgui_discard(&sg_imgui);
    simgui_shutdown();
    sg_shutdown();
}

void input(const sapp_event* event) 
{
    simgui_handle_event(event);
}

sapp_desc sokol_main(int argc, char* argv[])
{
    std::string app_path(argv[0]);
    size_t index = app_path.rfind('/');
    if (index == std::string::npos)
        index = app_path.rfind('\\');
    g_app_path = app_path.substr(0, index);

    sapp_desc desc = { };
    desc.init_cb = init;
    desc.frame_cb = frame;
    desc.cleanup_cb = cleanup;
    desc.event_cb = input;
    desc.width = 1200;
    desc.height = 1000;
    desc.gl_force_gles2 = true;
    desc.window_title = "LabSound GraphToy";
    desc.ios_keyboard_resizes_canvas = false;
    return desc;
}
