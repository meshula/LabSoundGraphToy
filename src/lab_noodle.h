
#ifndef included_noodle_h
#define included_noodle_h

#include <entt/entt.hpp>
#include <functional>
#include <string>

namespace lab { namespace noodle {

    struct vec2 { float x, y; };

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
    /// @TODO add a render delegate, so that a void* doesn't have to be passed
    /// where a DrawList would go
    struct NodeRender
    {
        // entity, ul_ws, lr_ws, scale, drawlist (typically ImGui::DrawList)
        std::function<void(entt::entity, vec2, vec2, float, void*)> render;
    };

    // every entity may have a name, Pins, Nodes, are typical. A scenegraph
    // might name connections as well, with terms such as "is parent of".
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
        Pin() = default;
        ~Pin() = default;
        enum class Kind { Setting, Param, BusIn, BusOut };
        enum class DataType { None, Bus, Bool, Integer, Enumeration, Float, String };
        Kind         kind = Kind::Setting;
        DataType     dataType = DataType::None;
        std::string  shortName;
        entt::entity pin_id = entt::null;
        entt::entity node_id = entt::null;
        std::string  value_as_string;
        char const* const* names = nullptr; // if an DataType is Enumeration, they'll be here
    };

    // PinEdit, if it exists, is a mechanism by which a pin can 
    // accept a value in the form of a string, or get a value as a string.
    // A custom edit routine could go here as well, as for the renderer.
    /// @TODO hook them up
    struct PinEdit
    {
        std::function<void(entt::entity, std::string)> set;
        std::function<std::string(entt::entity)> get;
    };
 
    // Schematic nodes may be connected from a pin on one node to a pin on another
    struct Connection
    {
        entt::entity id = entt::null;
        entt::entity pin_from = entt::null;
        entt::entity node_from = entt::null;
        entt::entity pin_to = entt::null;
        entt::entity node_to = entt::null;
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
