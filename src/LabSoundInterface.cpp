
#include "LabSoundInterface.h"
#include "lab_imgui_ext.hpp"

#include <LabSound/LabSound.h>

#include <stdio.h>

namespace lab { namespace Sound {

    using std::shared_ptr;
    using std::string;
    using std::vector;


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


    void CreateEntities(shared_ptr<lab::AudioNode> audio_node, entt::entity audio_node_id)
    {
        if (!audio_node)
            return;

        entt::registry& registry = Registry();

        vector<entt::entity> pins;

        ContextRenderLock r(g_audio_context.get(), "LabSoundGraphToy_init");
        if (nullptr != dynamic_cast<lab::AnalyserNode*>(audio_node.get()))
        {
            g_audio_context->addAutomaticPullNode(audio_node);
            registry.assign<noodle::NodeRender>(audio_node_id, noodle::NodeRender{
                [](entt::entity id, noodle::vec2 ul_ws, noodle::vec2 lr_ws, float scale, void* drawList) {
                    DrawSpectrum(id, {ul_ws.x, ul_ws.y}, {lr_ws.x, lr_ws.y}, scale, reinterpret_cast<ImDrawList*>(drawList));
                } });
        }

        //---------- inputs

        int c = (int)audio_node->numberOfInputs();
        for (int i = 0; i < c; ++i)
        {
            entt::entity pin_id = registry.create();
            pins.push_back(pin_id);
            registry.assign<noodle::Name>(pin_id, "");
            registry.assign<noodle::Pin>(pin_id, noodle::Pin{
                noodle::Pin::Kind::BusIn,
                noodle::Pin::DataType::Bus,
                "",
                pin_id, audio_node_id,
                });
            registry.assign<AudioPin>(pin_id, AudioPin{
                });
        }

        //---------- outputs

        c = (int)audio_node->numberOfOutputs();
        for (int i = 0; i < c; ++i)
        {
            entt::entity pin_id = registry.create();
            pins.push_back(pin_id);
            registry.assign<noodle::Name>(pin_id, "");
            registry.assign<noodle::Pin>(pin_id, noodle::Pin{
                noodle::Pin::Kind::BusOut,
                noodle::Pin::DataType::Bus,
                "",
                pin_id, audio_node_id,
                });
            registry.assign<AudioPin>(pin_id, AudioPin{
                });
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

            entt::entity pin_id = registry.create();
            pins.push_back(pin_id);
            registry.assign<AudioPin>(pin_id, AudioPin{
                settings[i]
                });
            registry.assign<noodle::Name>(pin_id, names[i]);
            registry.assign<noodle::Pin>(pin_id, noodle::Pin{
                noodle::Pin::Kind::Setting,
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
            pins.push_back(pin_id);
            registry.assign<AudioPin>(pin_id, AudioPin{
                shared_ptr<AudioSetting>(),
                params[i],
                });
            registry.assign<noodle::Name>(pin_id, names[i]);
            registry.assign<noodle::Pin>(pin_id, noodle::Pin{
                noodle::Pin::Kind::Param,
                noodle::Pin::DataType::Float,
                shortNames[i],
                pin_id, audio_node_id,
                buff
                });
        }

        registry.assign<vector<entt::entity>>(audio_node_id, pins);
    }

    entt::entity Work::eval()
    {
        entt::registry& registry = Registry();

        switch (type)
        {
        case WorkType::CreateRuntimeContext:
        {
            const auto defaultAudioDeviceConfigurations = GetDefaultAudioDeviceConfiguration(true);
            g_audio_context = lab::MakeRealtimeAudioContext(defaultAudioDeviceConfigurations.second, defaultAudioDeviceConfigurations.first);
            entt::entity id = registry.create();
            registry.assign<shared_ptr<lab::AudioNode>>(id, g_audio_context->device());
            CreateEntities(g_audio_context->device(), id);
            printf("CreateRuntimeContext %d\n", id);
            return id;
        }
        case WorkType::CreateNode:
        {
            shared_ptr<lab::AudioNode> node = NodeFactory(name);
            entt::entity id = registry.create();
            registry.assign<shared_ptr<lab::AudioNode>>(id, node);
            CreateEntities(node, id);
            printf("CreateNode %s %d\n", name.c_str(), id);
            return id;
        }
        case WorkType::SetParam:
        {
            if (pin_id != entt::null && registry.valid(pin_id))
            {
                noodle::Pin& pin = registry.get<noodle::Pin>(pin_id);
                AudioPin& a_pin = registry.get<AudioPin>(pin_id);
                if (pin.kind == noodle::Pin::Kind::Param && a_pin.param)
                {
                    a_pin.param->setValue(float_value);
                    printf("SetParam(%f) %d\n", float_value, pin_id);
                    return pin_id;
                }
            }
            return entt::null;
        }
        case WorkType::SetFloatSetting:
        {
            if (pin_id != entt::null)
            {
                noodle::Pin& pin = registry.get<noodle::Pin>(pin_id);
                AudioPin& a_pin = registry.get<AudioPin>(pin_id);
                if (pin.kind == noodle::Pin::Kind::Setting && a_pin.setting)
                {
                    a_pin.setting->setFloat(float_value);
                    printf("SetFloatSetting(%f) %d\n", float_value, pin_id);
                    return pin_id;
                }
            }
            return entt::null;
        }
        case WorkType::SetIntSetting:
        {
            if (pin_id != entt::null)
            {
                noodle::Pin& pin = registry.get<noodle::Pin>(pin_id);
                AudioPin& a_pin = registry.get<AudioPin>(pin_id);
                if (pin.kind == noodle::Pin::Kind::Setting && a_pin.setting)
                {
                    a_pin.setting->setUint32(int_value);
                    printf("SetIntSetting(%d) %d\n", int_value, pin_id);
                    return pin_id;
                }
            }
            return entt::null;
        }
        case WorkType::SetBoolSetting:
        {
            if (pin_id != entt::null)
            {
                noodle::Pin& pin = registry.get<noodle::Pin>(pin_id);
                AudioPin& a_pin = registry.get<AudioPin>(pin_id);
                if (pin.kind == noodle::Pin::Kind::Setting && a_pin.setting)
                {
                    a_pin.setting->setBool(bool_value);
                    printf("SetBoolSetting(%s) %d\n", bool_value ? "true" : "false", pin_id);
                    return pin_id;
                }
            }
            return entt::null;
        }
        case WorkType::SetBusSetting:
        {
            if (pin_id != entt::null && name.length() > 0)
            {
                noodle::Pin& pin = registry.get<noodle::Pin>(pin_id);
                AudioPin& a_pin = registry.get<AudioPin>(pin_id);
                if (pin.kind == noodle::Pin::Kind::Setting && a_pin.setting)
                {
                    auto soundClip = lab::MakeBusFromFile(name.c_str(), false);
                    a_pin.setting->setBus(soundClip.get());
                    printf("SetBusSetting %d %s\n", pin_id, name.c_str());
                    return pin_id;
                }
            }
            return entt::null;
        }
        case WorkType::ConnectBusOutToBusIn:
        {
            if (!registry.valid(input_node_id) || !registry.valid(output_node_id))
                return entt::null;

            shared_ptr<lab::AudioNode> in = registry.get<shared_ptr<lab::AudioNode>>(input_node_id);
            shared_ptr<lab::AudioNode> out = registry.get<shared_ptr<lab::AudioNode>>(output_node_id);
            if (!in || !out)
                return entt::null;

            g_audio_context->connect(in, out, 0, 0);
            printf("ConnectBusOutToBusIn %d %d\n", input_node_id, output_node_id);

            //// @TODO this plumbing belongs in lab_noodle as it is audio-agnostic

            entt::entity pin_from = entt::null;
            entt::entity pin_to = entt::null;
            auto view = registry.view<noodle::Pin>();
            for (auto entity : view) {
                noodle::Pin& pin = view.get<noodle::Pin>(entity);
                if (pin.node_id == output_node_id && pin.kind == noodle::Pin::Kind::BusOut)
                {
                    pin_from = pin.pin_id;
                }
                if (pin.node_id == input_node_id && pin.kind == noodle::Pin::Kind::BusIn)
                {
                    pin_to = pin.pin_id;
                }
            }

            entt::entity connection_id = registry.create();
            registry.assign<noodle::Connection>(connection_id, noodle::Connection{
                connection_id,
                pin_from, output_node_id,
                pin_to, input_node_id
                });

            /// @end plumbing

            return connection_id;
        }
        case WorkType::ConnectBusOutToParamIn:
        {
            if (!registry.valid(pin_id) || !registry.valid(output_node_id))
                return entt::null;

            AudioPin& a_pin = registry.get<AudioPin>(pin_id);
            noodle::Pin& pin = registry.get<noodle::Pin>(pin_id);
            if (!registry.valid(pin.node_id) || !a_pin.param)
                return entt::null;

            shared_ptr<lab::AudioNode> out = registry.get<shared_ptr<lab::AudioNode>>(output_node_id);
            if (!out)
                return entt::null;

            g_audio_context->connectParam(a_pin.param, out, 0);
            printf("ConnectBusOutToParamIn %d %d\n", pin_id, output_node_id);

            //// @TODO this plumbing belongs in lab_noodle as it is audio-agnostic

            entt::entity pin_from = entt::null;
            auto view = registry.view<noodle::Pin>();
            for (auto entity : view) {
                noodle::Pin& pin = view.get<noodle::Pin>(entity);
                if (pin.node_id == output_node_id && pin.kind == noodle::Pin::Kind::BusOut)
                {
                    pin_from = pin.pin_id;
                }
            }

            entt::entity connection_id = registry.create();
            registry.assign<noodle::Connection>(connection_id, noodle::Connection{
                connection_id,
                pin_from, output_node_id,
                pin_id, pin.node_id
                });

            // end plumbing

            return connection_id;
        }
        case WorkType::DisconnectInFromOut:
        {
            if (!registry.valid(connection_id))
                return entt::null;

            noodle::Connection& conn = registry.get<noodle::Connection>(connection_id);
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
                    noodle::Pin& in_pin = registry.get<noodle::Pin>(input_pin);
                    noodle::Pin& out_pin = registry.get<noodle::Pin>(output_pin);
                    if ((in_pin.kind == noodle::Pin::Kind::BusIn) && (out_pin.kind == noodle::Pin::Kind::BusOut))
                    {
                        g_audio_context->disconnect(input_node, output_node, 0, 0);
                        printf("DisconnectInFromOut (bus from bus) %d %d\n", input_node_id, output_node_id);
                    }
                    else if ((in_pin.kind == noodle::Pin::Kind::Param) && (out_pin.kind == noodle::Pin::Kind::BusOut))
                    {
                        g_audio_context->disconnectParam(a_in_pin.param, output_node, 0);
                        printf("DisconnectInFromOut (param from bus) %d %d\n", input_node_id, output_node_id);
                    }
                }
            }

            //// @TODO this plumbing belongs in lab_noodle as it is audio-agnostic
            registry.destroy(connection_id);
            /// @TODO end
            return entt::null;
        }
        case WorkType::DeleteNode:
        {
            shared_ptr<lab::AudioNode> in_node;
            shared_ptr<lab::AudioNode> out_node;

            if (input_node_id != entt::null && registry.valid(input_node_id))
            {
                printf("DeleteNode %d\n", input_node_id);

                // force full disconnection
                in_node = registry.get<shared_ptr<lab::AudioNode>>(input_node_id);
                g_audio_context->disconnect(in_node);

                /// @TODO this bit should be managed in lab_noodle
                // delete all the node's pins
                for (const auto entity : registry.view<noodle::Pin>())
                {
                    noodle::Pin& pin = registry.get<noodle::Pin>(entity);
                    if (pin.node_id == entity)
                        registry.destroy(entity);
                }
            }

            if (output_node_id != entt::null && registry.valid(output_node_id))
            {
                printf("DeleteNode %d\n", output_node_id);

                // force full disconnection
                in_node = registry.get<shared_ptr<lab::AudioNode>>(output_node_id);
                g_audio_context->disconnect(in_node);

                /// @TODO this bit should be managed in lab_noodle
                // delete all the node's pins
                for (const auto entity : registry.view<noodle::Pin>())
                {
                    noodle::Pin& pin = registry.get<noodle::Pin>(entity);
                    if (pin.node_id == entity)
                        registry.destroy(entity);
                }
            }

            //// @TODO this plumbing belongs in lab_noodle as it is audio-agnostic
            // remove connections
            for (const auto entity : registry.view<noodle::Connection>())
            {
                noodle::Connection& conn = registry.get<noodle::Connection>(entity);
                if (entity != entt::null && (conn.node_from == output_node_id || conn.node_to == output_node_id ||
                                              conn.node_from == input_node_id  || conn.node_to == input_node_id))
                    registry.destroy(entity);
            }

            if (input_node_id != entt::null)
                registry.destroy(input_node_id);
            if (output_node_id != entt::null)
                registry.destroy(output_node_id);

            /// @TODO end plumbing

            return entt::null;
        }
        case WorkType::Start:
        {
            if (input_node_id == entt::null)
                return entt::null;

            shared_ptr<lab::AudioNode> in_node = registry.get<shared_ptr<lab::AudioNode>>(input_node_id);
            if (!in_node)
                return entt::null;

            lab::AudioScheduledSourceNode* n = dynamic_cast<lab::AudioScheduledSourceNode*>(in_node.get());
            if (n)
            {
                if (n->isPlayingOrScheduled())
                    n->stop(0);
                else
                    n->start(0);
            }

            printf("Start %d\n", input_node_id);
            return input_node_id;
        }
        case WorkType::Bang:
        {
            if (input_node_id == entt::null)
                return entt::null;

            shared_ptr<lab::AudioNode> in_node = registry.get<shared_ptr<lab::AudioNode>>(input_node_id);
            if (!in_node)
                return entt::null;

            lab::AudioScheduledSourceNode* n = dynamic_cast<lab::AudioScheduledSourceNode*>(in_node.get());
            n->start(0);
            printf("Bang %d\n", input_node_id);
            return input_node_id;
        }
        default:
            return entt::null;
        }
    }

