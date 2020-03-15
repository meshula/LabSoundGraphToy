
#include <imgui.h>
#include <imgui_internal.h>
#include <entt/entt.hpp>
#include <algorithm>
#include <LabSound/LabSound.h>
#include <LabSound/core/AudioNode.h>

namespace lab {
namespace noodle {

    entt::registry g_registry;

    entt::entity g_ent_main_window;
    ImGuiID g_id_main_window;
    ImGuiID g_id_main_invis;

    ImVec2 g_canvas_pixel_offset_ws = { 0, 0 };
    float g_canvas_scale = 1;


    enum Channels { ChannelContent = 0, ChannelGrid = 1, 
                    ChannelNodes = 2, 
                    ChannelCount = 3 };

    ImU32 g_bg_color;

    bool g_in_canvas = false;
    bool g_dragging = false;
    bool g_dragging_wire = false;
    bool g_dragging_node = false;

    ImVec2 g_initial_click_pos_ws = { 0, 0 };
    ImVec2 g_canvas_clickpos_cs = { 0, 0 };
    ImVec2 g_prevDrag = { 0, 0 };
    ImVec2 g_canvas_clicked_pixel_offset_ws = { 0, 0 };
    ImVec2 g_mouse_ws = { 0, 0 };
    ImVec2 g_mouse_cs = { 0, 0 };


    static entt::entity g_no_entity;
    static entt::entity g_edit_connection;
    static entt::entity g_originating_pin_id;
    static entt::entity g_edit_pin;
    static entt::entity g_edit_node;
    static float g_edit_pin_float = 0;
    static int g_edit_pin_int = 0;
    static bool g_edit_pin_bool = false;

    ImVec2 g_node_initial_pos_cs = { 0, 0 };

    std::unique_ptr<lab::AudioContext> g_audio_context;

    struct AudioGuiNode;

    using namespace ImGui;

    using std::map;
    using std::shared_ptr;
    using std::string;
    using std::unique_ptr;
    using std::vector;


#define NOC_FILE_DIALOG_WIN32
#define NOC_FILE_DIALOG_IMPLEMENTATION
//https://github.com/guillaumechereau/noc/blob/master/noc_file_dialog.h
/* noc_file_dialog library
 *
 * Copyright (c) 2015 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

 /* A portable library to create open and save dialogs on linux, osx and
  * windows.
  *
  * The library define a single function : noc_file_dialog_open.
  * With three different implementations.
  *
  * Usage:
  *
  * The library does not automatically select the implementation, you need to
  * define one of those macros before including this file:
  *
  *  NOC_FILE_DIALOG_GTK
  *  NOC_FILE_DIALOG_WIN32
  *  NOC_FILE_DIALOG_OSX
  */

    enum {
        NOC_FILE_DIALOG_OPEN = 1 << 0,   // Create an open file dialog.
        NOC_FILE_DIALOG_SAVE = 1 << 1,   // Create a save file dialog.
        NOC_FILE_DIALOG_DIR = 1 << 2,   // Open a directory.
        NOC_FILE_DIALOG_OVERWRITE_CONFIRMATION = 1 << 3,
    };

    // There is a single function defined.

    /* flags            : union of the NOC_FILE_DIALOG_XXX masks.
     * filters          : a list of strings separated by '\0' of the form:
     *                      "name1 reg1 name2 reg2 ..."
     *                    The last value is followed by two '\0'.  For example,
     *                    to filter png and jpeg files, you can use:
     *                      "png\0*.png\0jpeg\0*.jpeg\0"
     *                    You can also separate patterns with ';':
     *                      "jpeg\0*.jpg;*.jpeg\0"
     *                    Set to NULL for no filter.
     * default_path     : the default file to use or NULL.
     * default_name     : the default file name to use or NULL.
     *
     * The function return a C string.  There is no need to free it, as it is
     * managed by the library.  The string is valid until the next call to
     * no_dialog_open.  If the user canceled, the return value is NULL.
     */
    const char* noc_file_dialog_open(int flags,
        const char* filters,
        const char* default_path,
        const char* default_name);

#ifdef NOC_FILE_DIALOG_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>

    static char* g_noc_file_dialog_ret = NULL;

#ifdef NOC_FILE_DIALOG_GTK

#include <gtk/gtk.h>

    const char* noc_file_dialog_open(int flags,
        const char* filters,
        const char* default_path,
        const char* default_name)
    {
        GtkWidget* dialog;
        GtkFileFilter* filter;
        GtkFileChooser* chooser;
        GtkFileChooserAction action;
        gint res;
        char buf[128], * patterns;

        action = flags & NOC_FILE_DIALOG_SAVE ? GTK_FILE_CHOOSER_ACTION_SAVE :
            GTK_FILE_CHOOSER_ACTION_OPEN;
        if (flags & NOC_FILE_DIALOG_DIR)
            action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;

        gtk_init_check(NULL, NULL);
        dialog = gtk_file_chooser_dialog_new(
            flags & NOC_FILE_DIALOG_SAVE ? "Save File" : "Open File",
            NULL,
            action,
            "_Cancel", GTK_RESPONSE_CANCEL,
            flags & NOC_FILE_DIALOG_SAVE ? "_Save" : "_Open", GTK_RESPONSE_ACCEPT,
            NULL);
        chooser = GTK_FILE_CHOOSER(dialog);
        if (flags & NOC_FILE_DIALOG_OVERWRITE_CONFIRMATION)
            gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);

        if (default_path)
            gtk_file_chooser_set_filename(chooser, default_path);
        if (default_name)
            gtk_file_chooser_set_current_name(chooser, default_name);

        while (filters && *filters) {
            filter = gtk_file_filter_new();
            gtk_file_filter_set_name(filter, filters);
            filters += strlen(filters) + 1;

            // Split the filter pattern with ';'.
            strcpy(buf, filters);
            buf[strlen(buf)] = '\0';
            for (patterns = buf; *patterns; patterns++)
                if (*patterns == ';') *patterns = '\0';
            patterns = buf;
            while (*patterns) {
                gtk_file_filter_add_pattern(filter, patterns);
                patterns += strlen(patterns) + 1;
            }

            gtk_file_chooser_add_filter(chooser, filter);
            filters += strlen(filters) + 1;
        }

        res = gtk_dialog_run(GTK_DIALOG(dialog));

        free(g_noc_file_dialog_ret);
        g_noc_file_dialog_ret = NULL;

        if (res == GTK_RESPONSE_ACCEPT)
            g_noc_file_dialog_ret = gtk_file_chooser_get_filename(chooser);
        gtk_widget_destroy(dialog);
        while (gtk_events_pending()) gtk_main_iteration();
        return g_noc_file_dialog_ret;
    }

#endif

#ifdef NOC_FILE_DIALOG_WIN32

#include <windows.h>
#include <commdlg.h>

    const char* noc_file_dialog_open(int flags,
        const char* filters,
        const char* default_path,
        const char* default_name)
    {
        OPENFILENAME ofn;       // common dialog box structure
        char szFile[260];       // buffer for file name
        int ret;

        // init default file name
        if (default_name)
            strncpy(szFile, default_name, sizeof(szFile) - 1);
        else
            szFile[0] = '\0';

        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = filters;
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = (LPSTR)default_path;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

        if (flags & NOC_FILE_DIALOG_OPEN)
            ret = GetOpenFileName(&ofn);
        else
            ret = GetSaveFileName(&ofn);

        free(g_noc_file_dialog_ret);
        g_noc_file_dialog_ret = ret ? _strdup(szFile) : NULL;
        return g_noc_file_dialog_ret;
    }

#endif

#ifdef NOC_FILE_DIALOG_OSX

#include <AppKit/AppKit.h>

