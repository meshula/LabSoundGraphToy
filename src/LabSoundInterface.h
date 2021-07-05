#ifndef included_labsoundinterface_h
#define included_labsoundinterface_h

/*
    LabSoundInterface provides a concrete bridge from a
    lab::noodle schematic to the LabSound audio engine.
*/

#include "lab_noodle.h"

#include <entt/entt.hpp>
#include <map>
#include <memory>
#include <string>
#include <vector>

class LabSoundProvider final : public lab::noodle::Provider
{
public:
    virtual ~LabSoundProvider() override = default;

    virtual entt::registry& registry() const override;

    virtual ln_Context create_runtime_context(ln_Node id) override;

    // node creation and deletion
    virtual char const* const* node_names() const override;
    virtual ln_Node node_create(const std::string& name, ln_Node id) override;
    virtual void node_delete(ln_Node node) override;

    // node access
    virtual float node_get_timing(ln_Node node) override;      // in seconds
    virtual float node_get_self_timing(ln_Node node) override; // in seconds
    virtual void  node_start_stop(ln_Node node, float when) override;
    virtual void  node_bang(ln_Node node) override;

    virtual ln_Pin node_input_with_index(ln_Node node, int output) override;
    virtual ln_Pin node_output_named(ln_Node node, const std::string& output_name) override;
    virtual ln_Pin node_output_with_index(ln_Node node, int output) override;
    virtual ln_Pin node_param_named(ln_Node node, const std::string& output_name) override;

    // pins
    virtual void  pin_set_param_value(const std::string& node_name, const std::string& param_name, float) override;
    virtual void  pin_set_setting_float_value(const std::string& node_name, const std::string& setting_name, float) override;
    virtual void  pin_set_float_value(ln_Pin pin, float) override;
    virtual float pin_float_value(ln_Pin pin) override;
    virtual void  pin_set_setting_int_value(const std::string& node_name, const std::string& setting_name, int) override;
    virtual void  pin_set_int_value(ln_Pin pin, int) override;
    virtual int   pin_int_value(ln_Pin pin) override;
    virtual void  pin_set_setting_bool_value(const std::string& node_name, const std::string& setting_name, bool) override;
    virtual void  pin_set_bool_value(ln_Pin pin, bool) override;
    virtual bool  pin_bool_value(ln_Pin pin) override;
    virtual void  pin_set_setting_bus_value(const std::string& node_name, const std::string& setting_name, const std::string& path) override;
    virtual void  pin_set_bus_from_file(ln_Pin pin, const std::string& path) override;
    virtual void  pin_set_enumeration_value(ln_Pin pin, const std::string& value) override;
    virtual void  pin_set_setting_enumeration_value(const std::string& node_name, const std::string& setting_name, const std::string& value) override;

    // string based interfaces
    virtual void pin_create_output(const std::string& node_name, const std::string& output_name, int channels) override;

    // connections
    virtual void connect_bus_out_to_bus_in(ln_Node node_out_id, ln_Pin output_pin_id, ln_Node node_in_id) override;
    virtual void connect_bus_out_to_param_in(ln_Node output_node_id, ln_Pin output_pin_id, ln_Pin pin_id) override;
    virtual void disconnect(ln_Connection connection_id) override;

    void add_osc_addr(char const* const addr, int addr_id, int channels, float* data);

    ln_Node _osc_node = ln_Node_null();
};

#endif
