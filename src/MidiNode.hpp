#pragma once

//--------------------------------------------------------------

#include <LabSound/core/AudioBus.h>
#include <LabSound/core/AudioNode.h>
#include <LabSound/core/AudioNodeOutput.h>
#include <map>

struct MidiNode : public lab::AudioNode
{
    MidiNode(lab::AudioContext& ac)
        : AudioNode(ac)
    {
        initialize();
    }
    virtual ~MidiNode() = default;

    struct AddrData
    {
        std::string addr;
        int output_index = 0;
        int value_count = 0;
        float value[3] = { 0,0,0 };
    };
    std::map<int, AddrData> key_to_addrData;

    // returns true if the address was added
    bool addAddress(char const* const addr, int addr_id, int channels, float* data)
    {
        auto it = key_to_addrData.find(addr_id);
        bool must_add = it == key_to_addrData.end();

        if (must_add)
        {
            AddrData d{ addr, numberOfOutputs(), channels };
            key_to_addrData[addr_id] = d;
            addOutput(std::unique_ptr<lab::AudioNodeOutput>(new lab::AudioNodeOutput(this, channels)));
            it = key_to_addrData.find(addr_id);
        }

        if (it != key_to_addrData.end())
        {
            // update the value to output
            for (int i = 0; i < channels; ++i)
                it->second.value[i] = data[i];
        }

        return must_add;
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