    const char* noc_file_dialog_open(int flags,
        const char* filters,
        const char* default_path,
        const char* default_name)
    {
        NSURL* url;
        const char* utf8_path;
        NSSavePanel* panel;
        NSOpenPanel* open_panel;
        NSMutableArray* types_array;
        NSURL* default_url;
        char buf[128], * patterns;
        // XXX: I don't know about memory management with cococa, need to check
        // if I leak memory here.
        NSAutoreleasePool* pool = [[NSAutoreleasePool alloc]init];

        if (flags & NOC_FILE_DIALOG_OPEN) {
            panel = open_panel = [NSOpenPanel openPanel];
        }
        else {
            panel = [NSSavePanel savePanel];
        }

        if (flags & NOC_FILE_DIALOG_DIR) {
            [open_panel setCanChooseDirectories : YES] ;
            [open_panel setCanChooseFiles : NO] ;
        }

        if (default_path) {
            default_url = [NSURL fileURLWithPath :
            [NSString stringWithUTF8String : default_path] ];
            [panel setDirectoryURL : default_url] ;
            [panel setNameFieldStringValue : default_url.lastPathComponent] ;
        }

        if (filters) {
            types_array = [NSMutableArray array];
            while (*filters) {
                filters += strlen(filters) + 1; // skip the name
                // Split the filter pattern with ';'.
                strcpy(buf, filters);
                buf[strlen(buf) + 1] = '\0';
                for (patterns = buf; *patterns; patterns++)
                    if (*patterns == ';') *patterns = '\0';
                patterns = buf;
                while (*patterns) {
                    assert(strncmp(patterns, "*.", 2) == 0);
                    patterns += 2; // Skip the "*."
                    [types_array addObject : [NSString stringWithUTF8String : patterns] ] ;
                    patterns += strlen(patterns) + 1;
                }
                filters += strlen(filters) + 1;
            }
            [panel setAllowedFileTypes : types_array];
        }

        free(g_noc_file_dialog_ret);
        g_noc_file_dialog_ret = NULL;
        if ([panel runModal] == NSModalResponseOK) {
            url = [panel URL];
            utf8_path = [[url path]UTF8String];
            g_noc_file_dialog_ret = strdup(utf8_path);
        }

        [pool release] ;
        return g_noc_file_dialog_ret;
    }
#endif


#endif




    // LabSound stuff


// Returns input, output
    inline std::pair<lab::AudioStreamConfig, lab::AudioStreamConfig> GetDefaultAudioDeviceConfiguration(const bool with_input = false)
    {
        using namespace lab;
        AudioStreamConfig inputConfig;
        AudioStreamConfig outputConfig;

        const std::vector<AudioDeviceInfo> audioDevices = lab::MakeAudioDeviceList();
        const uint32_t default_output_device = lab::GetDefaultOutputAudioDeviceIndex();
        const uint32_t default_input_device = lab::GetDefaultInputAudioDeviceIndex();

        AudioDeviceInfo defaultOutputInfo, defaultInputInfo;
        for (auto& info : audioDevices)
        {
            if (info.index == default_output_device) defaultOutputInfo = info;
            else if (info.index == default_input_device) defaultInputInfo = info;
        }

        if (defaultOutputInfo.index != -1)
        {
            outputConfig.device_index = defaultOutputInfo.index;
            outputConfig.desired_channels = std::min(uint32_t(2), defaultOutputInfo.num_output_channels);
            outputConfig.desired_samplerate = defaultOutputInfo.nominal_samplerate;
        }

        if (with_input)
        {
            if (defaultInputInfo.index != -1)
            {
                inputConfig.device_index = defaultInputInfo.index;
                inputConfig.desired_channels = std::min(uint32_t(1), defaultInputInfo.num_input_channels);
                inputConfig.desired_samplerate = defaultInputInfo.nominal_samplerate;
            }
            else
            {
                throw std::invalid_argument("the default audio input device was requested but none were found");
            }
        }

        return { inputConfig, outputConfig };
    }



    shared_ptr<lab::AudioNode> NodeFactory(const std::string& n)
    {
        AudioContext& ac = *g_audio_context.get();
        if (n == "ADSR") return std::make_shared<lab::ADSRNode>(ac);
        if (n == "Analyser") return std::make_shared<lab::AnalyserNode>(ac);
        //if (n == "AudioBasicProcessor") return std::make_shared<lab::AudioBasicProcessorNode>(ac);
        //if (n == "AudioHardwareSource") return std::make_shared<lab::ADSRNode>(ac);
        if (n == "BiquadFilter") return std::make_shared<lab::BiquadFilterNode>(ac);
        if (n == "ChannelMerger") return std::make_shared<lab::ChannelMergerNode>(ac);
        if (n == "ChannelSplitter") return std::make_shared<lab::ChannelSplitterNode>(ac);
        if (n == "Clip") return std::make_shared<lab::ClipNode>(ac);
        if (n == "Convolver") return std::make_shared<lab::ConvolverNode>(ac);
        //if (n == "DefaultAudioDestination") return std::make_shared<lab::DefaultAudioDestinationNode>(ac);
        if (n == "Delay") return std::make_shared<lab::DelayNode>(ac);
        if (n == "Diode") return std::make_shared<lab::DiodeNode>(ac);
        if (n == "DynamicsCompressor") return std::make_shared<lab::DynamicsCompressorNode>(ac);
        //if (n == "Function") return std::make_shared<lab::FunctionNode>(ac);
        if (n == "Gain") return std::make_shared<lab::GainNode>(ac);
        //if (n == "Granulation") return std::make_shared<lab::GranulationNode>(ac);
        if (n == "Noise") return std::make_shared<lab::NoiseNode>(ac);
        //if (n == "OfflineAudioDestination") return std::make_shared<lab::OfflineAudioDestinationNode>(ac);
        if (n == "Oscillator") return std::make_shared<lab::OscillatorNode>(ac);
        //if (n == "Panner") return std::make_shared<lab::PannerNode>(ac);
#ifdef PD
        if (n == "PureData") return std::make_shared<lab::PureDataNode>(ac);
#endif
        if (n == "PeakCompressor") return std::make_shared<lab::PeakCompNode>(ac);
        //if (n == "PingPongDelay") return std::make_shared<lab::PingPongDelayNode>(ac);
        if (n == "PolyBLEP") return std::make_shared<lab::PolyBLEPNode>(ac);
        if (n == "PowerMonitor") return std::make_shared<lab::PowerMonitorNode>(ac);
        if (n == "PWM") return std::make_shared<lab::PWMNode>(ac);
        //if (n == "Recorder") return std::make_shared<lab::RecorderNode>(ac);
        if (n == "SampledAudio") return std::make_shared<lab::SampledAudioNode>(ac);
        if (n == "Sfxr") return std::make_shared<lab::SfxrNode>(ac);
        if (n == "Spatialization") return std::make_shared<lab::SpatializationNode>(ac);
        if (n == "SpectralMonitor") return std::make_shared<lab::SpectralMonitorNode>(ac);
        if (n == "StereoPanner") return std::make_shared<lab::StereoPannerNode>(ac);
        if (n == "SuperSaw") return std::make_shared<lab::SupersawNode>(ac);
        if (n == "WaveShaper") return std::make_shared<lab::WaveShaperNode>(ac);
        return {};
    }




    // Audio Nodes and Noodles

    struct AudioGuiNode;
    struct AudioPin
    {
        enum class Kind { Setting, Param, BusIn, BusOut };

        string name, shortName;

        Kind kind;
        entt::entity pin_id;
        entt::entity node;
        shared_ptr<lab::AudioSetting> setting;
        shared_ptr<lab::AudioParam> param;

        std::string value;

        ImVec2 pos_cs; // canvas space position
    };

    // the whole point of an ECS is that the AudioPin should be a component associated with the entity LOL
    map<entt::entity, AudioPin> g_pins;

    struct AudioGuiNode
    {
        entt::entity gui_node_id;

        std::string name;
        ImVec2 pos;
        ImVec2 lr;   // recalculated at the beginning of each interaction cycle

        shared_ptr<lab::AudioNode> node;
        vector<entt::entity> pins;

        AudioGuiNode(char const* const n)  : name(n) { Create(n); }
        AudioGuiNode(const std::string& n) : name(n) { Create(n.c_str()); }
        AudioGuiNode(char const* const n,  shared_ptr<lab::AudioNode> node) { Create(node, n); }
        AudioGuiNode(const std::string& n, shared_ptr<lab::AudioNode> node) { Create(node, n.c_str()); }
        ~AudioGuiNode() = default;

        void Create(const std::string& name) { Create(name.c_str()); }
        void Create(char const* const name_)
        {
            // create the node, and fetch its parameters and settings
            auto node = NodeFactory(name_);
            if (node)
                Create(node, name_);
        }

