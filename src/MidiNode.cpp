
#include "MidiNode.hpp"
#include "LabSound/extended/Registry.h"

#include <LabMidi/LabMidi.h>
#include <array>
#include <map>

const int kMaxMidiNotes = 128;
const int kMaxMidiChannels = 16;

class MidiManager
{
    Lab::MidiPorts _midi_ports;
    std::map<int, std::unique_ptr<Lab::MidiIn>> _midi_ins;

    typedef std::array<uint8_t, kMaxMidiNotes> NoteStatus;
    std::array<NoteStatus, kMaxMidiChannels> _notes;

    // for indicating if a channel is active
    std::array<int, kMaxMidiChannels> _noteRefCounts;

public:
    MidiManager()
    {
        clear_notes();
    }

    void clear_notes()
    {
        for (auto& i : _notes)
            i.fill(0);

        _noteRefCounts.fill(0);
    }

    void refresh_ports()
    {
        _midi_ports.refreshPortList();
    }

    void list_ports()
    {
        int c = _midi_ports.inPorts();
        if (c == 0)
            std::cout << "No MIDI input ports found\n\n";
        else {
            std::cout << "MIDI input ports:" << std::endl;
            for (int i = 0; i < c; ++i)
                std::cout << "   " << i << ": " << _midi_ports.inPort(i) << std::endl;
            std::cout << std::endl;
        }

        c = _midi_ports.outPorts();
        if (c == 0)
            std::cout << "No MIDI output ports found\n\n";
        else {
            std::cout << "MIDI output ports:" << std::endl;
            for (int i = 0; i < c; ++i)
                std::cout << "   " << i << ": " << _midi_ports.outPort(i) << std::endl;
            std::cout << std::endl;
        }
    }

    static void midi_callback(void* user_data, Lab::MidiCommand* midi_cmd)
    {
        MidiManager* self = (MidiManager*)user_data;

        uintptr_t port = reinterpret_cast<uintptr_t>(user_data);
        int cmd = midi_cmd->command < 0xF0 ? 
                    midi_cmd->command & 0xF0 :
                    midi_cmd->command;

        // only valid for commands < 0xF0
        int channel = cmd & 0xf;

        switch (midi_cmd->command)
        {
        case MIDI_NOTE_OFF: { //           0x80
            uint8_t note = midi_cmd->byte1;
            uint8_t release_vel = midi_cmd->byte2;
            self->_notes[channel][note & 0x7f] = 0;
            --self->_noteRefCounts[channel];
            break;
        }

        case MIDI_NOTE_ON: { //            0x90
            uint8_t note = midi_cmd->byte1;
            uint8_t attack_vel = midi_cmd->byte2;
            self->_notes[channel][note & 0x7f] = 1;
            ++self->_noteRefCounts[channel];
            break;
        }

        case MIDI_POLY_PRESSURE: { //      0xA0
            uint8_t note = midi_cmd->byte1;
            uint8_t pressure = midi_cmd->byte2;
            break;
        }

        case MIDI_CONTROL_CHANGE: { //     0xB0
            uint8_t controller_number = midi_cmd->byte1;
            uint8_t value = midi_cmd->byte2;
            break;
        }

        case MIDI_PROGRAM_CHANGE: { //     0xC0
            uint8_t program_number = midi_cmd->byte1;
            break;
        }

        case MIDI_CHANNEL_PRESSURE: { //   0xD0
            uint8_t pressure = midi_cmd->byte1;
            break;
        }

        case MIDI_PITCH_BEND: { //         0xE0
            uint16_t pitch_bend = midi_cmd->byte1 + (midi_cmd->byte2 << 8);
            break;
        }

        // system common
        case MIDI_SYSTEM_EXCLUSIVE: //   0xF0
        case MIDI_TIME_CODE: //          0xF1
        case MIDI_SONG_POS_POINTER: //   0xF2
        case MIDI_SONG_SELECT: //        0xF3
        case MIDI_RESERVED1: //          0xF4
        case MIDI_RESERVED2: //          0xF5
        case MIDI_TUNE_REQUEST: //       0xF6
        case MIDI_EOX: //                0xF7
            
        // system realtime
        case MIDI_TIME_CLOCK: //         0xF8
        case MIDI_RESERVED3: //          0xF9
        case MIDI_START: //              0xFA
        case MIDI_CONTINUE: //           0xFB
            break;

        case MIDI_STOP: //               0xFC
            self->clear_notes();
            break;

        case MIDI_RESERVED4: //          0xFD
        case MIDI_ACTIVE_SENSING: //     0xFE
        case MIDI_SYSTEM_RESET: //       0xFF
            break;
        }
    }

    void open_port(int p)
    {
        auto in = std::make_unique<Lab::MidiIn>();
        in->addCallback(midi_callback, (void*) this);
        in->openPort(p);
        _midi_ins[p] = std::move(in);
    }

    void close_port(int p)
    {
        auto it = _midi_ins.find(p);
        if (it != _midi_ins.end())
        {
            _midi_ins.erase(it);
        }
    }

};

