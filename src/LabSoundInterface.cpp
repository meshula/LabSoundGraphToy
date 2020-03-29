
#include "LabSoundInterface.h"

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
        if (n == "Analyser")
        {
            shared_ptr<lab::AudioNode> node = std::make_shared<lab::AnalyserNode>(ac);
            g_audio_context->addAutomaticPullNode(node);
            return node;
        }
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



    void CreateEntities(shared_ptr<lab::AudioNode> audio_node, entt::entity audio_node_id)
    {
        if (!audio_node)
            return;

        entt::registry& registry = Registry();

        vector<entt::entity> pins;

        ContextRenderLock r(g_audio_context.get(), "LabSoundGraphToy_init");
        if (nullptr != dynamic_cast<RealtimeAnalyser*>(audio_node.get()))
        {
            g_audio_context->addAutomaticPullNode(audio_node);
        }

        //---------- settings

        vector<string> names = audio_node->settingNames();
        vector<string> shortNames = audio_node->settingShortNames();
        auto settings = audio_node->settings();
        int c = (int)settings.size();
        for (int i = 0; i < c; ++i)
        {
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

            entt::entity pin_id = registry.create();
            pins.push_back(pin_id);
            registry.assign<AudioPin>(pin_id, AudioPin{
                AudioPin::Kind::Setting,
                names[i], shortNames[i],
                pin_id, audio_node_id,
                settings[i], shared_ptr<lab::AudioParam>(),
                buff
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
                AudioPin::Kind::Param,
                names[i], shortNames[i],
                pin_id, audio_node_id,
                shared_ptr<AudioSetting>(),
                params[i],
                buff
            });
        }

        //---------- inputs

        c = (int)audio_node->numberOfInputs();
        for (int i = 0; i < c; ++i)
        {
            entt::entity pin_id = registry.create();
            pins.push_back(pin_id);
            registry.assign<AudioPin>(pin_id, AudioPin{
                AudioPin::Kind::BusIn,
                "", "",
                pin_id, audio_node_id,
                shared_ptr<AudioSetting>(),
            });
        }

        //---------- outputs

        c = (int)audio_node->numberOfOutputs();
        for (int i = 0; i < c; ++i)
        {
            entt::entity pin_id = registry.create();
            pins.push_back(pin_id);
            registry.assign<AudioPin>(pin_id, AudioPin{
                AudioPin::Kind::BusOut,
                "", "",
                pin_id, audio_node_id,
                shared_ptr<AudioSetting>(),
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
            registry.assign<lab::Sound::AudioNode>(id, lab::Sound::AudioNode{ "Device" });
            CreateEntities(g_audio_context->device(), id);
            printf("CreateRuntimeContext %d\n", id);
            return id;
        }
        case WorkType::CreateNode:
        {
            shared_ptr<lab::AudioNode> node = NodeFactory(name);
            entt::entity id = registry.create();
            registry.assign<shared_ptr<lab::AudioNode>>(id, node);
            registry.assign<lab::Sound::AudioNode>(id, lab::Sound::AudioNode{ name });
            CreateEntities(node, id);
            printf("CreateNode %s %d\n", name.c_str(), id);
            return id;
        }
        case WorkType::SetParam:
        {
            if (pin_id != entt::null && registry.valid(pin_id))
            {
                AudioPin& pin = registry.get<AudioPin>(pin_id);
                if (pin.kind == AudioPin::Kind::Param && pin.param)
                {
                    pin.param->setValue(float_value);
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
                AudioPin& pin = registry.get<AudioPin>(pin_id);
                if (pin.kind == AudioPin::Kind::Setting && pin.setting)
                {
                    pin.setting->setFloat(float_value);
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
                AudioPin& pin = registry.get<AudioPin>(pin_id);
                if (pin.kind == AudioPin::Kind::Setting && pin.setting)
                {
                    pin.setting->setUint32(int_value);
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
                AudioPin& pin = registry.get<AudioPin>(pin_id);
                if (pin.kind == AudioPin::Kind::Setting && pin.setting)
                {
                    pin.setting->setBool(bool_value);
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
                AudioPin& pin = registry.get<AudioPin>(pin_id);
                if (pin.kind == AudioPin::Kind::Setting && pin.setting)
                {
                    auto soundClip = lab::MakeBusFromFile(name.c_str(), false);
                    pin.setting->setBus(soundClip.get());
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

            entt::entity pin_from = entt::null;
            entt::entity pin_to = entt::null;
            auto view = registry.view<AudioPin>();
            for (auto entity : view) {
                AudioPin& pin = view.get<AudioPin>(entity);
                if (pin.node_id == output_node_id && pin.kind == AudioPin::Kind::BusOut)
                {
                    pin_from = pin.pin_id;
                }
                if (pin.node_id == input_node_id && pin.kind == AudioPin::Kind::BusIn)
                {
                    pin_to = pin.pin_id;
                }
            }

            entt::entity connection_id = registry.create();
            registry.assign<Connection>(connection_id, Connection{
                connection_id,
                pin_from, output_node_id,
                pin_to, input_node_id
                });

            return connection_id;
        }
        case WorkType::ConnectBusOutToParamIn:
        {
            if (!registry.valid(pin_id) || !registry.valid(output_node_id))
                return entt::null;

            AudioPin& pin = registry.get<AudioPin>(pin_id);
            if (!registry.valid(pin.node_id) || !pin.param)
                return entt::null;

            shared_ptr<lab::AudioNode> out = registry.get<shared_ptr<lab::AudioNode>>(output_node_id);
            if (!out)
                return entt::null;

            g_audio_context->connectParam(pin.param, out, 0);
            printf("ConnectBusOutToParamIn %d %d\n", pin_id, output_node_id);

            entt::entity pin_from = entt::null;
            auto view = registry.view<AudioPin>();
            for (auto entity : view) {
                AudioPin& pin = view.get<AudioPin>(entity);
                if (pin.node_id == output_node_id && pin.kind == AudioPin::Kind::BusOut)
                {
                    pin_from = pin.pin_id;
                }
            }

            entt::entity connection_id = registry.create();
            registry.assign<Connection>(connection_id, Connection{
                connection_id,
                pin_from, output_node_id,
                pin_id, pin.node_id
                });

            return connection_id;
        }
        case WorkType::DisconnectInFromOut:
        {
            if (!registry.valid(connection_id))
                return entt::null;

            Connection& conn = registry.get<Connection>(connection_id);
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
                    AudioPin& in_pin = registry.get<AudioPin>(input_pin);
                    AudioPin& out_pin = registry.get<AudioPin>(output_pin);
                    if ((in_pin.kind == AudioPin::Kind::BusIn) && (out_pin.kind == AudioPin::Kind::BusOut))
                    {
                        g_audio_context->disconnect(input_node, output_node, 0, 0);
                        printf("DisconnectInFromOut (bus from bus) %d %d\n", input_node_id, output_node_id);
                    }
                    else if ((in_pin.kind == AudioPin::Kind::Param) && (out_pin.kind == AudioPin::Kind::BusOut))
                    {
                        g_audio_context->disconnectParam(in_pin.param, output_node, 0);
                        printf("DisconnectInFromOut (param from bus) %d %d\n", input_node_id, output_node_id);
                    }
                }
            }

            registry.destroy(connection_id);
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

                // delete all the node's pins
                for (const auto entity : registry.view<AudioPin>())
                {
                    AudioPin& pin = registry.get<AudioPin>(entity);
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

                // delete all the node's pins
                for (const auto entity : registry.view<AudioPin>())
                {
                    AudioPin& pin = registry.get<AudioPin>(entity);
                    if (pin.node_id == entity)
                        registry.destroy(entity);
                }
            }

            // remove connections
            for (const auto entity : registry.view<Connection>())
            {
                Connection& conn = registry.get<Connection>(entity);
                if (entity != entt::null && (conn.node_from == output_node_id || conn.node_to == output_node_id ||
                                              conn.node_from == input_node_id  || conn.node_to == input_node_id))
                    registry.destroy(entity);
            }

            if (input_node_id != entt::null)
                registry.destroy(input_node_id);
            if (output_node_id != entt::null)
                registry.destroy(output_node_id);

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


}} // lab::Sound

