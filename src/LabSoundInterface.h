#ifndef LABSOUNDINTERFACE_H
#define LABSOUNDINTERFACE_H

#include <entt/entt.hpp>
#include <memory>
#include <string>

namespace lab {
    class AudioParam;
    class AudioSetting;
}

namespace lab { namespace Sound {

//--------------------------------------------------------------
// AudioPins are created for every created node

struct AudioPin
{
    enum class Kind { Setting, Param, BusIn, BusOut };
    Kind kind;

    std::string name, shortName;
    entt::entity pin_id;
    entt::entity node_id;
    std::shared_ptr<lab::AudioSetting> setting;
    std::shared_ptr<lab::AudioParam> param;

    std::string value_as_string;
};

//--------------------------------------------------------------

struct AudioNode
{
    std::string name;
};

//--------------------------------------------------------------
struct Connection
{
    entt::entity id;
    entt::entity pin_from;
    entt::entity node_from;
    entt::entity pin_to;
    entt::entity node_to;
};

//--------------------------------------------------------------
// LabSound Work - everything the lab sound interfaces can do

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
    Work()
        : input_node_id(entt::null), output_node_id(entt::null), pin_id(entt::null) {}

    ~Work() = default;

    WorkType type = WorkType::Nop;
    entt::entity input_node_id;
    entt::entity output_node_id;
    entt::entity pin_id;
    entt::entity connection_id;
    std::string name;
    float float_value = 0.f;
    int int_value = 0;
    bool bool_value = false;

    entt::entity eval();
};

class Provider
{
public:
    entt::entity Create(char const* const name);
    std::vector<entt::entity>& pins(entt::entity audio_node_id) const;
    entt::registry& registry() const;
};

}} // lab::Sound

#endif