    entt::entity Provider::Create(char const* const name)
    {
        Work work;
        work.name.assign(name);
        work.type = WorkType::CreateNode;
        return work.eval();
    }

    vector<entt::entity>& Provider::pins(entt::entity audio_pin_id) const
    {
        return Registry().get<vector<entt::entity>>(audio_pin_id);
    }

    entt::registry& Provider::registry() const
    {
        return Registry();
    }

    char const* const* Provider::node_names() const
    {
        return lab::AudioNodeNames();
    }

    float Provider::pin_float_value(entt::entity pin)
    {
        lab::Sound::AudioPin& a_pin = registry().get<lab::Sound::AudioPin>(pin);
        if (a_pin.param)
            return a_pin.param->value();
        else if (a_pin.setting)
            return a_pin.setting->valueFloat();
        else
            return 0.f;
    }

    int Provider::pin_int_value(entt::entity pin)
    {
        lab::Sound::AudioPin& a_pin = registry().get<lab::Sound::AudioPin>(pin);
        if (a_pin.param)
            return static_cast<int>(a_pin.param->value());
        else if (a_pin.setting)
            return a_pin.setting->valueUint32();
        else
            return 0;
    }

    bool Provider::pin_bool_value(entt::entity pin)
    {
        lab::Sound::AudioPin& a_pin = registry().get<lab::Sound::AudioPin>(pin);
        if (a_pin.param)
            return a_pin.param->value() != 0.f;
        else if (a_pin.setting)
            return a_pin.setting->valueBool();
        else
            return 0;
    }

}} // lab::Sound

