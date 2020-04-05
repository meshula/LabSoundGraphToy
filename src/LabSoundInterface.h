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


namespace lab { namespace Sound {


//--------------------------------------------------------------

class Provider
{
public:
    entt::registry& registry() const;

    entt::entity create_runtime_context();

    // node creation and deletion
    char const* const* node_names() const;
    entt::entity node_create(const std::string& name);
    void node_delete(entt::entity node);

    // node access
    bool  node_has_play_controller(entt::entity node);
    bool  node_has_bang_controller(entt::entity node);
    float node_get_timing_ms(entt::entity node);
    float node_get_self_timing_ms(entt::entity node);
    void  node_start_stop(entt::entity node, float when);
    void  node_bang(entt::entity node);

    // pins
    std::vector<entt::entity>& pins(entt::entity audio_node_id) const;
    void  pin_set_float_value(entt::entity pin, float);
    float pin_float_value(entt::entity pin);
    void  pin_set_int_value(entt::entity pin, int);
    int   pin_int_value(entt::entity pin);
    void  pin_set_bool_value(entt::entity pin, bool);
    bool  pin_bool_value(entt::entity pin);
    void  pin_set_bus_from_file(entt::entity pin, const std::string& path);

    // connections
    entt::entity connect_bus_out_to_bus_in(entt::entity node_out_id, entt::entity node_in_id);
    entt::entity connect_bus_out_to_param_in(entt::entity output_node_id, entt::entity pin_id);
    void disconnect(entt::entity connection_id);
};

}} // lab::Sound

#endif
