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

//--------------------------------------------------------------

#include <LabSound/core/AudioBus.h>
#include <LabSound/core/AudioNode.h>
#include <LabSound/core/AudioNodeOutput.h>
struct OSCNode : public lab::AudioNode
{
    OSCNode(lab::AudioContext& ac)
        : AudioNode(ac)
    {
        initialize();
    }
    virtual ~OSCNode() = default;

    struct AddrData
    {
        std::string addr;
        int output_index = 0;
        int value_count = 0;
        float value[3] = { 0,0,0 };
    };
    std::map<int, AddrData> key_to_addrData;

    // returns true if the address was added
    bool addAddress(char const*const addr, int addr_id, int channels, float* data)
    {
        auto it = key_to_addrData.find(addr_id);
        if (it != key_to_addrData.end())
        {
            // update the value to output
            for (int i = 0; i < channels; ++i)
                it->second.value[i] = data[i];

            return false;
        }

        AddrData d{ addr, numberOfOutputs(), channels };
        key_to_addrData[addr_id] = d;
        addOutput(std::unique_ptr<lab::AudioNodeOutput>(new lab::AudioNodeOutput(this, channels)));
        return true;
    }

    //--------------------------------------------------
    // required interface
    //
    virtual const char* name() const override { return "OSC"; }

    // The AudioNodeInput(s) (if any) will already have their input data available when process() is called.
    // Subclasses will take this input data and put the results in the AudioBus(s) of its AudioNodeOutput(s) (if any).
    // Called from context's audio thread.
    virtual void process(lab::ContextRenderLock& r, int bufferSize) override
    {
        /// @TODO make the value changes sample accurate
        for (auto i : key_to_addrData)
        {
            for (int j = 0; j < i.second.value_count; ++j)
            {
                if (output(i.second.output_index)->isConnected())
                {
                    float* buff = output(i.second.output_index)->bus(r)->channel(j)->mutableData();
                    for (int k = 0; k < bufferSize; ++k)
                        buff[k] = i.second.value[j];
                }
            }
        }
    }

    // Resets DSP processing state (clears delay lines, filter memory, etc.)
    // Called from context's audio thread.

    virtual void reset(lab::ContextRenderLock&) override { }

    // tailTime() is the length of time (not counting latency time) where non-zero output may occur after continuous silent input.
    virtual double tailTime(lab::ContextRenderLock& r) const override { return 0.; }

    // latencyTime() is the length of time it takes for non-zero output to appear after non-zero input is provided. This only applies to
    // processing delay which is an artifact of the processing algorithm chosen and is *not* part of the intrinsic desired effect. For
    // example, a "delay" effect is expected to delay the signal, and thus would not be considered latency.
    virtual double latencyTime(lab::ContextRenderLock& r) const override { return 0.; }
};

class LabSoundProvider final : public lab::noodle::Provider
{
public:
    virtual ~LabSoundProvider() override = default;

    virtual entt::registry& registry() const override;

    virtual entt::entity create_runtime_context(entt::entity id) override;

    // node creation and deletion
    virtual char const* const* node_names() const override;
    virtual entt::entity node_create(const std::string& name, entt::entity id) override;
    virtual void node_delete(entt::entity node) override;

    // node access
    virtual float node_get_timing(entt::entity node) override;      // in seconds
    virtual float node_get_self_timing(entt::entity node) override; // in seconds
    virtual void  node_start_stop(entt::entity node, float when) override;
    virtual void  node_bang(entt::entity node) override;

    virtual entt::entity node_input_with_index(entt::entity node, int output) override;
    virtual entt::entity node_output_named(entt::entity node, const std::string& output_name) override;
    virtual entt::entity node_output_with_index(entt::entity node, int output) override;
    virtual entt::entity node_param_named(entt::entity node, const std::string& output_name) override;

    // pins
    virtual void  pin_set_param_value(const std::string& node_name, const std::string& param_name, float) override;
    virtual void  pin_set_setting_float_value(const std::string& node_name, const std::string& setting_name, float) override;
    virtual void  pin_set_float_value(entt::entity pin, float) override;
    virtual float pin_float_value(entt::entity pin) override;
    virtual void  pin_set_setting_int_value(const std::string& node_name, const std::string& setting_name, int) override;
    virtual void  pin_set_int_value(entt::entity pin, int) override;
    virtual int   pin_int_value(entt::entity pin) override;
    virtual void  pin_set_setting_bool_value(const std::string& node_name, const std::string& setting_name, bool) override;
    virtual void  pin_set_bool_value(entt::entity pin, bool) override;
    virtual bool  pin_bool_value(entt::entity pin) override;
    virtual void  pin_set_setting_bus_value(const std::string& node_name, const std::string& setting_name, const std::string& path) override;
    virtual void  pin_set_bus_from_file(entt::entity pin, const std::string& path) override;
    virtual void  pin_set_enumeration_value(entt::entity pin, const std::string& value) override;
    virtual void  pin_set_setting_enumeration_value(const std::string& node_name, const std::string& setting_name, const std::string& value) override;

    // string based interfaces
    virtual void pin_create_output(const std::string& node_name, const std::string& output_name, int channels) override;

    // connections
    virtual void connect_bus_out_to_bus_in(entt::entity node_out_id, entt::entity output_pin_id, entt::entity node_in_id) override;
    virtual void connect_bus_out_to_param_in(entt::entity output_node_id, entt::entity output_pin_id, entt::entity pin_id) override;
    virtual void disconnect(entt::entity connection_id) override;

    void add_osc_addr(char const* const addr, int addr_id, int channels, float* data);

    entt::entity _osc_node = entt::null;
};

#endif
