
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
