//------------------------------------------------------------------------------
//  imgui-sapp.c
//
//  Demonstrates Dear ImGui UI rendering via sokol_gfx.h and
//  the utility header sokol_imgui.h
//------------------------------------------------------------------------------

#define SOKOL_TRACE_HOOKS
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "imgui.h"
#include "sokol_imgui.h"
#include "sokol_gfx_imgui.h"
#include <string>

static uint64_t last_time = 0;

void AudioGraphEditor_Initialize();
void AudioGraphEditor_Finalize();
void AudioGraphEditor_RunUI();

static sg_pass_action pass_action;
sg_imgui_t sg_imgui;
#define MSAA_SAMPLES (4)

std::string g_app_path;
static bool quit = false;
ImFont* g_roboto = nullptr;

void init(void) {
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

    AudioGraphEditor_Initialize();
}

void frame()
{
    const int width = sapp_width();
    const int height = sapp_height();
    const float w = (float)sapp_width();
    const float h = (float)sapp_height();
    const double delta_time = stm_sec(stm_laptime(&last_time));

    sg_begin_default_pass(&pass_action, width, height);

    simgui_new_frame(width, height, delta_time);

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({ 0,0 });
    ImGui::SetNextWindowSize(io.DisplaySize);
    static bool show_editor = true;
    ImGui::Begin("Editor", &show_editor,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse);

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("GraphToy")) {
            ImGui::MenuItem("Quit", 0, &quit);
            if (quit)
                sapp_request_quit();
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    AudioGraphEditor_RunUI();
    ImGui::End();

    sg_imgui_draw(&sg_imgui);

    // the sokol_gfx draw pass
    simgui_render();
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    AudioGraphEditor_Finalize();
    sg_imgui_discard(&sg_imgui);
    simgui_shutdown();
    sg_shutdown();
}

void input(const sapp_event* event) {
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
    desc.window_title = "LabSound Graph Toy";
    desc.ios_keyboard_resizes_canvas = false;
    return desc;
}
