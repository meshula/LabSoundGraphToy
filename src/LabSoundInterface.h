#ifndef LABSOUNDINTERFACE_H
#define LABSOUNDINTERFACE_H

/*
    LabSoundInterface provides a concrete bridge from a
    lab::noodle schematic to the LabSound audio engine.

    Ultimately, lab_noodle.cpp will not link to this file, instead
    main will create a LabSoundProvider as an abstract interface to the
    noodle engine.
*/

#include "lab_noodle.h"

#include <entt/entt.hpp>
#include <memory>
#include <string>

namespace lab {
    class AudioParam;
    class AudioSetting;
}

namespace lab { namespace Sound {


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
    Work() = default;
    ~Work() = default;

    WorkType type = WorkType::Nop;
    entt::entity input_node_id = entt::null;
    entt::entity output_node_id = entt::null;
    entt::entity pin_id = entt::null;
    entt::entity connection_id = entt::null;
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
    char const* const* node_names() const;

    float pin_float_value(entt::entity pin);
    int pin_int_value(entt::entity pin);
    bool pin_bool_value(entt::entity pin);
    bool node_has_play_controller(entt::entity node);
    bool node_has_bang_controller(entt::entity node);
    float get_timing_ms(entt::entity node);
    float get_self_timing_ms(entt::entity node);
};

}} // lab::Sound

#endif
