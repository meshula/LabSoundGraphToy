
#include "LabSoundInterface.h"
#include "lab_imgui_ext.hpp"

#include <LabSound/LabSound.h>

#include <stdio.h>


using std::shared_ptr;
using std::string;
using std::vector;

//--------------------------------------------------------------
// AudioPins are created for every created node
// There is one noodle pin for every AudioPin
struct AudioPin
{
    int output_index = 0;
    std::shared_ptr<lab::AudioSetting> setting;
    std::shared_ptr<lab::AudioParam> param;
};


struct NodeReverseLookup
{
    std::map<std::string, entt::entity> input_pin_map;
    std::map<std::string, entt::entity> output_pin_map;
    std::map<std::string, entt::entity> param_pin_map;
};

std::map<entt::entity, NodeReverseLookup> g_node_reverse_lookups;

std::unique_ptr<lab::AudioContext> g_audio_context;

std::unique_ptr<entt::registry> g_ls_registry;
entt::registry& Registry()
{
    if (!g_ls_registry)
    {
        g_ls_registry = std::make_unique<entt::registry>();
    }
    return *g_ls_registry.get();
}

// Returns input, output
std::pair<lab::AudioStreamConfig, lab::AudioStreamConfig> GetDefaultAudioDeviceConfiguration(const bool with_input)
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

shared_ptr<lab::AudioNode> NodeFactory(const string& n)
{
    lab::AudioContext& ac = *g_audio_context.get();
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
    if (n == "Granulation") return std::make_shared<lab::GranulationNode>(ac);
    if (n == "Noise") return std::make_shared<lab::NoiseNode>(ac);
    //if (n == "OfflineAudioDestination") return std::make_shared<lab::OfflineAudioDestinationNode>(ac);
    if (n == "Oscillator") return std::make_shared<lab::OscillatorNode>(ac);
    if (n == "Panner") return std::make_shared<lab::PannerNode>(ac);
#ifdef PD
    if (n == "PureData") return std::make_shared<lab::PureDataNode>(ac);
#endif
    if (n == "PeakCompressor") return std::make_shared<lab::PeakCompNode>(ac);
    //if (n == "PingPongDelay") return std::make_shared<lab::PingPongDelayNode>(ac);
    if (n == "PolyBLEP") return std::make_shared<lab::PolyBLEPNode>(ac);
    if (n == "PowerMonitor") return std::make_shared<lab::PowerMonitorNode>(ac);
    if (n == "PWM") return std::make_shared<lab::PWMNode>(ac);
    if (n == "Recorder") return std::make_shared<lab::RecorderNode>(ac);
    if (n == "SampledAudio") return std::make_shared<lab::SampledAudioNode>(ac);
    if (n == "Sfxr") return std::make_shared<lab::SfxrNode>(ac);
    if (n == "Spatialization") return std::make_shared<lab::SpatializationNode>(ac);
    if (n == "SpectralMonitor") return std::make_shared<lab::SpectralMonitorNode>(ac);
    if (n == "StereoPanner") return std::make_shared<lab::StereoPannerNode>(ac);
    if (n == "SuperSaw") return std::make_shared<lab::SupersawNode>(ac);
    if (n == "WaveShaper") return std::make_shared<lab::WaveShaperNode>(ac);
    return {};
}