        void Create(shared_ptr<lab::AudioNode> audio_node, char const*const name_)
        {
            name = name_;
            gui_node_id = g_registry.create();

            node = audio_node;
            if (!node)
                return;

            ContextRenderLock r(g_audio_context.get(), "LabSoundGraphToy_init");

            if (!strncmp(name_, "Analyser", 8))
            {
                g_audio_context->addAutomaticPullNode(audio_node);
            }

            vector<string> names = audio_node->settingNames();
            vector<string> shortNames = audio_node->settingShortNames();
            auto settings = audio_node->settings();
            int c = (int) settings.size();
            for (int i = 0; i < c; ++i)
            {
                entt::entity pin_id = g_registry.create();
                pins.emplace_back(pin_id);
                g_pins[pin_id] = AudioPin{
                    names[i], shortNames[i], AudioPin::Kind::Setting,
                    pin_id, gui_node_id,
                    settings[i]
                };

                char buff[64] = { '\0' };

                lab::AudioSetting::Type type = settings[i]->type();
                if (type == lab::AudioSetting::Type::Float)
                {
                    sprintf(buff, "%f", settings[i]->valueFloat());
                }
                else if (type == lab::AudioSetting::Type::Integer)
                {
                    sprintf(buff, "%d", settings[i]->valueUint32());
                }
                else if (type == lab::AudioSetting::Type::Bool)
                {
                    sprintf(buff, "%s", settings[i]->valueBool() ? "1" : "0");
                }
                else if (type == lab::AudioSetting::Type::Enumeration)
                {
                    char const* const* names = settings[i]->enums();
                    sprintf(buff, "%s", names[settings[i]->valueUint32()]);
                }

                g_pins[pin_id].value.assign(buff);
            }
            names = audio_node->paramNames();
            shortNames = audio_node->paramShortNames();
            auto params = audio_node->params();
            c = (int) params.size();
            for (int i = 0; i < c; ++i)
            {
                entt::entity pin_id = g_registry.create();
                pins.emplace_back(pin_id);
                g_pins[pin_id] = AudioPin{
                    names[i], shortNames[i], AudioPin::Kind::Param,
                    pin_id, gui_node_id,
                    shared_ptr<AudioSetting>(),
                    params[i]
                    };

                char buff[64];
                sprintf(buff, "%f", params[i]->value(r));
                g_pins[pin_id].value.assign(buff);
            }
            c = (int) audio_node->numberOfInputs();
            for (int i = 0; i < c; ++i)
            {
                entt::entity pin_id = g_registry.create();
                pins.emplace_back(pin_id);
                g_pins[pin_id] = AudioPin{
                    "", "", AudioPin::Kind::BusIn,
                    pin_id, gui_node_id,
                    shared_ptr<AudioSetting>(),
                    };
            }
            c = (int) audio_node->numberOfOutputs();
            for (int i = 0; i < c; ++i)
            {
                entt::entity pin_id = g_registry.create();
                pins.emplace_back(pin_id);
                g_pins[pin_id] = AudioPin{
                    "", "", AudioPin::Kind::BusOut,
                    pin_id, gui_node_id,
                    shared_ptr<AudioSetting>(),
                    };
            }
        }
    };

    // the whole point of an ECS is that the AudioGuiNode should be a component associated with the entity LOL
    map<entt::entity, shared_ptr<AudioGuiNode>> g_nodes;


    struct Connection
    {
        entt::entity id;
        entt::entity pin_from;
        entt::entity node_from;
        entt::entity pin_to;
        entt::entity node_to;
    };

    // the whole point of an ECS is that the Connection should be a component associated with the entity LOL
    map<entt::entity, Connection> g_connections;






    enum class WorkType
    {
        Nop, CreateRealtimeContext, CreateNode, DeleteNode, SetParam,
        SetFloatSetting, SetIntSetting, SetBoolSetting, SetBusSetting,
        ConnectAudioOutToAudioIn, ConnectAudioOutToParamIn,
        DisconnectInFromOut,
        Start, Bang,
    };

    struct Work
    {
        WorkType type = WorkType::Nop;
        std::string name;
        entt::entity input_node = g_no_entity;
        entt::entity output_node = g_no_entity;
        entt::entity param_pin = g_no_entity;
        entt::entity setting_pin = g_no_entity;
        entt::entity connection_id = g_no_entity;
        float float_value = 0.f;
        int int_value = 0;
        bool bool_value = false;
        ImVec2 canvas_pos = { 0, 0 };

        void eval()
        {
            if (type == WorkType::CreateRealtimeContext)
            {
                const auto defaultAudioDeviceConfigurations = GetDefaultAudioDeviceConfiguration();
                g_audio_context = lab::MakeRealtimeAudioContext(defaultAudioDeviceConfigurations.second, defaultAudioDeviceConfigurations.first);
                auto node = std::make_shared<AudioGuiNode>(name, g_audio_context->device());
                node->pos = canvas_pos;
                g_nodes[node->gui_node_id] = node;
                printf("CreateRealtimeContext %d\n", node->gui_node_id);
            }
            else if (type == WorkType::CreateNode)
            {
                auto node = std::make_shared<AudioGuiNode>(name);
                node->pos = canvas_pos;
                g_nodes[node->gui_node_id] = node;
                printf("CreateNode %s %d\n", name.c_str(), node->gui_node_id);
            }
            else if (type == WorkType::SetParam)
            {
                if (param_pin != g_no_entity)
                {
                    auto pin = g_pins.find(param_pin);
                    if (pin != g_pins.end())
                        pin->second.param->setValue(float_value);
                    printf("SetParam(%f) %d\n", float_value, param_pin);
                }
            }
            else if (type == WorkType::SetFloatSetting)
            {
                if (setting_pin != g_no_entity)
                {
                    auto setting = g_pins.find(setting_pin);
                    if (setting != g_pins.end())
                        setting->second.setting->setFloat(float_value);
                    printf("SetFloatSetting(%f) %d\n", float_value, setting_pin);
                }
            }
            else if (type == WorkType::SetIntSetting)
            {
                if (setting_pin != g_no_entity)
                {
                    auto setting = g_pins.find(setting_pin);
                    if (setting != g_pins.end())
                        setting->second.setting->setUint32(int_value);
                    printf("SetIntSetting(%d) %d\n", int_value, setting_pin);
                }
            }
            else if (type == WorkType::SetBoolSetting)
            {
                if (setting_pin != g_no_entity)
                {
                    auto setting = g_pins.find(setting_pin);
                    if (setting != g_pins.end())
                        setting->second.setting->setBool(bool_value);
                    printf("SetBoolSetting(%s) %d\n", bool_value? "true": "false", setting_pin);
                }
            }
            else if (type == WorkType::SetBusSetting)
            {
                if (setting_pin != g_no_entity && name.length() > 0)
                {
                    auto setting = g_pins.find(setting_pin);
                    if (setting != g_pins.end())
                    {
                        auto soundClip = lab::MakeBusFromFile(name.c_str(), false);
                        setting->second.setting->setBus(soundClip.get());
                        printf("SetBusSetting %d\n", setting_pin);
                    }
                }
            }
            else if (type == WorkType::ConnectAudioOutToAudioIn)
            {
                auto in_it = g_nodes.find(input_node);
                auto out_it = g_nodes.find(output_node);
                if (in_it != g_nodes.end() && out_it != g_nodes.end())
                {
                    g_audio_context->connect(in_it->second->node, out_it->second->node, 0, 0);
                    printf("ConnectAudioOutToAudioIn %d %d\n", input_node, output_node);
                }
            }
            else if (type == WorkType::ConnectAudioOutToParamIn)
            {
                auto in_it = g_pins.find(param_pin);
                auto out_it = g_nodes.find(output_node);
                if (in_it != g_pins.end() && out_it != g_nodes.end())
                {
                    g_audio_context->connectParam(in_it->second.param, out_it->second->node, 0);
                    printf("ConnectAudioOutToParamIn %d %d\n", param_pin, output_node);
                }
            }
            else if (type == WorkType::DisconnectInFromOut)
            {
                auto conn_it = g_connections.find(connection_id);
                if (conn_it != g_connections.end())
                {
                    entt::entity input_node = conn_it->second.node_to;
                    entt::entity output_node = conn_it->second.node_from;
                    entt::entity input_pin = conn_it->second.pin_to;
                    entt::entity output_pin = conn_it->second.pin_from;
                    auto in_it = g_nodes.find(input_node);
                    auto out_it = g_nodes.find(output_node);
                    auto in_pin = g_pins.find(input_pin);
                    auto out_pin = g_pins.find(output_pin);
                    if (in_it != g_nodes.end() && out_it != g_nodes.end() &&
                        in_pin != g_pins.end() && out_pin != g_pins.end())
                    {
                        if ((in_pin->second.kind == AudioPin::Kind::BusIn) && (out_pin->second.kind == AudioPin::Kind::BusOut))
                        {
                            g_audio_context->disconnect(in_it->second->node, out_it->second->node, 0, 0);
                            printf("DisconnectInFromOut (bus from bus) %d %d\n", input_node, output_node);
                        }
                        else if ((in_pin->second.kind == AudioPin::Kind::Param) && (out_pin->second.kind == AudioPin::Kind::BusOut))
                        {
                            g_audio_context->disconnectParam(in_pin->second.param, out_it->second->node, 0);
                            printf("DisconnectInFromOut (param from bus) %d %d\n", input_node, output_node);
                        }
                    }

                    g_connections.erase(connection_id);
                    g_registry.destroy(connection_id);
                }
            }
            else if (type == WorkType::DeleteNode)
            {
                // force full disconnection
                auto in_it = g_nodes.find(input_node);
                if (in_it != g_nodes.end())
                {
                    g_audio_context->disconnect(in_it->second->node);

                    for (map<entt::entity, AudioPin>::iterator i = g_pins.begin(); i != g_pins.end(); ++i)
                    {
                        if (i->second.node == input_node)
                        {
                            g_registry.destroy(i->second.pin_id);
                            i = g_pins.erase(i);
                            if (i == g_pins.end())
                                break;
                        }
                    }

                    printf("DeleteNode %d\n", input_node);
                    g_nodes.erase(in_it);
                }
                auto out_it = g_nodes.find(output_node);
                if (out_it != g_nodes.end())
                {
                    g_audio_context->disconnect(out_it->second->node);

                    for (map<entt::entity, AudioPin>::iterator i = g_pins.begin(); i != g_pins.end(); ++i)
                    {
                        if (i->second.node == output_node)
                        {
                            g_registry.destroy(i->second.pin_id);
                            i = g_pins.erase(i);
                            if (i == g_pins.end())
                                break;
                        }
                    }

                    printf("DeleteNode %d\n", output_node);
                    g_nodes.erase(out_it);
                }
                for (map<entt::entity, Connection>::iterator i = g_connections.begin(); i != g_connections.end(); ++i)
                {
                    if ((i->second.node_from == input_node) || (i->second.node_to == input_node) ||
                        (i->second.node_from == output_node) || (i->second.node_to == output_node))
                    {
                        g_registry.destroy(i->second.id);
                        i = g_connections.erase(i);
                        if (i == g_connections.end())
                            break;
                    }
                }
                if (input_node != g_no_entity)
                    g_registry.destroy(input_node);
                if (output_node != g_no_entity)
                    g_registry.destroy(output_node);
            }
            else if (type == WorkType::Start)
            {
                auto in_it = g_nodes.find(input_node);
                lab::AudioScheduledSourceNode* n = dynamic_cast<lab::AudioScheduledSourceNode*>(in_it->second->node.get());

                if (n)
                {
                    if (n->isPlayingOrScheduled())
                        n->stop(0);
                    else
                        n->start(0);
                }

                printf("Start %d\n", input_node);
            }
            else if (type == WorkType::Bang)
            {
                ContextRenderLock r(g_audio_context.get(), "pin read");
                auto in_it = g_nodes.find(input_node);
                in_it->second->node->_scheduler.start(0);
                printf("Bang %d\n", input_node);
            }
        }
    };

