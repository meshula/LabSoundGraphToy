
#include "LabSoundInterface.h"
#include "lab_imgui_ext.hpp"

#include <LabSound/LabSound.h>
#include "OSCNode.hpp"

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
    std::map<std::string, ln_Pin> input_pin_map;
    std::map<std::string, ln_Pin> output_pin_map;
    std::map<std::string, ln_Pin> param_pin_map;
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
inline std::pair<lab::AudioStreamConfig, lab::AudioStreamConfig> GetDefaultAudioDeviceConfiguration(const bool with_input = false)
{
    if (with_input)
        return
    {
        lab::GetDefaultInputAudioDeviceConfiguration(),
        lab::GetDefaultOutputAudioDeviceConfiguration()
    };

    return
    {
        lab::AudioStreamConfig(),
        lab::GetDefaultOutputAudioDeviceConfiguration()
    };
}


shared_ptr<lab::AudioNode> NodeFactory(const string& n)
{
    lab::AudioContext& ac = *g_audio_context.get();
    lab::AudioNode* node = lab::NodeRegistry::Instance().Create(n, ac);
    return std::shared_ptr<lab::AudioNode>(node);
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


void CreateEntities(shared_ptr<lab::AudioNode> audio_node, lab::noodle::NoodleNode& node, ln_Node audio_node_id)
{
    if (!audio_node)
        return;

    entt::registry& registry = Registry();

    auto reverse_it = g_node_reverse_lookups.find(audio_node_id.id);
    if (reverse_it == g_node_reverse_lookups.end())
    {
        g_node_reverse_lookups[audio_node_id.id] = NodeReverseLookup{};
        reverse_it = g_node_reverse_lookups.find(audio_node_id.id);
    }
    auto& reverse = reverse_it->second;

    lab::ContextRenderLock r(g_audio_context.get(), "LabSoundGraphToy_init");
    if (nullptr != dynamic_cast<lab::AnalyserNode*>(audio_node.get()))
    {
        g_audio_context->addAutomaticPullNode(audio_node);
        registry.emplace<lab::noodle::NodeRender>(audio_node_id.id, 
            lab::noodle::NodeRender{
                [](ln_Node id, lab::noodle::vec2 ul_ws, lab::noodle::vec2 lr_ws, float scale, void* drawList) {
                    DrawSpectrum(id.id, {ul_ws.x, ul_ws.y}, {lr_ws.x, lr_ws.y}, scale, reinterpret_cast<ImDrawList*>(drawList));
                } 
            });
    }

    //---------- inputs

    int c = (int)audio_node->numberOfInputs();
    for (int i = 0; i < c; ++i)
    {
        ln_Pin pin_id = { registry.create(), true };
        node.pins.push_back(pin_id);
        std::string name = ""; // audio_node->input(i)->name(); @TODO an IDL for all the things
        reverse.input_pin_map[name] = pin_id;
        registry.emplace<lab::noodle::Name>(pin_id.id, name);
        registry.emplace<lab::noodle::Pin>(pin_id.id, lab::noodle::Pin{
            lab::noodle::Pin::Kind::BusIn,
            lab::noodle::Pin::DataType::Bus,
            "",
            pin_id, audio_node_id,
            });
        registry.emplace<AudioPin>(pin_id.id, AudioPin{ 0 });
    }

    //---------- outputs

    c = (int)audio_node->numberOfOutputs();
    for (int i = 0; i < c; ++i)
    {
        ln_Pin pin_id = { registry.create(), true };
        node.pins.push_back(pin_id);
        std::string name = audio_node->output(i)->name();
        reverse.output_pin_map[name] = ln_Pin{ pin_id };
        registry.emplace<lab::noodle::Name>(pin_id.id, name);
        registry.emplace<lab::noodle::Pin>(pin_id.id, lab::noodle::Pin{
            lab::noodle::Pin::Kind::BusOut,
            lab::noodle::Pin::DataType::Bus,
            "",
            pin_id, audio_node_id,
            });
        registry.emplace<AudioPin>(pin_id.id, AudioPin{ i });
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

        ln_Pin pin_id{ registry.create(), true };
        node.pins.push_back(pin_id);
        registry.emplace<AudioPin>(pin_id.id, AudioPin{ 0, settings[i] });
        registry.emplace<lab::noodle::Name>(pin_id.id, names[i]);
        registry.emplace<lab::noodle::Pin>(pin_id.id, lab::noodle::Pin {
            lab::noodle::Pin::Kind::Setting,
            dataType,
            shortNames[i],
            pin_id, audio_node_id,
            std::string{ buff },
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
        ln_Pin pin_id { registry.create(), true };
        reverse.param_pin_map[names[i]] = pin_id;
        node.pins.push_back(pin_id);
        registry.emplace<AudioPin>(pin_id.id, AudioPin{ 0,
            shared_ptr<lab::AudioSetting>(),
            params[i],
            });
        registry.emplace<lab::noodle::Name>(pin_id.id, names[i]);
        registry.emplace<lab::noodle::Pin>(pin_id.id, lab::noodle::Pin{
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
    ln_Node node = entity_for_node_named(node_name);
    if (!node.valid)
        return;

    auto n = registry().get<std::shared_ptr<lab::AudioNode>>(node.id);
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

void LabSoundProvider::pin_set_bus_from_file(ln_Pin pin_id, const std::string& path)
{
    if (!pin_id.valid || !path.length())
        return;

    entt::registry& registry = Registry();
    lab::noodle::Pin& pin = registry.get<lab::noodle::Pin>(pin_id.id);
    AudioPin& a_pin = registry.get<AudioPin>(pin_id.id);
    if (pin.kind == lab::noodle::Pin::Kind::Setting && a_pin.setting)
    {
        auto soundClip = lab::MakeBusFromFile(path.c_str(), false);
        a_pin.setting->setBus(soundClip.get());
        printf("SetBusSetting %d %s\n", pin_id.id, path.c_str());
    }
}

void LabSoundProvider::connect_bus_out_to_bus_in(ln_Node output_node_id, ln_Pin output_pin_id, ln_Node input_node_id)
{
    if (!output_node_id.valid || !output_pin_id.valid || !input_node_id.valid)
        return;

    entt::registry& registry = Registry();
    shared_ptr<lab::AudioNode> in = registry.get<shared_ptr<lab::AudioNode>>(input_node_id.id);
    shared_ptr<lab::AudioNode> out = registry.get<shared_ptr<lab::AudioNode>>(output_node_id.id);
    if (!in || !out)
        return;

    g_audio_context->connect(in, out, 0, 0);
    printf("ConnectBusOutToBusIn %d %d\n", input_node_id.id, output_node_id.id);
}

void LabSoundProvider::connect_bus_out_to_param_in(ln_Node output_node_id, ln_Pin output_pin_id, ln_Pin param_pin_id)
{
    if (!output_node_id.valid || !output_pin_id.valid || !param_pin_id.valid)
        return;

    entt::registry& registry = Registry();
    AudioPin& param_pin = registry.get<AudioPin>(param_pin_id.id);
    lab::noodle::Pin& param_noodle_pin = registry.get<lab::noodle::Pin>(param_pin_id.id);
    if (!registry.valid(param_noodle_pin.node_id.id) || !param_pin.param)
        return;

    shared_ptr<lab::AudioNode> out = registry.get<shared_ptr<lab::AudioNode>>(output_node_id.id);
    if (!out)
        return;

    int output_index = 0;
    if (output_pin_id.id != entt::null)
    {
        AudioPin& output_pin = registry.get<AudioPin>(output_pin_id.id);
        output_index = output_pin.output_index;
    }

    g_audio_context->connectParam(param_pin.param, out, output_index);
    printf("ConnectBusOutToParamIn %d %d, index %d\n", param_pin_id.id, output_node_id.id, output_index);
}

void LabSoundProvider::disconnect(ln_Connection connection_id_)
{
    entt::registry& registry = Registry();
    entt::entity& connection_id = connection_id_.id;
    if (!registry.valid(connection_id)) return;

    lab::noodle::Connection& conn = registry.get<lab::noodle::Connection>(connection_id);
    ln_Node input_node_id = copy(conn.node_to);
    ln_Node output_node_id = copy(conn.node_from);
    ln_Pin input_pin = copy(conn.pin_to);
    ln_Pin output_pin = copy(conn.pin_from);
    if (input_node_id.valid && output_node_id.valid && input_pin.valid && output_pin.valid)
    {
        shared_ptr<lab::AudioNode> input_node = registry.get<shared_ptr<lab::AudioNode>>(output_node_id.id);
        shared_ptr<lab::AudioNode> output_node = registry.get<shared_ptr<lab::AudioNode>>(output_node_id.id);
        if (input_node && output_node)
        {
            AudioPin& a_in_pin = registry.get<AudioPin>(input_pin.id);
            lab::noodle::Pin& in_pin = registry.get<lab::noodle::Pin>(input_pin.id);
            lab::noodle::Pin& out_pin = registry.get<lab::noodle::Pin>(output_pin.id);
            if ((in_pin.kind == lab::noodle::Pin::Kind::BusIn) && (out_pin.kind == lab::noodle::Pin::Kind::BusOut))
            {
                g_audio_context->disconnect(input_node, output_node, 0, 0);
                printf("DisconnectInFromOut (bus from bus) %d %d\n", input_node_id.id, output_node_id.id);
            }
            else if ((in_pin.kind == lab::noodle::Pin::Kind::Param) && (out_pin.kind == lab::noodle::Pin::Kind::BusOut))
            {
                g_audio_context->disconnectParam(a_in_pin.param, output_node, 0);
                printf("DisconnectInFromOut (param from bus) %d %d\n", input_node_id.id, output_node_id.id);
            }
        }
    }

    //// @TODO this plumbing belongs in lab_noodle as it is audio-agnostic
    registry.destroy(connection_id);
    /// @TODO end
    return;
}

ln_Context LabSoundProvider::create_runtime_context(ln_Node id)
{
    const auto defaultAudioDeviceConfigurations = GetDefaultAudioDeviceConfiguration(true);

    if (!g_audio_context)
        g_audio_context = lab::MakeRealtimeAudioContext(defaultAudioDeviceConfigurations.second, defaultAudioDeviceConfigurations.first);

    entt::registry& registry = Registry();
    registry.emplace<shared_ptr<lab::AudioNode>>(id.id, g_audio_context->device());

    auto it = _noodleNodes.find(id);
    if (it == _noodleNodes.end()) {
        printf("Could not create runtime context\n");
        return ln_Context{ entt::null };
    }
    lab::noodle::NoodleNode& node = it->second;
    CreateEntities(g_audio_context->device(), node, id);
    printf("CreateRuntimeContext %d\n", id.id);
    return ln_Context{id.id};
}

void LabSoundProvider::node_start_stop(ln_Node node_id, float when)
{
    entt::registry& registry = Registry();
    if (node_id.id == entt::null)
        return;

    shared_ptr<lab::AudioNode> in_node = registry.get<shared_ptr<lab::AudioNode>>(node_id.id);
    if (!in_node)
        return;

    lab::AudioScheduledSourceNode* n = dynamic_cast<lab::AudioScheduledSourceNode*>(in_node.get());
    if (n)
    {
        if (n->isPlayingOrScheduled()) {
            printf("Stop %d\n", node_id.id);
            n->stop(when);
        }
        else {
            printf("Start %d\n", node_id.id);
            n->start(when);
        }
    }
}

void LabSoundProvider::node_bang(ln_Node node_id)
{
    entt::registry& registry = Registry();
    if (node_id.id == entt::null)
        return;

    shared_ptr<lab::AudioNode> in_node = registry.get<shared_ptr<lab::AudioNode>>(node_id.id);
    if (!in_node)
        return;

    shared_ptr<lab::AudioParam> gate = in_node->param("gate");
    if (gate)
    {
        gate->setValueAtTime(1.f, static_cast<float>(g_audio_context->currentTime()) + 0.f);
        gate->setValueAtTime(0.f, static_cast<float>(g_audio_context->currentTime()) + 1.f);
    }

    printf("Bang %d\n", node_id.id);
}

ln_Pin LabSoundProvider::node_output_named(ln_Node node_id, const std::string& output_name)
{
    if (!node_id.valid)
        return { entt::null, false };

    entt::registry& registry = Registry();
    shared_ptr<lab::AudioNode> node = registry.get<shared_ptr<lab::AudioNode>>(node_id.id);
    if (!node)
        return { entt::null, false };

    auto reverse_it = g_node_reverse_lookups.find(node_id.id);
    if (reverse_it == g_node_reverse_lookups.end())
        return { entt::null, false };

    auto& reverse = reverse_it->second;
    auto output_it = reverse.output_pin_map.find(output_name);
    if (output_it == reverse.output_pin_map.end())
        return { entt::null, false };

    ln_Pin result = copy(output_it->second);
    if (!result.valid)
        reverse.output_pin_map.erase(output_it);
    
    return result;
}

ln_Pin LabSoundProvider::node_input_with_index(ln_Node node_id, int output)
{
    if (!node_id.valid)
        return { entt::null, false };

    entt::registry& registry = Registry();
    shared_ptr<lab::AudioNode> node = registry.get<shared_ptr<lab::AudioNode>>(node_id.id);
    if (!node)
        return { entt::null, false };

    auto reverse_it = g_node_reverse_lookups.find(node_id.id);
    if (reverse_it == g_node_reverse_lookups.end())
        return { entt::null, false };

    auto& reverse = reverse_it->second;
    auto input_it = reverse.input_pin_map.find("");
    if (input_it == reverse.input_pin_map.end())
        return { entt::null, false };

    ln_Pin result = copy(input_it->second);
    if (!result.valid)
        reverse.input_pin_map.erase(input_it);
    
    return result;
}

ln_Pin LabSoundProvider::node_output_with_index(ln_Node node_id, int output)
{
    if (!node_id.valid)
        return { entt::null, false };

    entt::registry& registry = Registry();
    shared_ptr<lab::AudioNode> node = registry.get<shared_ptr<lab::AudioNode>>(node_id.id);
    if (!node)
        return { entt::null, false };

    auto reverse_it = g_node_reverse_lookups.find(node_id.id);
    if (reverse_it == g_node_reverse_lookups.end())
        return { entt::null, false };

    auto& reverse = reverse_it->second;
    auto output_it = reverse.output_pin_map.find("");
    if (output_it == reverse.output_pin_map.end())
        return { entt::null, false };

    ln_Pin result = copy(output_it->second);
    if (!result.valid)
        reverse.output_pin_map.erase(output_it);
    
    return result;
}

ln_Pin LabSoundProvider::node_param_named(ln_Node node_id, const std::string& output_name)
{
    if (!node_id.valid)
        return { entt::null, false };

    entt::registry& registry = Registry();
    shared_ptr<lab::AudioNode> node = registry.get<shared_ptr<lab::AudioNode>>(node_id.id);
    if (!node)
        return { entt::null, false };

    auto reverse_it = g_node_reverse_lookups.find(node_id.id);
    if (reverse_it == g_node_reverse_lookups.end())
        return { entt::null, false };

    auto& reverse = reverse_it->second;
    auto output_it = reverse.param_pin_map.find(output_name);
    if (output_it == reverse.param_pin_map.end())
        return { entt::null, false };

    ln_Pin result = copy(output_it->second);
    if (!result.valid)
        reverse.param_pin_map.erase(output_it);
    
    return result;
}

ln_Node LabSoundProvider::node_create(const std::string& kind, ln_Node id)
{
    entt::registry& registry = Registry();
    if (kind == "OSC")
    {
        shared_ptr<OSCNode> n = std::make_shared<OSCNode>(*g_audio_context.get());
        registry.emplace<shared_ptr<OSCNode>>(id.id, n);
        registry.emplace<shared_ptr<lab::AudioNode>>(id.id, n);
        _osc_node = id;
        return id;
    }

    shared_ptr<lab::AudioNode> n = NodeFactory(kind);
    if (n)
    {
        auto node = _noodleNodes.find(id);
        if (node != _noodleNodes.end()) {
            node->second.play_controller = n->isScheduledNode();
            node->second.bang_controller = !!n->param("gate");
            registry.emplace<shared_ptr<lab::AudioNode>>(id.id, n);
            CreateEntities(n, node->second, id);
            printf("CreateNode [%s] %d\n", kind.c_str(), id.id);
        }
    }
    else
    {
        printf("Could not CreateNode [%s]\n", kind.c_str());
    }

    return ln_Node{ id };
}

void LabSoundProvider::node_delete(ln_Node node_id)
{
    entt::registry& registry = Registry();
    if (node_id.id != entt::null && registry.valid(node_id.id))
    {
        printf("DeleteNode %d\n", node_id.id);

        // force full disconnection
        if (registry.has<shared_ptr<lab::AudioNode>>(node_id.id))
        {
            shared_ptr<lab::AudioNode> in_node = registry.get<shared_ptr<lab::AudioNode>>(node_id.id);
            g_audio_context->disconnect(in_node);
        }

        /// @TODO this bit should be managed in lab_noodle
        // delete all the node's pins
        for (const auto entity : registry.view<lab::noodle::Pin>())
        {
            lab::noodle::Pin& pin = registry.get<lab::noodle::Pin>(entity);
            if (pin.node_id.id == entity)
                registry.destroy(entity);
        }

        //// @TODO this plumbing belongs in lab_noodle as it is audio-agnostic
        // remove connections
        for (const auto entity : registry.view<lab::noodle::Connection>())
        {
            lab::noodle::Connection& conn = registry.get<lab::noodle::Connection>(entity);
            if (conn.node_from.id == node_id.id || conn.node_to.id == node_id.id)
                registry.destroy(entity);
        }

        /// @TODO end plumbing

        auto reverse_it = g_node_reverse_lookups.find(node_id.id);
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
    static const char** names = nullptr;
    if (!names)
    {
        static auto src_names = lab::NodeRegistry::Instance().Names();
        names = reinterpret_cast<const char**>(malloc(sizeof(char*) * (src_names.size() + 1)));
        if (names)
        {
            for (int i = 0; i < src_names.size(); ++i)
                names[i] = src_names[i].c_str();

            names[src_names.size()] = nullptr;
        }
    }
    return names;
}

void LabSoundProvider::pin_set_param_value(const std::string& node_name, const std::string& param_name, float v)
{
    ln_Node node = entity_for_node_named(node_name);
    if (!node.valid)
        return;

    auto n = registry().get<std::shared_ptr<lab::AudioNode>>(node.id);
    if (!n)
        return;

    auto p = n->param(param_name.c_str());
    if (p)
        p->setValue(v);
}

void LabSoundProvider::pin_set_setting_float_value(const std::string& node_name, const std::string& setting_name, float v)
{
    ln_Node node = entity_for_node_named(node_name);
    if (!node.valid)
        return;

    auto n = registry().get<std::shared_ptr<lab::AudioNode>>(node.id);
    if (!n)
        return;

    auto s = n->setting(setting_name.c_str());
    if (s)
        s->setFloat(v);
}

void LabSoundProvider::pin_set_float_value(ln_Pin pin, float v)
{
    if (!pin.valid)
        return;
    
    entt::registry& registry = Registry();
    AudioPin& a_pin = registry.get<AudioPin>(pin.id);
    if (a_pin.param)
    {
        a_pin.param->setValue(v);
        printf("SetParam(%f) %d\n", v, pin.id);
    }
    else if (a_pin.setting)
    {
        a_pin.setting->setFloat(v);
        printf("SetFloatSetting(%f) %d\n", v, pin.id);
    }
}

float LabSoundProvider::pin_float_value(ln_Pin pin)
{
    if (!pin.valid)
        return 0.f;

    AudioPin& a_pin = registry().get<AudioPin>(pin.id);
    if (a_pin.param)
        return a_pin.param->value();
    else if (a_pin.setting)
        return a_pin.setting->valueFloat();
    else
        return 0.f;
}

void LabSoundProvider::pin_set_setting_int_value(const std::string& node_name, const std::string& setting_name, int v)
{
    ln_Node node = entity_for_node_named(node_name);
    if (!node.valid)
        return;

    auto n = registry().get<std::shared_ptr<lab::AudioNode>>(node.id);
    if (!n)
        return;

    auto s = n->setting(setting_name.c_str());
    if (s)
        s->setUint32(v);
}

void LabSoundProvider::pin_set_int_value(ln_Pin pin, int v)
{
    if (!pin.valid)
        return;

    entt::registry& registry = Registry();
    AudioPin& a_pin = registry.get<AudioPin>(pin.id);
    if (a_pin.param)
    {
        a_pin.param->setValue(static_cast<float>(v));
        printf("SetParam(%d) %d\n", v, pin.id);
    }
    else if (a_pin.setting)
    {
        a_pin.setting->setUint32(v);
        printf("SetIntSetting(%d) %d\n", v, pin.id);
    }
}

int LabSoundProvider::pin_int_value(ln_Pin pin)
{
    if (!pin.valid)
        return 0;

    AudioPin& a_pin = registry().get<AudioPin>(pin.id);
    if (a_pin.param)
        return static_cast<int>(a_pin.param->value());
    else if (a_pin.setting)
        return a_pin.setting->valueUint32();
    else
        return 0;
}


void LabSoundProvider::pin_set_enumeration_value(ln_Pin pin, const std::string& value)
{
    if (!pin.valid)
        return;

    entt::registry& registry = Registry();
    AudioPin& a_pin = registry.get<AudioPin>(pin.id);
    if (a_pin.setting)
    {
        int e = a_pin.setting->enumFromName(value.c_str());
        if (e >= 0)
        {
            a_pin.setting->setUint32(e);
            printf("SetEnumSetting(%d) %d\n", e, pin.id);
        }
    }
}

void LabSoundProvider::pin_set_setting_enumeration_value(const std::string& node_name, const std::string& setting_name, const std::string& value)
{
    ln_Node node = entity_for_node_named(node_name);
    if (!node.valid)
        return;

    auto n = registry().get<std::shared_ptr<lab::AudioNode>>(node.id);
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
    ln_Node node = entity_for_node_named(node_name);
    if (!node.valid)
        return;

    auto n = registry().get<std::shared_ptr<lab::AudioNode>>(node.id);
    if (!n)
        return;

    auto s = n->setting(setting_name.c_str());
    if (s)
        s->setBool(v);
}

void LabSoundProvider::pin_set_bool_value(ln_Pin pin, bool v)
{
    if (!pin.valid)
        return;

    entt::registry& registry = Registry();
    AudioPin& a_pin = registry.get<AudioPin>(pin.id);
    if (a_pin.param)
    {
        a_pin.param->setValue(v? 1.f : 0.f);
        printf("SetParam(%d) %d\n", v, pin.id);
    }
    else if (a_pin.setting)
    {
        a_pin.setting->setBool(v);
        printf("SetBoolSetting(%s) %d\n", v ? "true": "false", pin.id);
    }
}

bool LabSoundProvider::pin_bool_value(ln_Pin pin)
{
    if (!pin.valid)
        return false;
    
    AudioPin& a_pin = registry().get<AudioPin>(pin.id);
    if (a_pin.param)
        return a_pin.param->value() != 0.f;
    else if (a_pin.setting)
        return a_pin.setting->valueBool();
    else
        return false;
}

void LabSoundProvider::pin_create_output(const std::string& node_name, const std::string& output_name, int channels)
{
    ln_Node node_e = entity_for_node_named(node_name);
    if (!node_e.valid)
        return;

    auto n = registry().get<std::shared_ptr<lab::AudioNode>>(node_e.id);
    if (!n)
        return;

    if (!n->output(output_name.c_str()))
    {
        auto reverse_it = g_node_reverse_lookups.find(node_e.id);
        if (reverse_it == g_node_reverse_lookups.end())
        {
            g_node_reverse_lookups[node_e.id] = NodeReverseLookup{};
            reverse_it = g_node_reverse_lookups.find(node_e.id);
        }
        auto& reverse = reverse_it->second;

        ln_Pin pin_id{ registry().create(), true };

        auto node = _noodleNodes.find(node_e);
        if (node == _noodleNodes.end()) {
            printf("Could not find node %s\n", node_name.c_str());
            return;
        }

        node->second.pins.push_back(pin_id);
        reverse.output_pin_map[output_name] = ln_Pin{ pin_id };
        registry().emplace<lab::noodle::Name>(pin_id.id, output_name);
        registry().emplace<lab::noodle::Pin>(pin_id.id, lab::noodle::Pin{
            lab::noodle::Pin::Kind::BusOut,
            lab::noodle::Pin::DataType::Bus,
            "",
            pin_id, node_e,
            });
        registry().emplace<AudioPin>(pin_id.id, n->numberOfOutputs() - 1);

        lab::ContextGraphLock glock(g_audio_context.get(), "AudioHardwareDeviceNode");
        n->addOutput(glock, std::unique_ptr<lab::AudioNodeOutput>(new lab::AudioNodeOutput(n.get(), output_name.c_str(), channels)));
    }
}

float LabSoundProvider::node_get_timing(ln_Node node)
{
    if (!registry().valid(node.id))
        return 0;

    auto n = registry().get<std::shared_ptr<lab::AudioNode>>(node.id);
    if (!n)
        return 0;
    return n->graphTime.microseconds.count() * 1.e-6f;
}

float LabSoundProvider::node_get_self_timing(ln_Node node)
{
    if (!registry().valid(node.id))
        return 0;

    if (!registry().has<std::shared_ptr<lab::AudioNode>>(node.id))
        return 0;

    auto n = registry().get<std::shared_ptr<lab::AudioNode>>(node.id);
    if (!n)
        return 0;

    return (n->totalTime.microseconds.count() - n->graphTime.microseconds.count()) * 1.e-6f;
}

void LabSoundProvider::add_osc_addr(char const* const addr, int addr_id, int channels, float* data)
{
    if (_osc_node.id == entt::null)
        return;

    entt::registry& registry = Registry();
    shared_ptr<OSCNode> n = registry.get<shared_ptr<OSCNode>>(_osc_node.id);
    if (!n->addAddress(addr, addr_id, channels, data))
        return;

    auto it = n->key_to_addrData.find(addr_id);
    if (it != n->key_to_addrData.end())
    {
        auto node = _noodleNodes.find(_osc_node);
        if (node == _noodleNodes.end())
            return;

        ln_Pin pin_id{ registry.create(), true };
        node->second.pins.push_back(pin_id);
        registry.emplace<lab::noodle::Name>(pin_id.id, addr);
        registry.emplace<lab::noodle::Pin>(pin_id.id, lab::noodle::Pin{
            lab::noodle::Pin::Kind::BusOut,
            lab::noodle::Pin::DataType::Bus,
            "",
            pin_id, _osc_node,
            });
        registry.emplace<AudioPin>(pin_id.id, AudioPin{ it->second.output_index });
    }
}
