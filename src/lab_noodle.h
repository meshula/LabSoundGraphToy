
#ifndef included_noodle_h
#define included_noodle_h

#include <entt/entt.hpp>
#include <functional>
#include <string>

namespace lab { namespace noodle {

    //--------------------------------------------------------------------------
    // Schematic
    //
    // A schematic is comprised of nodes; nodes have collections of pins;
    // and connections connect a pin in one node to a pin in another.
    //
    // Any element in a schematic may have a name.
    //
    //--------------------------------------------------------------------------

    // The node is a primary entity
    // every AudioNode is a Node, but not every Node is an AudioNode.
    // for example, a Documentation node might be known only to lab noodle.
    struct Node
    {
    };

    // Some nodes may have overridden draw methods, such as the LabSound 
    // AnalyserNode. If such exists, the associated NodeRender will have a 
    // functor to call.
    /// @TODO hook it up
    struct NodeRender
    {
        std::function<void(entt::entity)> render;
    };

    // every entity may have a name
    struct Name
    {
        std::string name;
    };

    // given a proposed name, of the form name, or name-1 create a new
    // unique name of the form name-2.
    std::string unique_name(std::string proposed_name);

    // pins have kind. Settings can't be connected to.
    // Busses carry signals, and parameters parameterize a node.
    // Busses can connect to paramaters to drive them.
    struct Pin
    {
        enum class Kind { Setting, Param, BusIn, BusOut };
        Kind kind;
        std::string  shortName;
        entt::entity pin_id;
        entt::entity node_id;
        std::string  value_as_string;
    };

    // PinSetter, if it exists, is a mechanism by which a pin can 
    // accept a value in the form of a string. This is how lab_noodle
    // will no longer need to know about LabSound specifically
    // ditto for PinGetter
    /// @TODO hook them up
    struct PinSetter
    {
        std::function<void(entt::entity, std::string)> set;
    };
    struct PinGetter
    {
        std::function<std::string(entt::entity)> get;
    };

    // Schematic nodes may be connected from a pin on one node to a pin on another
    struct Connection
    {
        entt::entity id;
        entt::entity pin_from;
        entt::entity node_from;
        entt::entity pin_to;
        entt::entity node_to;
    };

    //--------------------------------------------------------------------------
    // Runtime
    //--------------------------------------------------------------------------

    enum class Command
    {
        None,
        New,
        Open,
        Save
    };

    struct RunConfig
    {
        bool show_profiler = false;
        bool show_debug = false;
        Command command = Command::None;
    };

    bool run_noodles(RunConfig&);
} }

#endif
