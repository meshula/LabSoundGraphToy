#ifndef included_labsoundinterface_h
#define included_labsoundinterface_h

/*
    LabSoundInterface provides a concrete bridge from a
    lab::noodle schematic to the LabSound audio engine.
*/

#include "lab_noodle.h"

#include <entt/entt.hpp>
#include <memory>
#include <string>
#include <vector>

//--------------------------------------------------------------

class LabSoundProvider final : public lab::noodle::Provider
{
public:
    virtual ~LabSoundProvider() override = default;

    virtual entt::registry& registry() const override;

    virtual entt::entity create_runtime_context() override;

    // node creation and deletion
    virtual char const* const* node_names() const override;
    virtual entt::entity node_create(const std::string& name) override;
    virtual void node_delete(entt::entity node) override;

    // node access
    virtual bool  node_has_play_controller(entt::entity node) override;
    virtual bool  node_has_bang_controller(entt::entity node) override;
    virtual float node_get_timing(entt::entity node) override;      // in seconds
    virtual float node_get_self_timing(entt::entity node) override; // in seconds
    virtual void  node_start_stop(entt::entity node, float when) override;
    virtual void  node_bang(entt::entity node) override;

    // pins
    virtual std::vector<entt::entity>& pins(entt::entity audio_node_id) const override;
    virtual void  pin_set_float_value(entt::entity pin, float) override;
    virtual float pin_float_value(entt::entity pin) override;
    virtual void  pin_set_int_value(entt::entity pin, int) override;
    virtual int   pin_int_value(entt::entity pin) override;
    virtual void  pin_set_bool_value(entt::entity pin, bool) override;
    virtual bool  pin_bool_value(entt::entity pin) override;
    virtual void  pin_set_bus_from_file(entt::entity pin, const std::string& path) override;

    // connections
    virtual entt::entity connect_bus_out_to_bus_in(entt::entity node_out_id, entt::entity node_in_id) override;
    virtual entt::entity connect_bus_out_to_param_in(entt::entity output_node_id, entt::entity pin_id) override;
    virtual void disconnect(entt::entity connection_id) override;

    // loading and saving
    void save(const std::string& path);
};

#endif