    vector<Work> g_pending_work;













    bool NoodleMenu(ImVec2 canvas_pos)
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
                    Work work;
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


    void EditPin(entt::entity pin)
    {
        auto pin_it = g_pins.find(pin);
        if (pin_it == g_pins.end())
            return;

        auto node_it = g_nodes.find(pin_it->second.node);
        if (node_it == g_nodes.end())
            return;

        char buff[256];
        sprintf(buff, "%s:%s", node_it->second->name.c_str(), pin_it->second.name.c_str());

        ImGui::OpenPopup(buff);
        if (ImGui::BeginPopupModal(buff, nullptr, ImGuiWindowFlags_NoCollapse))
        {

            bool accept = false;
            char const* const* names = nullptr;

            lab::AudioSetting::Type type = lab::AudioSetting::Type::None;
            if (pin_it->second.kind == AudioPin::Kind::Setting)
                type = pin_it->second.setting->type();

            if (pin_it->second.kind == AudioPin::Kind::Param || type == lab::AudioSetting::Type::Float)
            {
                if (ImGui::InputFloat("###EditPinParamFloat", &g_edit_pin_float,
                    0, 0, 5,
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsScientific))
                {
                    accept = true;
                }
            }
            else if (type == lab::AudioSetting::Type::Integer)
            {
                if (ImGui::InputInt("###EditPinParamInt", &g_edit_pin_int,
                    0, 0,
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsScientific))
                {
                    accept = true;
                }
            }
            else if (type == lab::AudioSetting::Type::Bool)
            {
                if (ImGui::Checkbox("###EditPinParamBool", &g_edit_pin_bool))
                {
                    accept = true;
                }
            }
            else if (type == lab::AudioSetting::Type::Enumeration)
            {
                names = pin_it->second.setting->enums();
                int enum_idx = g_edit_pin_int;
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
                        g_edit_pin_int = clicked;
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
                        Work work;
                        work.setting_pin = pin;
                        work.name.assign(file);
                        work.type = WorkType::SetBusSetting;
                        g_pending_work.emplace_back(work);
                        g_edit_pin = g_no_entity;
                    }
                }
            }

