
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#   define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <LabSound/LabSound.h>
#include <imgui.h>
#include <imgui_internal.h>

#include "imgui_node_editor.h"

#include <map>
#include <set>
#include <string>
#include <vector>

void GrooveStart(lab::AudioContext* context);


std::shared_ptr<lab::AudioNode> NodeFactory(const std::string& n)
{
    if (n == "ADSR") return std::make_shared<lab::ADSRNode>();
    if (n == "Analyser") return std::make_shared<lab::AnalyserNode>();
    //if (n == "AudioBasicProcessor") return std::make_shared<lab::AudioBasicProcessorNode>();
    //if (n == "AudioHardwareSource") return std::make_shared<lab::ADSRNode>();
    if (n == "BiquadFilter") return std::make_shared<lab::BiquadFilterNode>();
    if (n == "ChannelMerger") return std::make_shared<lab::ChannelMergerNode>();
    if (n == "ChannelSplitter") return std::make_shared<lab::ChannelSplitterNode>();
    if (n == "Clip") return std::make_shared<lab::ClipNode>();
    if (n == "Convolver") return std::make_shared<lab::Sound::ConvolverNode>();
    //if (n == "DefaultAudioDestination") return std::make_shared<lab::DefaultAudioDestinationNode>();
    if (n == "Delay") return std::make_shared<lab::DelayNode>();
    //if (n == "Diode") return std::make_shared<lab::DiodeNode>(); // @TODO subclass DiodeNode from WaveShaperNode
    if (n == "DynamicsCompressor") return std::make_shared<lab::DynamicsCompressorNode>();
    //if (n == "Function") return std::make_shared<lab::FunctionNode>();
    if (n == "Gain") return std::make_shared<lab::GainNode>();
    if (n == "Noise") return std::make_shared<lab::NoiseNode>();
    //if (n == "OfflineAudioDestination") return std::make_shared<lab::OfflineAudioDestinationNode>();
    if (n == "Oscillator") return std::make_shared<lab::OscillatorNode>();
    if (n == "Panner") return std::make_shared<lab::PannerNode>();
#ifdef PD
    if (n == "PureData") return std::make_shared<lab::PureDataNode>();
#endif
    if (n == "PeakCompressor") return std::make_shared<lab::PeakCompNode>();
    //if (n == "PingPongDelay") return std::make_shared<lab::PingPongDelayNode>(); @TODO subclass PingPongDelayNode from AudioNode
    if (n == "PowerMonitor") return std::make_shared<lab::PowerMonitorNode>();
    if (n == "PWM") return std::make_shared<lab::PWMNode>();
    if (n == "Recorder") return std::make_shared<lab::RecorderNode>();
    if (n == "SampledAudio") return std::make_shared<lab::SampledAudioNode>();
    if (n == "Sfxr") return std::make_shared<lab::SfxrNode>();
    if (n == "Spatialization") return std::make_shared<lab::SpatializationNode>();
    if (n == "SpectralMonitor") return std::make_shared<lab::SpectralMonitorNode>();
    if (n == "StereoPanner") return std::make_shared<lab::StereoPannerNode>();
    if (n == "SuperSaw") return std::make_shared<lab::SupersawNode>();
    if (n == "WaveShaper") return std::make_shared<lab::WaveShaperNode>();
    return {};
}


namespace ed = ax::NodeEditor;


enum class PinType { Input, Output, ParameterInput };
enum class IconType { Flow, Circle, Square, Grid, RoundSquare, Diamond };