static constexpr float node_border_radius = 4.f;
void DrawSpectrum(entt::entity id, ImVec2 ul_ws, ImVec2 lr_ws, float scale, ImDrawList* drawList)
{
    entt::registry& reg = *g_ls_registry.get();
    std::shared_ptr<lab::AudioNode> audio_node = reg.get<std::shared_ptr<lab::AudioNode>>(id);
    ul_ws.x += 5 * scale; ul_ws.y += 5 * scale;
    lr_ws.x = ul_ws.x + (lr_ws.x - ul_ws.x) * 0.5f;
    lr_ws.y -= 5 * scale;
    drawList->AddRect(ul_ws, lr_ws, ImColor(255, 128, 0, 255), node_border_radius, 15, 2);

    int left = static_cast<int>(ul_ws.x + 2 * scale);
    int right = static_cast<int>(lr_ws.x - 2 * scale);
    int pixel_width = right - left;
    lab::AnalyserNode* node = dynamic_cast<lab::AnalyserNode*>(audio_node.get());
    static std::vector<uint8_t> bins;
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


void CreateEntities(shared_ptr<lab::AudioNode> audio_node, lab::noodle::Node& node, entt::entity audio_node_id)
{
    if (!audio_node)
        return;

    entt::registry& registry = Registry();

    auto reverse_it = g_node_reverse_lookups.find(audio_node_id);
    if (reverse_it == g_node_reverse_lookups.end())
    {
        g_node_reverse_lookups[audio_node_id] = NodeReverseLookup{};
        reverse_it = g_node_reverse_lookups.find(audio_node_id);
    }
    auto& reverse = reverse_it->second;

    lab::ContextRenderLock r(g_audio_context.get(), "LabSoundGraphToy_init");
    if (nullptr != dynamic_cast<lab::AnalyserNode*>(audio_node.get()))
    {
        g_audio_context->addAutomaticPullNode(audio_node);
        registry.emplace<lab::noodle::NodeRender>(audio_node_id, lab::noodle::NodeRender{
            [](entt::entity id, lab::noodle::vec2 ul_ws, lab::noodle::vec2 lr_ws, float scale, void* drawList) {
                DrawSpectrum(id, {ul_ws.x, ul_ws.y}, {lr_ws.x, lr_ws.y}, scale, reinterpret_cast<ImDrawList*>(drawList));
            } });
    }

    //---------- inputs

    int c = (int)audio_node->numberOfInputs();
    for (int i = 0; i < c; ++i)
    {
        entt::entity pin_id = registry.create();
        node.pins.push_back(pin_id);
        std::string name = ""; // audio_node->input(i)->name(); @TODO an IDL for all the things
        reverse.input_pin_map[name] = pin_id;
        registry.emplace<lab::noodle::Name>(pin_id, name);
        registry.emplace<lab::noodle::Pin>(pin_id, lab::noodle::Pin{
            lab::noodle::Pin::Kind::BusIn,
            lab::noodle::Pin::DataType::Bus,
            "",
            pin_id, audio_node_id,
            });
        registry.emplace<AudioPin>(pin_id, AudioPin{ 0 });
    }

    //---------- outputs

    c = (int)audio_node->numberOfOutputs();
    for (int i = 0; i < c; ++i)
    {
        entt::entity pin_id = registry.create();
        node.pins.push_back(pin_id);
        std::string name = audio_node->output(i)->name();
        reverse.output_pin_map[name] = pin_id;
        registry.emplace<lab::noodle::Name>(pin_id, name);
        registry.emplace<lab::noodle::Pin>(pin_id, lab::noodle::Pin{
            lab::noodle::Pin::Kind::BusOut,
            lab::noodle::Pin::DataType::Bus,
            "",
            pin_id, audio_node_id,
            });
        registry.emplace<AudioPin>(pin_id, AudioPin{ i });
    }

    //---------- settings

    vector<string> names = audio_node->settingNames();
    vector<string> shortNames = audio_node->settingShortNames();
    auto settings = audio_node->settings();
    c = (int)settings.size();
    for (int i = 0; i < c; ++i)
    {
        char buff[64] = { '\0' };
        char const* const* enums = nullptr;

        lab::noodle::Pin::DataType dataType = lab::noodle::Pin::DataType::Float;
        lab::AudioSetting::Type type = settings[i]->type();
        if (type == lab::AudioSetting::Type::Float)
        {
            dataType = lab::noodle::Pin::DataType::Float;
            sprintf(buff, "%f", settings[i]->valueFloat());
        }
        else if (type == lab::AudioSetting::Type::Integer)
        {
            dataType = lab::noodle::Pin::DataType::Integer;
            sprintf(buff, "%d", settings[i]->valueUint32());
        }
        else if (type == lab::AudioSetting::Type::Bool)
        {
            dataType = lab::noodle::Pin::DataType::Bool;
            sprintf(buff, "%s", settings[i]->valueBool() ? "1" : "0");
        }
        else if (type == lab::AudioSetting::Type::Enumeration)
        {
            dataType = lab::noodle::Pin::DataType::Enumeration;
            enums = settings[i]->enums();
            sprintf(buff, "%s", enums[settings[i]->valueUint32()]);
        }
        else if (type == lab::AudioSetting::Type::Bus)
        {
            dataType = lab::noodle::Pin::DataType::Bus;
            strcpy(buff, "...");
        }

        entt::entity pin_id = registry.create();
        node.pins.push_back(pin_id);
        registry.emplace<AudioPin>(pin_id, AudioPin{ 0, settings[i] });
        registry.emplace<lab::noodle::Name>(pin_id, names[i]);
        registry.emplace<lab::noodle::Pin>(pin_id, lab::noodle::Pin {
            lab::noodle::Pin::Kind::Setting,
            dataType,
            shortNames[i],
            pin_id, audio_node_id,
            buff,
            enums
            });
    }

    //---------- params

    names = audio_node->paramNames();
    shortNames = audio_node->paramShortNames();
    auto params = audio_node->params();
    c = (int)params.size();
    for (int i = 0; i < c; ++i)
    {
        char buff[64];
        sprintf(buff, "%f", params[i]->value());
        entt::entity pin_id = registry.create();
        reverse.param_pin_map[names[i]] = pin_id;
        node.pins.push_back(pin_id);
        registry.emplace<AudioPin>(pin_id, AudioPin{ 0,
            shared_ptr<lab::AudioSetting>(),
            params[i],
            });
        registry.emplace<lab::noodle::Name>(pin_id, names[i]);
        registry.emplace<lab::noodle::Pin>(pin_id, lab::noodle::Pin{
            lab::noodle::Pin::Kind::Param,
            lab::noodle::Pin::DataType::Float,
            shortNames[i],
            pin_id, audio_node_id,
            buff
            });
    }
}

void LabSoundProvider::pin_set_setting_bus_value(const std::string& node_name, const std::string& setting_name, const std::string& path)
{
    entt::entity node = entity_for_node_named(node_name);
    if (node == entt::null || !registry().valid(node))
        return;

    auto n = registry().get<std::shared_ptr<lab::AudioNode>>(node);
    if (!n)
        return;

    auto s = n->setting(setting_name.c_str());
    if (s)
    {
        auto soundClip = lab::MakeBusFromFile(path.c_str(), false);
        s->setBus(soundClip.get());
        printf("SetBusSetting %s %s\n", setting_name.c_str(), path.c_str());
    }
}

void LabSoundProvider::pin_set_bus_from_file(entt::entity pin_id, const std::string& path)
{
    entt::registry& registry = Registry();
    if (pin_id != entt::null && path.length() > 0)
    {
        lab::noodle::Pin& pin = registry.get<lab::noodle::Pin>(pin_id);
        AudioPin& a_pin = registry.get<AudioPin>(pin_id);
        if (pin.kind == lab::noodle::Pin::Kind::Setting && a_pin.setting)
        {
            auto soundClip = lab::MakeBusFromFile(path.c_str(), false);
            a_pin.setting->setBus(soundClip.get());
            printf("SetBusSetting %d %s\n", pin_id, path.c_str());
        }
    }
}

void LabSoundProvider::connect_bus_out_to_bus_in(entt::entity output_node_id, entt::entity output_pin_id, entt::entity input_node_id)
{
    entt::registry& registry = Registry();
    if (!registry.valid(input_node_id) || !registry.valid(output_node_id))
        return;

    shared_ptr<lab::AudioNode> in = registry.get<shared_ptr<lab::AudioNode>>(input_node_id);
    shared_ptr<lab::AudioNode> out = registry.get<shared_ptr<lab::AudioNode>>(output_node_id);
    if (!in || !out)
        return;

    g_audio_context->connect(in, out, 0, 0);
    printf("ConnectBusOutToBusIn %d %d\n", input_node_id, output_node_id);
}

void LabSoundProvider::connect_bus_out_to_param_in(entt::entity output_node_id, entt::entity output_pin_id, entt::entity param_pin_id)
{
    entt::registry& registry = Registry();
    if (!registry.valid(param_pin_id))
        return;

    if (output_pin_id != entt::null && !registry.valid(output_pin_id))
        return;

    if (!registry.valid(output_node_id))
        return;

    AudioPin& param_pin = registry.get<AudioPin>(param_pin_id);
    lab::noodle::Pin& param_noodle_pin = registry.get<lab::noodle::Pin>(param_pin_id);
    if (!registry.valid(param_noodle_pin.node_id) || !param_pin.param)
        return;

    shared_ptr<lab::AudioNode> out = registry.get<shared_ptr<lab::AudioNode>>(output_node_id);
    if (!out)
        return;

    int output_index = 0;
    if (output_pin_id != entt::null)
    {
        AudioPin& output_pin = registry.get<AudioPin>(output_pin_id);
        output_index = output_pin.output_index;
    }

    g_audio_context->connectParam(param_pin.param, out, output_index);
    printf("ConnectBusOutToParamIn %d %d, index %d\n", param_pin_id, output_node_id, output_index);
}

void LabSoundProvider::disconnect(entt::entity connection_id)
{
    entt::registry& registry = Registry();
    if (!registry.valid(connection_id))
        return;

    lab::noodle::Connection& conn = registry.get<lab::noodle::Connection>(connection_id);
    entt::entity input_node_id = conn.node_to;
    entt::entity output_node_id = conn.node_from;
    entt::entity input_pin = conn.pin_to;
    entt::entity output_pin = conn.pin_from;

    if (registry.valid(input_node_id) && registry.valid(output_node_id) && registry.valid(input_pin) && registry.valid(output_pin))
    {
        shared_ptr<lab::AudioNode> input_node = registry.get<shared_ptr<lab::AudioNode>>(output_node_id);
        shared_ptr<lab::AudioNode> output_node = registry.get<shared_ptr<lab::AudioNode>>(output_node_id);
        if (input_node && output_node)
        {
            AudioPin& a_in_pin = registry.get<AudioPin>(input_pin);
            lab::noodle::Pin& in_pin = registry.get<lab::noodle::Pin>(input_pin);
            lab::noodle::Pin& out_pin = registry.get<lab::noodle::Pin>(output_pin);
            if ((in_pin.kind == lab::noodle::Pin::Kind::BusIn) && (out_pin.kind == lab::noodle::Pin::Kind::BusOut))
            {
                g_audio_context->disconnect(input_node, output_node, 0, 0);
                printf("DisconnectInFromOut (bus from bus) %d %d\n", input_node_id, output_node_id);
            }
            else if ((in_pin.kind == lab::noodle::Pin::Kind::Param) && (out_pin.kind == lab::noodle::Pin::Kind::BusOut))
            {
                g_audio_context->disconnectParam(a_in_pin.param, output_node, 0);
                printf("DisconnectInFromOut (param from bus) %d %d\n", input_node_id, output_node_id);
            }
        }
    }

    //// @TODO this plumbing belongs in lab_noodle as it is audio-agnostic
    registry.destroy(connection_id);
    /// @TODO end
    return;
}

entt::entity LabSoundProvider::create_runtime_context(entt::entity id)
{
    entt::registry& registry = Registry();
    const auto defaultAudioDeviceConfigurations = GetDefaultAudioDeviceConfiguration(true);

    if (!g_audio_context)
        g_audio_context = lab::MakeRealtimeAudioContext(defaultAudioDeviceConfigurations.second, defaultAudioDeviceConfigurations.first);

    registry.emplace<shared_ptr<lab::AudioNode>>(id, g_audio_context->device());

    lab::noodle::Node& node = registry.get<lab::noodle::Node>(id);
    CreateEntities(g_audio_context->device(), node, id);
    printf("CreateRuntimeContext %d\n", id);
    return id;
}

void LabSoundProvider::node_start_stop(entt::entity node_id, float when)
{
    entt::registry& registry = Registry();
    if (node_id == entt::null)
        return;

    shared_ptr<lab::AudioNode> in_node = registry.get<shared_ptr<lab::AudioNode>>(node_id);
    if (!in_node)
        return;

    lab::AudioScheduledSourceNode* n = dynamic_cast<lab::AudioScheduledSourceNode*>(in_node.get());
    if (n)
    {
        if (n->isPlayingOrScheduled())
            n->stop(when);
        else
            n->start(when);
    }

    printf("Start %d\n", node_id);
}

void LabSoundProvider::node_bang(entt::entity node_id)
{
    entt::registry& registry = Registry();
    if (node_id == entt::null)
        return;

    shared_ptr<lab::AudioNode> in_node = registry.get<shared_ptr<lab::AudioNode>>(node_id);
    if (!in_node)
        return;

    shared_ptr<lab::AudioParam> gate = in_node->param("gate");
    if (gate)
    {
        gate->setValueAtTime(1.f, static_cast<float>(g_audio_context->currentTime()) + 0.f);
        gate->setValueAtTime(0.f, static_cast<float>(g_audio_context->currentTime()) + 1.f);
    }

    printf("Bang %d\n", node_id);
}

entt::entity LabSoundProvider::node_output_named(entt::entity node_id, const std::string& output_name)
{
    entt::registry& registry = Registry();
    if (node_id == entt::null)
        return entt::null;

    shared_ptr<lab::AudioNode> node = registry.get<shared_ptr<lab::AudioNode>>(node_id);
    if (!node)
        return entt::null;

    auto reverse_it = g_node_reverse_lookups.find(node_id);
    if (reverse_it == g_node_reverse_lookups.end())
        return entt::null;

    auto& reverse = reverse_it->second;
    auto output_it = reverse.output_pin_map.find(output_name);
    if (output_it == reverse.output_pin_map.end())
        return entt::null;

    return output_it->second;
}

entt::entity LabSoundProvider::node_input_with_index(entt::entity node_id, int output)
{
    entt::registry& registry = Registry();
    if (node_id == entt::null)
        return entt::null;

    shared_ptr<lab::AudioNode> node = registry.get<shared_ptr<lab::AudioNode>>(node_id);
    if (!node)
        return entt::null;

    auto reverse_it = g_node_reverse_lookups.find(node_id);
    if (reverse_it == g_node_reverse_lookups.end())
        return entt::null;

    auto& reverse = reverse_it->second;
    auto input_it = reverse.input_pin_map.find("");
    if (input_it == reverse.input_pin_map.end())
        return entt::null;

    return input_it->second;
}

entt::entity LabSoundProvider::node_output_with_index(entt::entity node_id, int output)
{
    entt::registry& registry = Registry();
    if (node_id == entt::null)
        return entt::null;

    shared_ptr<lab::AudioNode> node = registry.get<shared_ptr<lab::AudioNode>>(node_id);
    if (!node)
        return entt::null;

    auto reverse_it = g_node_reverse_lookups.find(node_id);
    if (reverse_it == g_node_reverse_lookups.end())
        return entt::null;

    auto& reverse = reverse_it->second;
    auto output_it = reverse.output_pin_map.find("");
    if (output_it == reverse.output_pin_map.end())
        return entt::null;

    return output_it->second;
}

entt::entity LabSoundProvider::node_param_named(entt::entity node_id, const std::string& output_name)
{
    entt::registry& registry = Registry();
    if (node_id == entt::null)
        return entt::null;

    shared_ptr<lab::AudioNode> node = registry.get<shared_ptr<lab::AudioNode>>(node_id);
    if (!node)
        return entt::null;

    auto reverse_it = g_node_reverse_lookups.find(node_id);
    if (reverse_it == g_node_reverse_lookups.end())
        return entt::null;

    auto& reverse = reverse_it->second;
    auto output_it = reverse.param_pin_map.find(output_name);
    if (output_it == reverse.param_pin_map.end())
        return entt::null;

    return output_it->second;
}

entt::entity LabSoundProvider::node_create(const std::string& kind, entt::entity id)
{
    entt::registry& registry = Registry();
    if (kind == "OSC")
    {
        shared_ptr<OSCNode> n = std::make_shared<OSCNode>(*g_audio_context.get());
        registry.emplace<shared_ptr<OSCNode>>(id, n);
        registry.emplace<shared_ptr<lab::AudioNode>>(id, n);
        _osc_node = id;
        return id;
    }

    shared_ptr<lab::AudioNode> n = NodeFactory(kind);
    if (n)
    {
        lab::noodle::Node& node = registry.get<lab::noodle::Node>(id);
        node.play_controller = n->isScheduledNode();
        node.bang_controller = !!n->param("gate");
        registry.emplace<shared_ptr<lab::AudioNode>>(id, n);
        CreateEntities(n, node, id);
        printf("CreateNode [%s] %d\n", kind.c_str(), id);
    }
    else
    {
        printf("Could not CreateNode [%s]\n", kind.c_str());
    }

    return id;
}

void LabSoundProvider::node_delete(entt::entity node_id)
{
    entt::registry& registry = Registry();
    if (node_id != entt::null && registry.valid(node_id))
    {
        printf("DeleteNode %d\n", node_id);

        // force full disconnection
        if (registry.has<shared_ptr<lab::AudioNode>>(node_id))
        {
            shared_ptr<lab::AudioNode> in_node = registry.get<shared_ptr<lab::AudioNode>>(node_id);
            g_audio_context->disconnect(in_node);
        }

        /// @TODO this bit should be managed in lab_noodle
        // delete all the node's pins
        for (const auto entity : registry.view<lab::noodle::Pin>())
        {
            lab::noodle::Pin& pin = registry.get<lab::noodle::Pin>(entity);
            if (pin.node_id == entity)
                registry.destroy(entity);
        }

        //// @TODO this plumbing belongs in lab_noodle as it is audio-agnostic
        // remove connections
        for (const auto entity : registry.view<lab::noodle::Connection>())
        {
            lab::noodle::Connection& conn = registry.get<lab::noodle::Connection>(entity);
            if (conn.node_from == node_id || conn.node_to == node_id)
                registry.destroy(entity);
        }

        /// @TODO end plumbing

        auto reverse_it = g_node_reverse_lookups.find(node_id);
        if (reverse_it != g_node_reverse_lookups.end())
            g_node_reverse_lookups.erase(reverse_it);

        // note: node_id is to be deleted externally, as it was created externally
    }
}

entt::registry& LabSoundProvider::registry() const
{
    return Registry();
}

char const* const* LabSoundProvider::node_names() const
{
    static char const** names = nullptr;
    if (!names)
    {
        int count = 0;
        char const* const* src_names = lab::AudioNodeNames();
        for (; *src_names != nullptr; ++src_names, ++count)
        {
        }
        names = reinterpret_cast<char const**>(malloc(sizeof(char*) * (count + 2)));
        if (names)
        {
            static char* osc_name = "OSC";
            names[0] = osc_name;
            memcpy(&names[1], lab::AudioNodeNames(), sizeof(char*) * count);
            names[count + 1] = nullptr;
        }
    }
    return names;
}

void LabSoundProvider::pin_set_param_value(const std::string& node_name, const std::string& param_name, float v)
{
    entt::entity node = entity_for_node_named(node_name);
    if (node == entt::null || !registry().valid(node))
        return;

    auto n = registry().get<std::shared_ptr<lab::AudioNode>>(node);
    if (!n)
        return;

    auto p = n->param(param_name.c_str());
    if (p)
        p->setValue(v);
}

void LabSoundProvider::pin_set_setting_float_value(const std::string& node_name, const std::string& setting_name, float v)
{
    entt::entity node = entity_for_node_named(node_name);
    if (node == entt::null || !registry().valid(node))
        return;

    auto n = registry().get<std::shared_ptr<lab::AudioNode>>(node);
    if (!n)
        return;

    auto s = n->setting(setting_name.c_str());
    if (s)
        s->setFloat(v);
}

void LabSoundProvider::pin_set_float_value(entt::entity pin, float v)
{
    entt::registry& registry = Registry();
    if (pin != entt::null && registry.valid(pin))
    {
        AudioPin& a_pin = registry.get<AudioPin>(pin);
        if (a_pin.param)
        {
            a_pin.param->setValue(v);
            printf("SetParam(%f) %d\n", v, pin);
        }
        else if (a_pin.setting)
        {
            a_pin.setting->setFloat(v);
            printf("SetFloatSetting(%f) %d\n", v, pin);
        }
    }
}

float LabSoundProvider::pin_float_value(entt::entity pin)
{
    AudioPin& a_pin = registry().get<AudioPin>(pin);
    if (a_pin.param)
        return a_pin.param->value();
    else if (a_pin.setting)
        return a_pin.setting->valueFloat();
    else
        return 0.f;
}

void LabSoundProvider::pin_set_setting_int_value(const std::string& node_name, const std::string& setting_name, int v)
{
    entt::entity node = entity_for_node_named(node_name);
    if (node == entt::null || !registry().valid(node))
        return;

    auto n = registry().get<std::shared_ptr<lab::AudioNode>>(node);
    if (!n)
        return;

    auto s = n->setting(setting_name.c_str());
    if (s)
        s->setUint32(v);
}

void LabSoundProvider::pin_set_int_value(entt::entity pin, int v)
{
    entt::registry& registry = Registry();
    if (pin != entt::null && registry.valid(pin))
    {
        AudioPin& a_pin = registry.get<AudioPin>(pin);
        if (a_pin.param)
        {
            a_pin.param->setValue(static_cast<float>(v));
            printf("SetParam(%d) %d\n", v, pin);
        }
        else if (a_pin.setting)
        {
            a_pin.setting->setUint32(v);
            printf("SetIntSetting(%d) %d\n", v, pin);
        }
    }
}

int LabSoundProvider::pin_int_value(entt::entity pin)
{
    AudioPin& a_pin = registry().get<AudioPin>(pin);
    if (a_pin.param)
        return static_cast<int>(a_pin.param->value());
    else if (a_pin.setting)
        return a_pin.setting->valueUint32();
    else
        return 0;
}


void LabSoundProvider::pin_set_enumeration_value(entt::entity pin, const std::string& value)
{
    entt::registry& registry = Registry();
    if (pin != entt::null && registry.valid(pin))
    {
        AudioPin& a_pin = registry.get<AudioPin>(pin);
        if (a_pin.setting)
        {
            int e = a_pin.setting->enumFromName(value.c_str());
            if (e >= 0)
            {
                a_pin.setting->setUint32(e);
                printf("SetEnumSetting(%d) %d\n", e, pin);
            }
        }
    }
}

void LabSoundProvider::pin_set_setting_enumeration_value(const std::string& node_name, const std::string& setting_name, const std::string& value)
{
    entt::entity node = entity_for_node_named(node_name);
    if (node == entt::null || !registry().valid(node))
        return;

    auto n = registry().get<std::shared_ptr<lab::AudioNode>>(node);
    if (!n)
        return;

    auto s = n->setting(setting_name.c_str());
    if (s)
    {
        int e = s->enumFromName(value.c_str());
        if (e >= 0)
        {
            s->setUint32(e);
            printf("SetEnumSetting(%s) = %s\n", setting_name.c_str(), value.c_str());
        }
    }
}

void LabSoundProvider::pin_set_setting_bool_value(const std::string& node_name, const std::string& setting_name, bool v)
{
    entt::entity node = entity_for_node_named(node_name);
    if (node == entt::null || !registry().valid(node))
        return;

    auto n = registry().get<std::shared_ptr<lab::AudioNode>>(node);
    if (!n)
        return;

    auto s = n->setting(setting_name.c_str());
    if (s)
        s->setBool(v);
}

void  LabSoundProvider::pin_set_bool_value(entt::entity pin, bool v)
{
    entt::registry& registry = Registry();
    if (pin != entt::null && registry.valid(pin))
    {
        AudioPin& a_pin = registry.get<AudioPin>(pin);
        if (a_pin.param)
        {
            a_pin.param->setValue(v? 1.f : 0.f);
            printf("SetParam(%d) %d\n", v, pin);
        }
        else if (a_pin.setting)
        {
            a_pin.setting->setBool(v);
            printf("SetBoolSetting(%s) %d\n", v ? "true": "false", pin);
        }
    }
}

bool LabSoundProvider::pin_bool_value(entt::entity pin)
{
    AudioPin& a_pin = registry().get<AudioPin>(pin);
    if (a_pin.param)
        return a_pin.param->value() != 0.f;
    else if (a_pin.setting)
        return a_pin.setting->valueBool();
    else
        return 0;
}

void LabSoundProvider::pin_create_output(const std::string& node_name, const std::string& output_name, int channels)
{
    entt::entity node_e = entity_for_node_named(node_name);
    if (node_e == entt::null || !registry().valid(node_e))
        return;

    auto n = registry().get<std::shared_ptr<lab::AudioNode>>(node_e);
    if (!n)
        return;

    if (!n->output(output_name.c_str()))
    {
        auto reverse_it = g_node_reverse_lookups.find(node_e);
        if (reverse_it == g_node_reverse_lookups.end())
        {
            g_node_reverse_lookups[node_e] = NodeReverseLookup{};
            reverse_it = g_node_reverse_lookups.find(node_e);
        }
        auto& reverse = reverse_it->second;

        entt::entity pin_id = registry().create();

        lab::noodle::Node& node = registry().get<lab::noodle::Node>(node_e);

        node.pins.push_back(pin_id);
        reverse.output_pin_map[output_name] = pin_id;
        registry().emplace<lab::noodle::Name>(pin_id, output_name);
        registry().emplace<lab::noodle::Pin>(pin_id, lab::noodle::Pin{
            lab::noodle::Pin::Kind::BusOut,
            lab::noodle::Pin::DataType::Bus,
            "",
            pin_id, node_e,
            });
        registry().emplace<AudioPin>(pin_id, n->numberOfOutputs() - 1);

        lab::ContextGraphLock glock(g_audio_context.get(), "AudioHardwareDeviceNode");
        n->addOutput(glock, std::unique_ptr<lab::AudioNodeOutput>(new lab::AudioNodeOutput(n.get(), output_name.c_str(), channels)));
    }
}

float LabSoundProvider::node_get_timing(entt::entity node)
{
    if (!registry().valid(node))
        return 0;

    auto n = registry().get<std::shared_ptr<lab::AudioNode>>(node);
    if (!n)
        return 0;
    return n->graphTime.microseconds.count() * 1.e-6f;
}

float LabSoundProvider::node_get_self_timing(entt::entity node)
{
    if (!registry().valid(node))
        return 0;

    if (!registry().has<std::shared_ptr<lab::AudioNode>>(node))
        return 0;

    auto n = registry().get<std::shared_ptr<lab::AudioNode>>(node);
    if (!n)
        return 0;

    return (n->totalTime.microseconds.count() - n->graphTime.microseconds.count()) * 1.e-6f;
}

void LabSoundProvider::add_osc_addr(char const* const addr, int addr_id, int channels, float* data)
{
    if (_osc_node == entt::null)
        return;

    entt::registry& registry = Registry();
    shared_ptr<OSCNode> n = registry.get<shared_ptr<OSCNode>>(_osc_node);
    if (!n->addAddress(addr, addr_id, channels, data))
        return;

    auto it = n->key_to_addrData.find(addr_id);
    if (it != n->key_to_addrData.end())
    {
        lab::noodle::Node& node = registry.get<lab::noodle::Node>(_osc_node);
        entt::entity pin_id = registry.create();
        node.pins.push_back(pin_id);
        registry.emplace<lab::noodle::Name>(pin_id, addr);
        registry.emplace<lab::noodle::Pin>(pin_id, lab::noodle::Pin{
            lab::noodle::Pin::Kind::BusOut,
            lab::noodle::Pin::DataType::Bus,
            "",
            pin_id, _osc_node,
            });
        registry.emplace<AudioPin>(pin_id, AudioPin{ it->second.output_index });
    }

}