            if ((type != lab::AudioSetting::Type::Bus) && (accept || ImGui::Button("OK")))
            {
                Work work;
                work.param_pin = pin;
                work.setting_pin = pin;
                buff[0] = '\0'; // clear the string

                if (pin_it->second.kind == AudioPin::Kind::Param)
                {
                    sprintf(buff, "%f", g_edit_pin_float);
                    work.type = WorkType::SetParam;
                    work.float_value = g_edit_pin_float;
                }
                else if (type == lab::AudioSetting::Type::Float)
                {
                    sprintf(buff, "%f", g_edit_pin_float);
                    work.type = WorkType::SetFloatSetting;
                    work.float_value = g_edit_pin_float;
                }
                else if (type == lab::AudioSetting::Type::Integer)
                {
                    sprintf(buff, "%d", g_edit_pin_int);
                    work.type = WorkType::SetIntSetting;
                    work.int_value = g_edit_pin_int;
                }
                else if (type == lab::AudioSetting::Type::Bool)
                {
                    sprintf(buff, "%s", g_edit_pin_bool ? "True" : "False");
                    work.type = WorkType::SetBoolSetting;
                    work.bool_value = g_edit_pin_bool;
                }
                else if (type == lab::AudioSetting::Type::Enumeration)
                {
                    sprintf(buff, "%s", names[g_edit_pin_int]);
                    work.type = WorkType::SetIntSetting;
                    work.int_value = g_edit_pin_int;
                }

                pin_it->second.value.assign(buff);

                g_pending_work.emplace_back(work);
                g_edit_pin = g_no_entity;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                g_edit_pin = g_no_entity;

            ImGui::EndPopup();
        }
    }

    void EditConnection(entt::entity connection)
    {
        auto conn_it = g_connections.find(connection);
        if (conn_it == g_connections.end())
        {
            g_edit_connection = g_no_entity;
            return;
        }

        ImGui::OpenPopup("Connection");
        if (ImGui::BeginPopupModal("Connection", nullptr, ImGuiWindowFlags_NoCollapse))
        {
            if (ImGui::Button("Delete"))
            {
                Work work{ WorkType::DisconnectInFromOut };
                work.connection_id = connection;
                g_pending_work.emplace_back(work);
                g_edit_connection = g_no_entity;
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                g_edit_connection = g_no_entity;

            ImGui::EndPopup();
        }
    }

    void EditNode(entt::entity node)
    {
        auto node_it = g_nodes.find(node);
        if (node_it == g_nodes.end())
        {
            g_edit_node = g_no_entity;
            return;
        }

        char buff[256];
        sprintf(buff, "%s Node", node_it->second->name.c_str());
        ImGui::OpenPopup(buff);
        if (ImGui::BeginPopupModal(buff, nullptr, ImGuiWindowFlags_NoCollapse))
        {
            if (ImGui::Button("Delete"))
            {
                Work work{ WorkType::DeleteNode };
                work.input_node = node;
                g_pending_work.emplace_back(work);
                g_edit_node = g_no_entity;
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                g_edit_node = g_no_entity;

            ImGui::EndPopup();
        }
    }





    void DrawSpectrum(shared_ptr<AudioGuiNode> gui_node, ImVec2 ul_ws, ImVec2 lr_ws, float canvas_scale, ImDrawList* drawList)
    {
        float rounding = 3;
        ul_ws.x += 5 * canvas_scale; ul_ws.y += 5 * canvas_scale;
        lr_ws.x = ul_ws.x + (lr_ws.x - ul_ws.x) * 0.5f;
        lr_ws.y -= 5 * canvas_scale;
        drawList->AddRect(ul_ws, lr_ws, ImColor(255, 128, 0, 255), rounding, 15, 2);

        int left = static_cast<int>(ul_ws.x + 2 * canvas_scale);
        int right = static_cast<int>(lr_ws.x - 2 * canvas_scale);
        int pixel_width = right - left;
        lab::AnalyserNode* node = dynamic_cast<lab::AnalyserNode*>(gui_node->node.get());
        static vector<uint8_t> bins;
        if (bins.size() != pixel_width)
            bins.resize(pixel_width);

        // fetch the byte frequency data because it is normalized vs the analyser's min/maxDecibels.
        node->getByteFrequencyData(bins, true);

        float base = lr_ws.y - 2 * canvas_scale;
        float height = lr_ws.y - ul_ws.y - 4 * canvas_scale;
        drawList->PathClear();
        for (int x = 0; x < pixel_width; ++x)
        {
            drawList->PathLineTo(ImVec2(x + float(left), base - height * bins[x] / 255.0f));
        }
        drawList->PathStroke(ImColor(255, 255, 0, 255), false, 2);
    }










    // GraphEditor stuff

    struct HoverState
    {
        void reset(entt::entity en)
        {
            g_hover.node_id = en;
            g_originating_pin_id = en;
            g_hover.pin_id = en;
            g_hover.pin_label_id = en;
            pin_pos_cs = { 0, 0 };
            node_menu = false;
            bang = false;
            play = false;
            node.reset();
        }

        std::shared_ptr<AudioGuiNode> node;
        ImVec2 pin_pos_cs = { 0,0 };
        entt::entity node_id;
        entt::entity pin_id;
        entt::entity pin_label_id;
        entt::entity connection_id;
        bool node_menu = false;
        bool bang = false;
        bool play = false;
    } g_hover;

    const float terminal_w = 20;
    static bool g_interacting_with_canvas = false;
    static bool g_click_initiated = false;
    static bool g_click_ended = false;

    void init()
    {
        static bool once = true;
        if (!once)
            return;

        once = false;
        g_ent_main_window = g_registry.create();
        g_id_main_window = ImGui::GetID(&g_ent_main_window);
        g_bg_color = ImColor(0.2f, 0.2f, 0.2f, 1.0f);
        g_id_main_invis = ImGui::GetID(&g_id_main_invis);
        g_no_entity = g_registry.create();
        g_hover.reset(g_no_entity);
        g_edit_pin = g_no_entity;
        g_edit_connection = g_no_entity;
        g_edit_node = g_no_entity;

        ImGuiIO& io = ImGui::GetIO();
        ImGuiWindow* win = ImGui::GetCurrentWindow();
        ImRect edit_rect = win->ContentRegionRect;
        float y = (edit_rect.Max.y + edit_rect.Min.y) * 0.5f - 64;
        Work work = { WorkType::CreateRealtimeContext, "AudioDestination" };
        work.canvas_pos = ImVec2{ edit_rect.Max.x - 300, y };
        g_pending_work.push_back(work);
    }

    void update_mouse_state() 
    {
        //---------------------------------------------------------------------
        // determine hovered, dragging, pressed, and released, as well as
        // window local coordinate and canvas local coordinate

        ImGuiIO& io = ImGui::GetIO();
        ImGuiWindow* win = ImGui::GetCurrentWindow();
        ImRect edit_rect = win->ContentRegionRect;

        g_click_ended = false;
        g_click_initiated = false;
        g_in_canvas = false;
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
        {
            ImGui::SetCursorScreenPos(edit_rect.Min);
            ImGui::PushID(g_id_main_invis);
            bool result = ImGui::InvisibleButton("Noodle", edit_rect.GetSize());
            ImGui::PopID();
            if (result)
            {
                //printf("button released\n");
                g_dragging_node = false;
                g_click_ended = true;
            }
            g_in_canvas = ImGui::IsItemHovered();

            if (g_in_canvas)
            {
                g_mouse_ws = io.MousePos - ImGui::GetCurrentWindow()->Pos;
                g_mouse_cs = (g_mouse_ws - g_canvas_pixel_offset_ws) / g_canvas_scale;

                if (io.MouseDown[0] && io.MouseDownOwned[0])
                {
                    if (!g_dragging)
                    {
                        //printf("button clicked\n");
                        g_click_initiated = true;
                        g_initial_click_pos_ws = io.MousePos;
                        g_canvas_clickpos_cs = g_mouse_cs;
                        g_canvas_clicked_pixel_offset_ws = g_canvas_pixel_offset_ws;
                    }

                    g_dragging = true;
                }
                else
                    g_dragging = false;
            }
            else
                g_dragging = false;
        }
        else
            g_dragging = false;

        if (!g_dragging)
            g_hover.node.reset();
    }


    void update_hovers()
    {
        // if a node is not already hovered, and the button is not pressed, go look for hover highlights
        bool find_highlights = g_dragging_wire;
        find_highlights |= !g_hover.node && !g_dragging;

        if (find_highlights)
        {
            g_hover.bang = false;
            g_hover.play = false;
            g_hover.node_menu = false;
            g_edit_node = g_no_entity;
            g_hover.node_id = g_no_entity;
            g_hover.pin_id = g_no_entity;
            g_hover.pin_label_id = g_no_entity;
            g_hover.connection_id = g_no_entity;
            float mouse_x_cs = g_mouse_cs.x;
            float mouse_y_cs = g_mouse_cs.y;
            for (auto& i : g_nodes)
            {
                ImVec2 ul = i.second->pos;
                ImVec2 lr = i.second->lr;
                if (mouse_x_cs >= ul.x && mouse_x_cs <= (lr.x + terminal_w) && mouse_y_cs >= (ul.y - 20) && mouse_y_cs <= lr.y)
                {
                    ImVec2 pin_ul;

                    // check output pins
                    if (mouse_x_cs >= lr.x)
                    {
                        pin_ul.x = lr.x - 4;// -4 * g_canvas_scale;
                        pin_ul.y = ul.y + 8;// +8 * g_canvas_scale;
                        for (auto& j : i.second->pins)
                        {
                            auto pin_it = g_pins.find(j);
                            if (pin_it->second.kind != lab::noodle::AudioPin::Kind::BusOut)
                                continue;

                            if (mouse_y_cs >= pin_ul.y && mouse_y_cs <= (pin_ul.y + terminal_w))
                            {
                                g_hover.pin_id = pin_it->second.pin_id;
                                g_hover.node_id = i.second->gui_node_id;
                                g_hover.node = i.second;
                                if (g_originating_pin_id == g_no_entity)
                                {
                                    g_hover.pin_pos_cs.x = pin_ul.x + terminal_w * 0.5f;
                                    g_hover.pin_pos_cs.y = pin_ul.y + terminal_w * 0.5f;
                                }
                                break;
                            }
                            pin_ul.y += 20;
                        }
                    }
                    if (g_hover.node_id != g_no_entity)
                        break;

                    // not hovered on an output, x is outside, continue to next node
                    if (mouse_x_cs > lr.x)
                        continue;

                    if (mouse_x_cs < lr.x)
                    {
                        // check input pins
                        pin_ul = ul;
                        pin_ul.x -= 3 * g_canvas_scale;
                        pin_ul.y += 8 * g_canvas_scale;
                        for (auto& j : i.second->pins)
                        {
                            auto pin_it = g_pins.find(j);
                            if (pin_it->second.kind != lab::noodle::AudioPin::Kind::BusIn)
                                continue;

                            if ((mouse_x_cs < ul.x + terminal_w) && mouse_y_cs >= pin_ul.y && mouse_y_cs <= (pin_ul.y + terminal_w))
                            {
                                g_hover.pin_id = pin_it->second.pin_id;
                                g_hover.node_id = i.second->gui_node_id;
                                if (g_originating_pin_id == g_no_entity)
                                {
                                    g_hover.pin_pos_cs.x = pin_ul.x + terminal_w * 0.5f;
                                    g_hover.pin_pos_cs.y = pin_ul.y + terminal_w * 0.5f;
                                }
                                break;
                            }

                            pin_ul.y += 20;
                        }

                        if (g_hover.node_id == g_no_entity)
                        {
                            // keep looking for param pins
                            for (auto& j : i.second->pins)
                            {
                                auto pin_it = g_pins.find(j);
                                if (pin_it->second.kind != lab::noodle::AudioPin::Kind::Param)
                                    continue;

                                if (mouse_y_cs >= pin_ul.y && mouse_y_cs <= (pin_ul.y + terminal_w))
                                {
                                    if (mouse_x_cs < ul.x + terminal_w)
                                    {
                                        g_hover.pin_id = pin_it->second.pin_id;
                                        g_hover.node_id = i.second->gui_node_id;
                                    }
                                    else if (mouse_x_cs < ul.x + 150)
                                    {
                                        g_hover.pin_label_id = pin_it->second.pin_id;
                                        g_hover.node_id = i.second->gui_node_id;
                                    }

                                    if (g_originating_pin_id == g_no_entity)
                                    {
                                        g_hover.pin_pos_cs.x = pin_ul.x + terminal_w * 0.5f;
                                        g_hover.pin_pos_cs.y = pin_ul.y + terminal_w * 0.5f;
                                    }
                                    break;
                                }

                                pin_ul.y += 20;
                            }
                        }

                        if (g_hover.node_id == g_no_entity)
                        {
                            // keep looking, for settings
                            pin_ul = ul;
                            pin_ul.x += 200;
                            pin_ul.y += 8;
                            for (auto& j : i.second->pins)
                            {
                                auto pin_it = g_pins.find(j);
                                if (pin_it->second.kind != lab::noodle::AudioPin::Kind::Setting)
                                    continue;

                                if (mouse_y_cs >= pin_ul.y && mouse_y_cs <= (pin_ul.y + terminal_w))
                                {
                                    if (mouse_x_cs > pin_ul.x)
                                    {
                                        g_hover.pin_id = g_no_entity;
                                        g_hover.pin_label_id = pin_it->second.pin_id;
                                        g_hover.node_id = i.second->gui_node_id;
                                    }
                                    break;
                                }

                                pin_ul.y += 20;
                            }
                        }
                    }

                    if (mouse_y_cs < ul.y)
                    {
                        float testx = ul.x + 20;
                        bool play = false;
                        bool bang = false;
                        bool scheduled = i.second->node->isScheduledNode();

                        // in banner
                        if (mouse_x_cs < testx && scheduled)
                        {
                            g_hover.play = true;
                            play = true;
                        }

                        if (scheduled)
                            testx += 20;

                        if (!play && mouse_x_cs < testx && i.second->node->_scheduler._onStart)
                        {
                            g_hover.bang = true;
                            bang = true;
                        }

                        if (!play && !bang)
                        {
                            g_hover.node_menu = true;
                        }
                    }

                    g_hover.node_id = i.second->gui_node_id;
                    g_hover.node = i.second;
                    break;
                }
            }

            if (g_hover.node_id == g_no_entity)
            {
                // no node or node furniture hovered, check connections
                for (auto i : g_connections)
                {
                    entt::entity from_pin = i.second.pin_from;
                    entt::entity to_pin = i.second.pin_to;
                    auto from_it = g_pins.find(i.second.pin_from);
                    auto to_it = g_pins.find(i.second.pin_to);
                    ImVec2 from_pos = from_it->second.pos_cs + ImVec2(16, 10);
                    ImVec2 to_pos = to_it->second.pos_cs + ImVec2(0, 10);

                    ImVec2 p0 = from_pos;
                    ImVec2 p1 = { p0.x + 64, p0.y };
                    ImVec2 p3 = to_pos;
                    ImVec2 p2 = { p3.x - 64, p3.y };
                    ImVec2 closest = ImBezierClosestPointCasteljau(p0, p1, p2, p3, g_mouse_cs, 10);
                    
                    ImVec2 delta = g_mouse_cs - closest;
                    float d = delta.x * delta.x + delta.y * delta.y;
                    if (d < 100)
                        g_hover.connection_id = i.second.id;
                }
            }
        }
    }



    bool run_noodles()
    {
        init();

        //---------------------------------------------------------------------
        // ensure node sizes are up to date

        for (auto& i : g_nodes)
        {
            int in_height = 0;
            int middle_height = 0;
            int out_height = 0;

            for (auto& j : i.second->pins)
            {
                auto pin_it = g_pins.find(j);
                switch (pin_it->second.kind)
                {
                case lab::noodle::AudioPin::Kind::BusIn: in_height += 1; break;
                case lab::noodle::AudioPin::Kind::BusOut: out_height += 1; break;
                case lab::noodle::AudioPin::Kind::Param: in_height += 1; break;
                case lab::noodle::AudioPin::Kind::Setting: middle_height += 1; break;
                }
            }

            int height = in_height > middle_height ? in_height : middle_height;
            if (out_height > height)
                height = out_height;

            float width = 256;
            if (middle_height > 0)
                width += 128;

            i.second->lr = i.second->pos + ImVec2{ width, 16 + 20 * (float)height };
        }

        //---------------------------------------------------------------------

        ImGuiIO& io = ImGui::GetIO();
        ImGuiWindow* win = ImGui::GetCurrentWindow();
        ImRect edit_rect = win->ContentRegionRect;

        //---------------------------------------------------------------------
        // Create a canvas

        float height = edit_rect.Max.y - edit_rect.Min.y
                        - ImGui::GetTextLineHeightWithSpacing()   // space for the time bar
                        - ImGui::GetTextLineHeightWithSpacing();  // space for horizontal scroller

        bool rv = ImGui::BeginChild(g_id_main_window, ImVec2(0, height), false,
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
            ImU32 grid_color = ImColor(1.f, std::max(1.f, g_canvas_scale * g_canvas_scale), 0.f, 0.25f * g_canvas_scale);
            float GRID_SX = 128.0f * g_canvas_scale;
            float GRID_SY = 128.0f * g_canvas_scale;
            ImVec2 VIEW_POS = ImGui::GetWindowPos();
            ImVec2 VIEW_SIZE = ImGui::GetWindowSize();

            drawList->AddRectFilled(VIEW_POS, VIEW_POS + VIEW_SIZE, g_bg_color);

            for (float x = fmodf(g_canvas_pixel_offset_ws.x, GRID_SX); x < VIEW_SIZE.x; x += GRID_SX)
            {
                drawList->AddLine(ImVec2(x, 0.0f) + VIEW_POS, ImVec2(x, VIEW_SIZE.y) + VIEW_POS, grid_color);
            }
            for (float y = fmodf(g_canvas_pixel_offset_ws.y, GRID_SY); y < VIEW_SIZE.y; y += GRID_SY)
            {
                drawList->AddLine(ImVec2(0.0f, y) + VIEW_POS, ImVec2(VIEW_SIZE.x, y) + VIEW_POS, grid_color);
            }
        }
        drawList->ChannelsSetCurrent(ChannelContent);

        //---------------------------------------------------------------------
        // run the Imgui portion of the UI

        update_mouse_state();
        ImVec2 window_origin_offset_ws = ImGui::GetWindowPos();

        if (g_dragging || g_dragging_wire || g_dragging_node)
        {
            if (g_dragging_wire)
                update_hovers();
        }
        else
        {
            bool imgui_business = NoodleMenu(g_mouse_cs);
            if (imgui_business)
            {
                g_dragging = false;
                g_hover.node.reset();
                g_hover.reset(g_no_entity);
                g_edit_pin = g_no_entity;
            }
            else if (g_edit_pin != g_no_entity)
            {
                ImGui::SetNextWindowPos(g_initial_click_pos_ws);
                EditPin(g_edit_pin);
                g_dragging = false;
                g_hover.node.reset();
                g_hover.reset(g_no_entity);
            }
            else if (g_edit_connection != g_no_entity)
            {
                ImGui::SetNextWindowPos(g_initial_click_pos_ws);
                EditConnection(g_edit_connection);
                g_dragging = false;
                g_hover.node.reset();
                g_hover.reset(g_no_entity);
            }
            else if (g_edit_node != g_no_entity)
            {
                ImGui::SetNextWindowPos(g_initial_click_pos_ws);
                EditNode(g_edit_node);
                g_dragging = false;
            }
            else
                update_hovers();
        }

        if (!g_dragging && g_dragging_wire)
        {
            if (g_originating_pin_id != g_no_entity && g_hover.pin_id != g_no_entity)
            {
                auto from = g_pins.find(g_originating_pin_id);
                auto to = g_pins.find(g_hover.pin_id);

                if (from == g_pins.end() || to == g_pins.end())
                {
                    printf("from and/or to are not valid pins\n");
                }
                else
                {
                    /// @TODO this logic can be duplicated into hover to turn the wire red for disallowed connections
                    // ensure to is the connection destination
                    if (from->second.kind == AudioPin::Kind::BusIn || from->second.kind == AudioPin::Kind::Param)
                    {
                        std::swap(to, from);
                        std::swap(g_originating_pin_id, g_hover.pin_id);
                    }

                    // check if a valid connection is requested
                    AudioPin::Kind to_kind = to->second.kind;
                    AudioPin::Kind from_kind = from->second.kind;
                    if (to_kind == AudioPin::Kind::Setting || to_kind == AudioPin::Kind::BusOut ||
                        from_kind == AudioPin::Kind::BusIn || from_kind == AudioPin::Kind::Param || from_kind == AudioPin::Kind::Setting)
                    {
                        printf("invalid connection request\n");
                    }
                    else
                    {
                        if (to_kind == AudioPin::Kind::BusIn)
                        {
                            Work work{ WorkType::ConnectAudioOutToAudioIn };
                            work.input_node = to->second.node;
                            work.output_node = from->second.node;
                            g_pending_work.emplace_back(work);
                        }
                        else if (to_kind == AudioPin::Kind::Param)
                        {
                            Work work{ WorkType::ConnectAudioOutToParamIn };
                            work.input_node = to->second.node;
                            work.output_node = from->second.node;
                            work.param_pin = to->second.pin_id;
                            g_pending_work.emplace_back(work);
                        }

                        entt::entity wire = g_registry.create();
                        g_connections[wire] = Connection{
                            wire,
                            g_originating_pin_id,
                            from->second.node,
                            g_hover.pin_id,
                            to->second.node
                        };
                    }
                }
            }
            g_dragging_wire = false;
            g_originating_pin_id = g_no_entity;
        }
        else if (g_click_ended)
        {
            if (g_hover.connection_id != g_no_entity)
            {
                g_edit_connection = g_hover.connection_id;
            }
            else if (g_hover.node_menu)
            {
                g_edit_node = g_hover.node_id;
            }
        }

        g_interacting_with_canvas = !g_hover.node;
        if (!g_interacting_with_canvas)
        {
            // nodes and wires
            if (g_click_initiated)
            {
                if (g_hover.bang)
                {
                    Work work{ WorkType::Bang };
                    work.input_node = g_hover.node->gui_node_id;
                    g_pending_work.push_back(work);
                }
                if (g_hover.play)
                {
                    Work work{ WorkType::Start };
                    work.input_node = g_hover.node->gui_node_id;
                    g_pending_work.push_back(work);
                }
                if (g_hover.pin_id != g_no_entity)
                {
                    g_dragging_wire = true;
                    g_dragging_node = false;
                    g_node_initial_pos_cs = g_mouse_cs;
                    if (g_originating_pin_id == g_no_entity)
                        g_originating_pin_id = g_hover.pin_id;
                }
                else if (g_hover.pin_label_id != g_no_entity)
                {
                    // set mode to edit the value of the hovered pin
                    g_edit_pin = g_hover.pin_label_id;
                    auto pin_it = g_pins.find(g_edit_pin);
                    ContextRenderLock r(g_audio_context.get(), "pin read");
                    if (pin_it->second.kind == AudioPin::Kind::Param)
                        g_edit_pin_float = pin_it->second.param->value(r);
                    else if (pin_it->second.kind == AudioPin::Kind::Setting)
                    {
                        auto type = pin_it->second.setting->type();
                        if (type == lab::AudioSetting::Type::Float)
                        {
                            g_edit_pin_float = pin_it->second.setting->valueFloat();
                        }
                        else if (type == lab::AudioSetting::Type::Integer || type == lab::AudioSetting::Type::Enumeration)
                        {
                            g_edit_pin_int = pin_it->second.setting->valueUint32();
                        }
                        else if (type == lab::AudioSetting::Type::Bool)
                        {
                            g_edit_pin_bool = pin_it->second.setting->valueBool();
                        }
                    }
                }
                else
                {
                    g_dragging_wire = false;
                    g_dragging_node = true;
                    g_node_initial_pos_cs = g_hover.node->pos;
                }
            }

            if (g_dragging_node)
            {
                ImVec2 delta = g_mouse_cs - g_canvas_clickpos_cs;
                ImVec2 sz = g_hover.node->lr - g_hover.node->pos;
                g_hover.node->pos = g_node_initial_pos_cs + delta;
                g_hover.node->lr = g_hover.node->pos + sz;

                g_hover.node_id = g_hover.node->gui_node_id;  // force the color to be highlighting
            }
        }
        else
        {
            // if the interaction is with the canvas itself, offset and scale the canvas

            if (g_dragging)
            {
                if (fabsf(io.MouseDelta.x) > 0.f || fabsf(io.MouseDelta.y) > 0.f)
                {
                    // pull the pivot around
                    g_canvas_pixel_offset_ws = g_canvas_clicked_pixel_offset_ws - g_initial_click_pos_ws + io.MousePos;
                }
            }
            else if (g_in_canvas)
            {
                if (fabsf(io.MouseWheel) > 0.f)
                {
                    // scale using where the mouse is currently hovered as the pivot
                    float prev_scale = g_canvas_scale;
                    g_canvas_scale += io.MouseWheel * 0.1f;
                    g_canvas_scale = g_canvas_scale < 1.f / 16.f ? 1.f / 16.f : g_canvas_scale;
                    g_canvas_scale = g_canvas_scale > 1.f ? 1.f : g_canvas_scale;

                    // solve for off2
                    // (mouse - off1) / scale1 = (mouse - off2) / scale2 
                    g_canvas_pixel_offset_ws = g_mouse_ws - (g_mouse_ws - g_canvas_pixel_offset_ws) * (g_canvas_scale / prev_scale);
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
        text_color |= (uint32_t)(255 * 2 * (g_canvas_scale - 0.5f)) << 24;

        // draw pulled noodle

        drawList->ChannelsSetCurrent(ChannelNodes);

        for (auto i : g_connections)
        {
            entt::entity from_pin = i.second.pin_from;
            entt::entity to_pin = i.second.pin_to;
            auto from_it = g_pins.find(i.second.pin_from);
            auto to_it = g_pins.find(i.second.pin_to);
            ImVec2 from_pos = from_it->second.pos_cs + ImVec2(16, 10);
            ImVec2 to_pos = to_it->second.pos_cs + ImVec2(0, 10);

            ImVec2 p0 = window_origin_offset_ws + from_pos * g_canvas_scale + g_canvas_pixel_offset_ws;
            ImVec2 p1 = { p0.x + 64 * g_canvas_scale, p0.y };
            ImVec2 p3 = window_origin_offset_ws + to_pos * g_canvas_scale + g_canvas_pixel_offset_ws;
            ImVec2 p2 = { p3.x - 64 * g_canvas_scale, p3.y };
            drawList->AddBezierCurve(p0, p1, p2, p3, 0xffffffff, 2.f);
        }

        if (g_dragging_wire)
        {
            ImVec2 p0 = window_origin_offset_ws + g_hover.pin_pos_cs * g_canvas_scale + g_canvas_pixel_offset_ws;
            ImVec2 p1 = { p0.x + 64 * g_canvas_scale, p0.y };
            ImVec2 p3 = g_mouse_ws;
            ImVec2 p2 = { p3.x - 64 * g_canvas_scale, p3.y };
            drawList->AddBezierCurve(p0, p1, p2, p3, 0xffffffff, 2.f);
        }

        // draw nodes

        for (auto& i : g_nodes)
        {
            ImVec2 ul_ws = i.second->pos;
            ImVec2 lr_ws = i.second->lr;
            ul_ws = window_origin_offset_ws + ul_ws * g_canvas_scale + g_canvas_pixel_offset_ws;
            lr_ws = window_origin_offset_ws + lr_ws * g_canvas_scale + g_canvas_pixel_offset_ws;

            // draw node

            const float rounding = 10;
            drawList->AddRectFilled(ul_ws, lr_ws, ImColor(10, 20, 30, 128), rounding);
            drawList->AddRect(ul_ws, lr_ws, ImColor(255, g_hover.node_id == i.second->gui_node_id ? 255 : 0, 0, 255), rounding, 15, 2);

            if (!strncmp(i.second->name.c_str(), "Analyser", 8))
            {
                DrawSpectrum(i.second, ul_ws, lr_ws, g_canvas_scale, drawList);
            }

            // node banner

            if (g_canvas_scale > 0.5f)
            {
                float font_size = 16 * g_canvas_scale;
                ImVec2 label_pos = ul_ws;
                label_pos.y -= 20 * g_canvas_scale;

                if (i.second->node->isScheduledNode())
                {
                    const char* label = ">";
                    drawList->AddText(io.FontDefault, font_size, label_pos, text_color, label, label+1);
                    if (false)
                    {
                        Work work{ WorkType::Start };
                        work.input_node = i.second->gui_node_id;
                        g_pending_work.push_back(work);
                    }
                    label_pos.x += 20;
                }
                if (i.second->node->_scheduler._onStart)
                {
                    const char* label = "!";
                    drawList->AddText(io.FontDefault, font_size, label_pos, text_color, label, label + 1);
                    if (false)
                    {
                        Work work{ WorkType::Bang };
                        work.input_node = i.second->gui_node_id;
                        g_pending_work.push_back(work);
                    }
                    label_pos.x += 20;
                }

                drawList->AddText(io.FontDefault, font_size, label_pos, text_color, i.second->name.c_str(), i.second->name.c_str() + i.second->name.length());
            }

            // draw input pins

            ImVec2 pin_ul = ul_ws;
            pin_ul.x -= 3 * g_canvas_scale;
            pin_ul.y += 8 * g_canvas_scale;
            for (auto& j : i.second->pins)
            {
                auto pin_it = g_pins.find(j);
                if (pin_it->second.kind != lab::noodle::AudioPin::Kind::BusIn)
                    continue;

                pin_it->second.pos_cs = (pin_ul - window_origin_offset_ws - g_canvas_pixel_offset_ws) / g_canvas_scale;

                uint32_t fill = (pin_it->second.pin_id == g_hover.pin_id || pin_it->second.pin_id == g_originating_pin_id) ? 0xffffff : 0x000000;
                fill |= (uint32_t)(128 + 128 * sinf(pulse * 8)) << 24;
                DrawIcon(drawList, pin_ul, ImVec2{ pin_ul.x + terminal_w * g_canvas_scale, pin_ul.y + terminal_w * g_canvas_scale },
                         lab::noodle::IconType::Flow, false, 0xffffffff, fill);
                
                if (g_canvas_scale > 0.5f)
                {
                    float font_size = 16 * g_canvas_scale;
                    ImVec2 label_pos = pin_ul;
                    label_pos.x += 20 * g_canvas_scale;
                    drawList->AddText(io.FontDefault, font_size, label_pos, text_color, 
                        pin_it->second.shortName.c_str(), pin_it->second.shortName.c_str() + pin_it->second.shortName.length());
                }

                pin_ul.y += 20 * g_canvas_scale;
            }

            for (auto& j : i.second->pins)
            {
                auto pin_it = g_pins.find(j);
                if (pin_it->second.kind != lab::noodle::AudioPin::Kind::Param)
                    continue;

                pin_it->second.pos_cs = (pin_ul - window_origin_offset_ws - g_canvas_pixel_offset_ws) / g_canvas_scale;

                uint32_t fill = (pin_it->second.pin_id == g_hover.pin_id || pin_it->second.pin_id == g_originating_pin_id) ? 0xffffff : 0x000000;
                fill |= (uint32_t)(128 + 128 * sinf(pulse * 8)) << 24;
                DrawIcon(drawList, pin_ul, ImVec2{ pin_ul.x + terminal_w * g_canvas_scale, pin_ul.y + terminal_w * g_canvas_scale },
                    lab::noodle::IconType::Flow, false, 0xff00ff00, fill);

                if (g_canvas_scale > 0.5f)
                {
                    float font_size = 16 * g_canvas_scale;
                    ImVec2 label_pos = pin_ul;
                    label_pos.x += 23 * g_canvas_scale;
                    drawList->AddText(io.FontDefault, font_size, label_pos, text_color, 
                        pin_it->second.shortName.c_str(), pin_it->second.shortName.c_str() + pin_it->second.shortName.length());

                    label_pos.x += 50 * g_canvas_scale;
                    drawList->AddText(io.FontDefault, font_size, label_pos, text_color, 
                        pin_it->second.value.c_str(), pin_it->second.value.c_str() + pin_it->second.value.length());
                }

                pin_ul.y += 20 * g_canvas_scale;
            }

            // draw settings
            pin_ul = ul_ws;
            pin_ul.x += 200 * g_canvas_scale;
            pin_ul.y += 8 * g_canvas_scale;
            for (auto& j : i.second->pins)
            {
                auto pin_it = g_pins.find(j);
                if (pin_it->second.kind != lab::noodle::AudioPin::Kind::Setting)
                    continue;

                pin_it->second.pos_cs = (pin_ul - window_origin_offset_ws - g_canvas_pixel_offset_ws) / g_canvas_scale;

                uint32_t fill = (pin_it->second.pin_id == g_hover.pin_id || pin_it->second.pin_id == g_originating_pin_id) ? 0xffffff : 0x000000;
                fill |= (uint32_t)(128 + 128 * sinf(pulse * 8)) << 24;
                DrawIcon(drawList, pin_ul, ImVec2{ pin_ul.x + terminal_w * g_canvas_scale, pin_ul.y + terminal_w * g_canvas_scale },
                    lab::noodle::IconType::Grid, false, 0xff00ff00, fill);

                if (g_canvas_scale > 0.5f)
                {
                    float font_size = 16 * g_canvas_scale;
                    ImVec2 label_pos = pin_ul;
                    label_pos.x += 20 * g_canvas_scale;
                    drawList->AddText(io.FontDefault, font_size, label_pos, text_color, 
                        pin_it->second.shortName.c_str(), pin_it->second.shortName.c_str() + pin_it->second.shortName.length());

                    label_pos.x += 50 * g_canvas_scale;
                    drawList->AddText(io.FontDefault, font_size, label_pos, text_color, 
                        pin_it->second.value.c_str(), pin_it->second.value.c_str() + pin_it->second.value.length());
                }

                pin_ul.y += 20 * g_canvas_scale;
            }

            // draw output pins

            pin_ul.x = lr_ws.x - 4 * g_canvas_scale;
            pin_ul.y = ul_ws.y + 8 * g_canvas_scale;
            for (auto& j : i.second->pins)
            {
                auto pin_it = g_pins.find(j);
                if (pin_it->second.kind != lab::noodle::AudioPin::Kind::BusOut)
                    continue;

                pin_it->second.pos_cs = (pin_ul - window_origin_offset_ws - g_canvas_pixel_offset_ws) / g_canvas_scale;

                lr_ws = ImVec2{ ul_ws.x + 16, ul_ws.y + 16 };
                uint32_t fill = (pin_it->second.pin_id == g_hover.pin_id || pin_it->second.pin_id == g_originating_pin_id) ? 0xffffff : 0x000000;
                fill |= (uint32_t)(128 + 128 * sinf(pulse * 8)) << 24;
                DrawIcon(drawList, pin_ul, ImVec2{ pin_ul.x + terminal_w * g_canvas_scale, pin_ul.y + terminal_w * g_canvas_scale },
                         lab::noodle::IconType::Flow, false, 0xffffffff, fill);

                if (g_canvas_scale > 0.5f)
                {
                    float font_size = 16 * g_canvas_scale;
                    ImVec2 label_pos = pin_ul;
                    label_pos.x += 20 * g_canvas_scale;
                    drawList->AddText(io.FontDefault, font_size, label_pos, text_color, 
                        pin_it->second.shortName.c_str(), pin_it->second.shortName.c_str() + pin_it->second.shortName.length());
                }

                pin_ul.y += 20 * g_canvas_scale;
            }
        }

        // draw debug information
        drawList->AddRect(ImVec2{ 0,0 }, ImVec2{ 300, 250 }, 0xffffffff);
        static char buff[256];
        float y = 10;
        sprintf(buff, "pos %d %d", (int) g_mouse_cs.x, (int) g_mouse_cs.y);
        drawList->AddText(io.FontDefault, 16, ImVec2{ 10, y }, 0xffffffff, buff, buff + strlen(buff));
        sprintf(buff, "LMB %s%s%s", g_click_initiated ? "*" : "-", g_dragging ? "*" : "-", g_click_ended ? "*" : "-");
        drawList->AddText(io.FontDefault, 16, ImVec2{ 10, y += 20 }, 0xffffffff, buff, buff + 7);
        sprintf(buff, "canvas interaction: %s", g_interacting_with_canvas ? "*" : ".");
        drawList->AddText(io.FontDefault, 16, ImVec2{ 10, y += 20 }, 0xffffffff, buff, buff + 21);
        sprintf(buff, "wire dragging: %s", g_dragging_wire ? "*" : ".");
        drawList->AddText(io.FontDefault, 16, ImVec2{ 10, y += 20 }, 0xffffffff, buff, buff + 16);
        sprintf(buff, "node hovered: %s", g_hover.node_id != g_no_entity ? "*" : ".");
        drawList->AddText(io.FontDefault, 16, ImVec2{ 10, y += 20 }, 0xffffffff, buff, buff + 15);
        sprintf(buff, "originating pin id: %d", g_originating_pin_id);
        drawList->AddText(io.FontDefault, 16, ImVec2{ 10, y += 20 }, 0xffffffff, buff, buff + strlen(buff));
        sprintf(buff, "hovered pin id: %d", g_hover.pin_id);
        drawList->AddText(io.FontDefault, 16, ImVec2{ 10, y += 20 }, 0xffffffff, buff, buff + strlen(buff));
        sprintf(buff, "hovered pin label: %d", g_hover.pin_label_id);
        drawList->AddText(io.FontDefault, 16, ImVec2{ 10, y += 20 }, 0xffffffff, buff, buff + strlen(buff));
        sprintf(buff, "hovered connection: %d", g_hover.connection_id);
        drawList->AddText(io.FontDefault, 16, ImVec2{ 10, y += 20 }, 0xffffffff, buff, buff + strlen(buff));
        sprintf(buff, "hovered node menu: %s", g_hover.node_menu ? "*" : ".");
        drawList->AddText(io.FontDefault, 16, ImVec2{ 10, y += 20 }, 0xffffffff, buff, buff + strlen(buff));
        sprintf(buff, "edit connection: %d", g_edit_connection);
        drawList->AddText(io.FontDefault, 16, ImVec2{ 10, y += 20 }, 0xffffffff, buff, buff + strlen(buff));
        drawList->ChannelsSetCurrent(ChannelContent);

        // finish

        drawList->ChannelsMerge();
        EndChild();

        for (Work& work : g_pending_work)
            work.eval();

        g_pending_work.clear();

        return true;
    }


}} // lab::noodle