void DrawIcon(ImDrawList* drawList, const ImVec2& a, const ImVec2& b, IconType type, bool filled, ImU32 color, ImU32 innerColor)
{
    struct rectf {
        float x, y, w, h;
        float center_x() const { return x + w * 0.5f; }
        float center_y() const { return y + h * 0.5f; }
    };

    rectf rect = { a.x, a.y, b.x - a.x, b.y - a.y };
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
        const rectf canvas {
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
                const auto p0 = ImVec2 { rect.center_x(), rect.center_y() } - ImVec2(r, r);
                const auto p1 = ImVec2 { rect.center_x(), rect.center_y() } + ImVec2(r, r);

                drawList->AddRectFilled(p0, p1, color, 0, 15 + extra_segments);
            }
            else
            {
                const auto r = 0.5f * rect.w / 2.0f - 0.5f;
                const auto p0 = ImVec2{ rect.center_x(), rect.center_y() } - ImVec2(r, r);
                const auto p1 = ImVec2{ rect.center_x(), rect.center_y() } + ImVec2(r, r);

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
                const auto p0 = ImVec2{ rect.center_x(), rect.center_y() } - ImVec2(r, r);
                const auto p1 = ImVec2{ rect.center_x(), rect.center_y() } + ImVec2(r, r);

                drawList->AddRectFilled(p0, p1, color, cr, 15);
            }
            else
            {
                const auto r = 0.5f * rect.w / 2.0f - 0.5f;
                const auto cr = r * 0.5f;
                const auto p0 = ImVec2{ rect.center_x(), rect.center_y() } - ImVec2(r, r);
                const auto p1 = ImVec2{ rect.center_x(), rect.center_y() } + ImVec2(r, r);

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


ImColor GetIconColor(PinType type)
{
    switch (type)
    {
    default:
    case PinType::Input:     return ImColor(255, 255, 255);
    case PinType::Output:     return ImColor(220, 48, 48);
    case PinType::ParameterInput:      return ImColor(68, 201, 156);
    }
};

void IconWidget(const ImVec2& size, IconType type, bool filled,
    const ImVec4& color/* = ImVec4(1, 1, 1, 1)*/, const ImVec4& innerColor/* = ImVec4(0, 0, 0, 0)*/)
{
    if (ImGui::IsRectVisible(size))
    {
        auto cursorPos = ImGui::GetCursorScreenPos();
        auto drawList = ImGui::GetWindowDrawList();
        DrawIcon(drawList, cursorPos, cursorPos + size, type, filled, ImColor(color), ImColor(innerColor));
    }

    ImGui::Dummy(size); // provide a bounds for ImGui interactions
}


void DrawPinIcon(PinType pinType, bool connected, int alpha)
{
    IconType iconType;
    ImColor  color = GetIconColor(pinType);
    color.Value.w = alpha / 255.0f;
    switch (pinType)
    {
    case PinType::Input:     iconType = IconType::Flow;   break;
    case PinType::Output:     iconType = IconType::Circle; break;
    case PinType::ParameterInput: iconType = IconType::Square; break;
    default:
        return;
    }

    constexpr float s_PinIconSize = 16;
    IconWidget(ImVec2(s_PinIconSize, s_PinIconSize), iconType, connected, color, ImColor(32, 32, 32, alpha));
};



// Struct to hold basic information about connection between
// pins. Note that connection (aka. link) has its own ID.
// This is useful later with dealing with selections, deletion
// or other operations.
struct LinkInfo
{
    ed::LinkId Id;
    ed::PinId  InputId;
    ed::PinId  OutputId;
};

static ed::EditorContext* g_Context = nullptr;    // Editor context, required to trace a editor state.
static bool               g_FirstFrame = true;    // Flag set for first frame only, some action need to be executed once.
static ImVector<LinkInfo> g_Links;                // List of live links. It is dynamic unless you want to create read-only view over nodes.
static int                g_NextLinkId = 100;     // Counter to help generate link ids. In real application this will probably based on pointer to user data structure.


void AudioGraphEditor_Initialize()
{
    ed::Config config;
    config.SettingsFile = "Simple.json";
    g_Context = ed::CreateEditor(&config);
}

void AudioGraphEditor_Finalize()
{
    ed::DestroyEditor(g_Context);
}

static int unique_node_id = 1;
static int unique_param_id = 1;

struct compare_pins {
    bool operator()(const ed::PinId& a, const ed::PinId& b) const {
        return a.AsPointer() < b.AsPointer();
    }
};
struct compare_nodes {
    bool operator()(const ed::NodeId& a, const ed::NodeId& b) const {
        return a.AsPointer() < b.AsPointer();
    }
};

struct AudioPin
{
    std::string name;
    ed::PinId id;
};

struct ImGuiVal
{
    std::string name;
    float float_val;
    int int_val;
    bool bool_val;
};

struct AudioNodeAccessor
{
    std::shared_ptr<lab::AudioNode> audio_node;

    std::vector<std::string> setting_names;
    std::vector<std::shared_ptr<lab::AudioSetting>> settings;  // 1:1 with names
    std::vector<ImGuiVal> setting_vals;

    std::vector<std::string> param_names;
    std::vector<std::shared_ptr<lab::AudioParam>> params;  // 1:1 with names
    std::vector<ImGuiVal> param_vals;

    void Create(char const*const name)
    {
        // create the node, and fetch its parameters and settings
        auto node = NodeFactory(name);
        if (node)
            Create(node);
    }
    void Create(const std::string& name)
    {
        // create the node, and fetch its parameters and settings
        auto node = NodeFactory(name.c_str());
        if (node)
            Create(node);
    }

    void Create(std::shared_ptr<lab::AudioNode> node)
    {
        audio_node = node;
        if (audio_node)
        {
            setting_names = audio_node->settings();
            param_names = audio_node->params();

            setting_vals.reserve(param_names.size());
            for (auto& setting : setting_names)
            {
                settings.push_back(audio_node->getSetting(setting.c_str()));
                setting_vals.push_back({ "##param" + std::to_string(unique_param_id++), 0.f });
            }

            param_vals.reserve(param_names.size());
            for (auto& param : param_names)
            {
                params.push_back(audio_node->getParam(param.c_str()));
                // preallocate an imgui identifier, and a buffer for imgui to write values into
                param_vals.push_back({ "##param" + std::to_string(unique_param_id++), 0.f });
            }
        }
    }

};

struct AudioGuiNode
{
    AudioGuiNode(char const* const n)
    : name(n), id(unique_node_id++), pos(static_cast<float>(unique_node_id)*10.f, 10)
    {
        entry.Create(n);
        create_pins();
    }

    AudioGuiNode(char const* const n, std::shared_ptr<lab::AudioNode> node)
        : name(n), id(unique_node_id++), pos(static_cast<float>(unique_node_id) * 10.f, 10)
    {
        entry.Create(node);
        create_pins();
    }

    AudioGuiNode(const std::string& n)
        : name(n), id(unique_node_id++), pos(static_cast<float>(unique_node_id) * 10.f, 10)
    {
        entry.Create(n.c_str());
        create_pins();
    }
    AudioGuiNode(const std::string& n, std::shared_ptr<lab::AudioNode> node)
        : name(n), id(unique_node_id++), pos(static_cast<float>(unique_node_id) * 10.f, 10)
    {
        entry.Create(node);
        create_pins();
    }

    void create_pins()
    {
        pinIn.clear();
        paramIn.clear();
        pinOut.clear();
        if (!entry.audio_node)
            return;

        size_t in_count = entry.audio_node->numberOfInputs();
        for (size_t i = 0; i < in_count; ++i)
            pinIn.push_back({ "in", ed::PinId(unique_node_id++) });

        for (auto& p : entry.param_names)
            paramIn.push_back({ p, ed::PinId(unique_node_id++) });

        size_t out_count = entry.audio_node->numberOfOutputs();
        for (size_t i = 0; i < out_count; ++i)
            pinOut.push_back({ "out", ed::PinId(unique_node_id++) });
    }

    ~AudioGuiNode() = default;

    std::string name;
    ed::NodeId id;
    std::vector<AudioPin> pinIn;
    std::vector<AudioPin> paramIn;
    std::vector<AudioPin> pinOut;
    ImVec2 pos;

    AudioNodeAccessor entry;
};
std::map<ed::NodeId, std::shared_ptr<AudioGuiNode>, compare_nodes> g_nodes;

struct AudioGuiNodeReverseLookup
{
    enum class PinKind { PIN_IN, PARAM_IN, PIN_OUT };
    PinKind pin_kind;
    std::weak_ptr<AudioGuiNode> associated_node;
};
std::map<ed::PinId, AudioGuiNodeReverseLookup, compare_pins> g_pin_reverse_lookup;

void map_pins(std::shared_ptr<AudioGuiNode> node)
{
    for (auto& i : node->pinIn)
        g_pin_reverse_lookup[i.id] = AudioGuiNodeReverseLookup{ AudioGuiNodeReverseLookup::PinKind::PIN_IN, node };
    for (auto& i : node->paramIn)
        g_pin_reverse_lookup[i.id] = AudioGuiNodeReverseLookup{ AudioGuiNodeReverseLookup::PinKind::PARAM_IN, node };
    for (auto& i : node->pinOut)
        g_pin_reverse_lookup[i.id] = AudioGuiNodeReverseLookup{ AudioGuiNodeReverseLookup::PinKind::PIN_OUT, node };
}

void unmap_pins(std::shared_ptr<AudioGuiNode> node)
{
    auto it = g_pin_reverse_lookup.begin();
    while (it != g_pin_reverse_lookup.end())
    {
        if (it->second.associated_node.lock().get() == node.get())
            it = g_pin_reverse_lookup.erase(it);
        else
            it++;
    }
}

std::unique_ptr<lab::AudioContext> g_audio_context;

enum class WorkType
{
    Nop, CreateRealtimeContext, CreateNode, DeleteNode, SetParam,
    SetFloatSetting, SetIntSetting, SetBoolSetting,
    ConnectAudioOutToAudioIn, ConnectAudioOutToParamIn,
    DisconnectAudioInFromOut, DisconnectParamInFromAudioOut,
};

struct Work
{
    WorkType type = WorkType::Nop;
    std::string name;
    ed::NodeId outputNode;
    ed::NodeId inputNode;
    std::shared_ptr<lab::AudioParam> param;
    std::shared_ptr<lab::AudioSetting> setting;
    float float_value;
    int int_value;
    bool bool_value;

    void eval()
    {
        if (type == WorkType::CreateRealtimeContext)
        {
            /// @TODO Make*AudioContext should return a shared pointer
            g_audio_context = lab::Sound::MakeRealtimeAudioContext(lab::Channels::Stereo);
            auto node = std::make_shared<AudioGuiNode>(name, g_audio_context->destination());
            g_nodes[node->id] = node;
            map_pins(node);
        }
        else if (type == WorkType::CreateNode)
        {
            auto node = std::make_shared<AudioGuiNode>(name);
            g_nodes[node->id] = node;
            map_pins(node);
        }
        else if (type == WorkType::SetParam)
        {
            if (param)
                param->setValue(float_value);
        }
        else if (type == WorkType::SetFloatSetting)
        {
            if (setting)
                setting->setFloat(float_value);
        }
        else if (type == WorkType::SetIntSetting)
        {
            if (setting)
                setting->setUint32(int_value);
        }
        else if (type == WorkType::SetBoolSetting)
        {
            if (setting)
                setting->setBool(bool_value);
        }
        else if (type == WorkType::ConnectAudioOutToAudioIn)
        {
            auto in_it = g_nodes.find(inputNode);
            auto out_it = g_nodes.find(outputNode);
            g_audio_context->connect(in_it->second->entry.audio_node, out_it->second->entry.audio_node, 0, 0);
        }
        else if (type == WorkType::DisconnectAudioInFromOut)
        {
            auto in_it = g_nodes.find(inputNode);
            auto out_it = g_nodes.find(outputNode);
            g_audio_context->disconnect(in_it->second->entry.audio_node, out_it->second->entry.audio_node, 0, 0);
        }
        else if (type == WorkType::DeleteNode)
        {
            // force full disconnection
            auto in_it = g_nodes.find(inputNode);
            if (in_it != g_nodes.end())
            {
                g_audio_context->disconnect(in_it->second->entry.audio_node);
                g_nodes.erase(in_it);
                unmap_pins(in_it->second);
            }
            auto out_it = g_nodes.find(outputNode);
            if (out_it != g_nodes.end())
            {
                g_audio_context->disconnect(out_it->second->entry.audio_node);
                g_nodes.erase(out_it);
                unmap_pins(out_it->second);
            }
        }
    }
};

std::vector<Work> g_pending_work;

void AudioGraphEditor_RunUI()
{
    if (g_FirstFrame)
    {
        g_pending_work.push_back(Work{ WorkType::CreateRealtimeContext, "AudioDestination" });
    }

    auto& io = ImGui::GetIO();

    //-------------------------------------------------------------------------
    // pop up menu is outside of the editor, so that it appears in the coordinate system of the node graph window
    if (ImGui::BeginPopupContextWindow())
    {
        ImGui::PushID("AudioGraphContextMenu");
        if (ImGui::BeginMenu("Create Node"))
        {
            std::string pressed = "";
            char const* const* nodes = lab::Sound::AudioNodeNames();
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
                g_pending_work.push_back(Work{ WorkType::CreateNode, pressed });
            }
        }
        ImGui::PopID();
        ImGui::EndPopup();
    }

    ed::SetCurrentEditor(g_Context);

    //-------------------------------------------------------------------------
    // Start interaction with editor.
    ed::Begin("LabSound Editor", ImVec2(0.0, 0.0f));

    int uniqueId = 1;

    //-------------------------------------------------------------------------
    // 1) Submit nodes
    for (auto& node_it : g_nodes)
    {
        auto& i = node_it.second;
        ed::BeginNode(i->id);
            ImGui::Text(i->name.c_str());
            if (i->entry.audio_node->isScheduledNode())
            {
                ImGui::SameLine();
                std::string n = ">###Start" + std::to_string(static_cast<int>((ptrdiff_t)i->id.AsPointer()));
                if (ImGui::Button(n.c_str()))
                {
                    lab::AudioScheduledSourceNode* n = reinterpret_cast<lab::AudioScheduledSourceNode*>(i->entry.audio_node.get());
                    n->start(0);
                }
            }
            if (i->entry.audio_node->hasBang())
            {
                ImGui::SameLine();
                std::string n = "!###Bang" + std::to_string(static_cast<int>((ptrdiff_t)i->id.AsPointer()));
                if (ImGui::Button(n.c_str()))
                {
                    i->entry.audio_node->bang();
                }
            }
            ImGui::TextUnformatted("-----------------------------");
            ImGui::BeginGroup();
            size_t c = i->pinIn.size();
            for (size_t idx = 0; idx < c; ++idx)
            {
                ed::BeginPin(i->pinIn[idx].id, ed::PinKind::Input);
                    ed::PinPivotAlignment(ImVec2(1.0f, 0.5f));
                    ed::PinPivotSize(ImVec2(0, 0));
                    constexpr float alpha = 1.f;
                    constexpr bool isLinked = false; /// @TODO should look up if pin is linked
                    DrawPinIcon(PinType::Input, isLinked, (int)(alpha * 255));
                ed::EndPin();
                ImGui::SameLine();
                ImGui::Text(i->pinIn[idx].name.c_str());
            }
            c = i->paramIn.size();
            for (size_t idx = 0; idx < c; ++idx)
            {
                ed::BeginPin(i->paramIn[idx].id, ed::PinKind::Input);
                    ed::PinPivotAlignment(ImVec2(1.0f, 0.5f));
                    ed::PinPivotSize(ImVec2(0, 0));
                    constexpr float alpha = 1.f;
                    constexpr bool isLinked = false; /// @TODO should look up if pin is linked
                    DrawPinIcon(PinType::ParameterInput, isLinked, (int)(alpha * 255));
                ed::EndPin();
                ImGui::SameLine();
                float x = ImGui::GetCursorPosX();
                ImGui::PushItemWidth(100.f);
                ImGui::Text(i->paramIn[idx].name.c_str());
                ImGui::SameLine();
                ImGui::SetCursorPosX(x + 100.f);
                if (ImGui::InputFloat(i->entry.param_vals[idx].name.c_str(), &i->entry.param_vals[idx].float_val,
                    0, 0, 5,
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsScientific))
                {
                    Work work{ WorkType::SetParam };
                    work.param = i->entry.params[idx];
                    work.float_value = i->entry.param_vals[idx].float_val;
                    g_pending_work.emplace_back(work);
                }
                ImGui::PopItemWidth();
            }
            ImGui::EndGroup();
            ImGui::SameLine();

            if (i->entry.setting_names.size())
            {
                ImGui::BeginGroup();
                size_t c = i->entry.setting_names.size();
                for (size_t idx = 0; idx < c; ++idx)
                {
                    float x = ImGui::GetCursorPosX();
                    ImGui::PushItemWidth(100.f);
                    ImGui::Text(i->entry.setting_names[idx].c_str());
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(x + 100.f);
                    lab::AudioSetting::Type type = i->entry.settings[idx]->type();
                    if (type == lab::AudioSetting::Type::Float)
                    {
                        if (ImGui::InputFloat(i->entry.setting_vals[idx].name.c_str(), &i->entry.setting_vals[idx].float_val,
                            0, 0, 5,
                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsScientific))
                        {
                            Work work{ WorkType::SetFloatSetting };
                            work.setting = i->entry.settings[idx];
                            work.float_value = i->entry.setting_vals[idx].float_val;
                            g_pending_work.emplace_back(work);
                        }
                    }
                    else if (type == lab::AudioSetting::Type::Integer)
                    {
                        if (ImGui::InputInt(i->entry.setting_vals[idx].name.c_str(), &i->entry.setting_vals[idx].int_val,
                            0, 0,
                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsScientific))
                        {
                            Work work{ WorkType::SetIntSetting };
                            work.setting = i->entry.settings[idx];
                            work.int_value = i->entry.setting_vals[idx].int_val;
                            g_pending_work.emplace_back(work);
                        }
                    }
                    else if (type == lab::AudioSetting::Type::Bool)
                    {
                        if (ImGui::Checkbox(i->entry.setting_vals[idx].name.c_str(), &i->entry.setting_vals[idx].bool_val))
                        {
                            Work work{ WorkType::SetBoolSetting };
                            work.setting = i->entry.settings[idx];
                            work.bool_value = i->entry.setting_vals[idx].bool_val;
                            g_pending_work.emplace_back(work);
                        }
                    }
                    else if (type == lab::AudioSetting::Type::Enumeration)
                    {
                        ed::Suspend();
                        char const* const* names = i->entry.settings[idx]->enums();
                        int enum_idx = i->entry.setting_vals[idx].int_val;
                        if (ImGui::BeginMenu(names[enum_idx]))
                        {
                            std::string pressed = "";
                            enum_idx = 0;
                            int clicked = -1;
                            for (; *names != nullptr; ++names, ++enum_idx)
                            {
                                if (ImGui::MenuItem(*names))
                                {
                                    clicked = enum_idx;
                                }
                            }
                            ImGui::EndMenu();
                            if (clicked >= 0)
                            {
                                i->entry.setting_vals[idx].int_val = clicked;
                                Work work{ WorkType::SetIntSetting };
                                work.setting = i->entry.settings[idx];
                                work.int_value = clicked;
                                g_pending_work.emplace_back(work);
                            }
                        }
                        ed::Resume();
                    }
                    ImGui::PopItemWidth();
                }
                ImGui::EndGroup();
                ImGui::SameLine();
            }

            ImGui::BeginGroup();
            for (auto& p : i->pinOut)
            {
                ImGui::Text(p.name.c_str());
                ImGui::SameLine();
                ed::BeginPin(p.id, ed::PinKind::Output);
                    ed::PinPivotAlignment(ImVec2(1.0f, 0.5f));
                    ed::PinPivotSize(ImVec2(0, 0));
                    constexpr float alpha = 1.f;
                    constexpr bool isLinked = false; /// @TODO should look up if pin is linked
                    DrawPinIcon(PinType::Output, isLinked, (int)(alpha * 255));
                ed::EndPin();
            }
            ImGui::EndGroup();
        ed::EndNode();
    }

    //-------------------------------------------------------------------------
    // Submit Links
    for (auto& linkInfo : g_Links)
        ed::Link(linkInfo.Id, linkInfo.InputId, linkInfo.OutputId);

    //
    // 2) Handle interactions
    //

    //-------------------------------------------------------------------------
    // Handle creation action, returns true if editor want to create new object (node or link)
    if (ed::BeginCreate())
    {
        ed::PinId inputPinId, outputPinId;
        if (ed::QueryNewLink(&inputPinId, &outputPinId))
        {
            // QueryNewLink returns true if editor want to create new link between pins.
            //
            // Link can be created only for two valid pins, it is up to you to
            // validate if connection make sense. Editor is happy to make any.
            //
            // Link always goes from input to output. User may choose to drag
            // link from output pin or input pin. This determine which pin ids
            // are valid and which are not:
            //   * input valid, output invalid - user started to drag new link from input pin
            //   * input invalid, output valid - user started to drag new link from output pin
            //   * input valid, output valid   - user dragged link over other pin, can be validated

            if (inputPinId && outputPinId) // both are valid, let's accept link
            {
                ///@TODO
                //if (pinOwner(inputPinId).compatible(pinOwner(outputPinId))
                //{
                    // ed::AcceptNewItem() return true when user release mouse button.
                    if (ed::AcceptNewItem())
                    {
                        // Since we accepted new link, lets add one to our list of links.
                        g_Links.push_back({ ed::LinkId(g_NextLinkId++), inputPinId, outputPinId });

                        // Draw new link.
                        ed::Link(g_Links.back().Id, g_Links.back().InputId, g_Links.back().OutputId);

                        auto in_it = g_pin_reverse_lookup.find(inputPinId);
                        auto out_it = g_pin_reverse_lookup.find(outputPinId);

                        bool audioToAudio = in_it != g_pin_reverse_lookup.end() && out_it != g_pin_reverse_lookup.end();
                        if (audioToAudio)
                        {
                            /// @TODO is the graph_ed in/out sense reversed?
                            std::shared_ptr<AudioGuiNode> in_node = out_it->second.associated_node.lock();
                            std::shared_ptr<AudioGuiNode> out_node = in_it->second.associated_node.lock();
                            if (in_node->entry.audio_node && out_node->entry.audio_node)
                            {
                                Work work{ WorkType::ConnectAudioOutToAudioIn };
                                work.inputNode = in_node->id;
                                work.outputNode = out_node->id;
                                g_pending_work.emplace_back(work);
                            }
                        }
/*                        else if (audioToParam)
                        {
                            Work work{ WorkType::ConnectAudioOutToParamIn };
                            work.node = node;
                            work.sourceNode = source;
                            g_pending_work.emplace_back(work);
                        }*/
                    }
                //}
                //else
                //{
                //  ed::RejectNewItem();    // let the editor provide visible feedback due to an incompatible link
                //}
            }
        }
    }
    ed::EndCreate(); // Wraps up object creation action handling.

    //-------------------------------------------------------------------------
    // Handle deletion action
    if (ed::BeginDelete())
    {
        // There may be many links marked for deletion, let's loop over them.
        ed::LinkId deletedLinkId;
        while (ed::QueryDeletedLink(&deletedLinkId))
        {
            // If you agree that link can be deleted, accept deletion.
            if (ed::AcceptDeletedItem())
            {
                // Then remove link from your data.
                for (auto& link : g_Links)
                {
                    if (link.Id == deletedLinkId)
                    {
                        g_Links.erase(&link);

                        auto in_it = g_pin_reverse_lookup.find(link.InputId);
                        auto out_it = g_pin_reverse_lookup.find(link.OutputId);

                        bool audioToAudio = in_it != g_pin_reverse_lookup.end() && out_it != g_pin_reverse_lookup.end();
                        if (audioToAudio)
                        {
                            std::shared_ptr<AudioGuiNode> in_node = in_it->second.associated_node.lock();
                            std::shared_ptr<AudioGuiNode> out_node = out_it->second.associated_node.lock();
                            if (in_node->entry.audio_node && out_node->entry.audio_node)
                            {
                                Work work{ WorkType::ConnectAudioOutToAudioIn };
                                work.inputNode = in_node->id;
                                work.outputNode = out_node->id;
                                g_pending_work.emplace_back(work);
                            }
                        }
/*                        else if (audioToParam)
                        {
                            Work work{ WorkType::ConnectAudioOutToParamIn };
                            work.node = node;
                            work.sourceNode = source;
                            g_pending_work.emplace_back(work);
                        }*/
                        break;
                    }
                }
            }

            // You may reject link deletion by calling:
            // ed::RejectDeletedItem();
        }

        ed::NodeId deletedNodeId;
        while (ed::QueryDeletedNode(&deletedNodeId))
        {
            Work work{ WorkType::DeleteNode };
            work.inputNode = deletedNodeId;
            g_pending_work.emplace_back(work);
        }
    }
    ed::EndDelete(); // Wrap up deletion action

    //-------------------------------------------------------------------------
    // End of interaction with editor.
    ed::End();

    if (g_FirstFrame)
    {
        ed::NavigateToContent(0.0f);
        g_FirstFrame = false;
    }

    ed::SetCurrentEditor(nullptr);
    // ImGui::ShowMetricsWindow();

    for (Work& work : g_pending_work)
        work.eval();
    g_pending_work.clear();
}
