
#ifndef included_noodle_h
#define included_noodle_h

#include <entt/entt.hpp>
#include <functional>
#include <string>
#include <map>

namespace lab { namespace noodle {

    struct vec2 { float x, y; };

    class Provider
    {
        std::map<std::string, entt::entity> _name_to_entity;

    public:
        virtual ~Provider() = default;

        virtual entt::registry& registry() const = 0;
        virtual entt::entity create_runtime_context(entt::entity id) = 0;

        void associate(entt::entity node, const std::string& name)
        {
            _name_to_entity[name] = node;
        }
        entt::entity entity_for_node_named(const std::string& name)
        {
            auto it = _name_to_entity.find(name);
            if (it == _name_to_entity.end())
                return entt::null;
            return it->second;
        }
        void clear_entity_node_associations()
        {
            _name_to_entity.clear();
        }

        // node creation and deletion
        virtual char const* const* node_names() const = 0;
        virtual entt::entity node_create(const std::string& name, entt::entity id) = 0;
        virtual void node_delete(entt::entity node) = 0;

        // node access
        virtual float node_get_timing(entt::entity node) = 0;
        virtual float node_get_self_timing(entt::entity node) = 0;
        virtual void  node_start_stop(entt::entity node, float when) = 0;
        virtual void  node_bang(entt::entity node) = 0;

        virtual entt::entity node_input_with_index(entt::entity node, int output) = 0;
        virtual entt::entity node_output_named(entt::entity node, const std::string& output_name) = 0;
        virtual entt::entity node_output_with_index(entt::entity node, int output) = 0;
        virtual entt::entity node_param_named(entt::entity node, const std::string& output_name) = 0;

        // pins
        virtual void  pin_set_param_value(const std::string& node_name, const std::string& param_name, float) = 0;
        virtual void  pin_set_setting_float_value(const std::string& node_name, const std::string& setting_name, float) = 0;
        virtual void  pin_set_float_value(entt::entity pin, float) = 0;
        virtual float pin_float_value(entt::entity pin) = 0;
        virtual void  pin_set_setting_int_value(const std::string& node_name, const std::string& setting_name, int) = 0;
        virtual void  pin_set_int_value(entt::entity pin, int) = 0;
        virtual int   pin_int_value(entt::entity pin) = 0;
        virtual void  pin_set_setting_bool_value(const std::string& node_name, const std::string& setting_name, bool) = 0;
        virtual void  pin_set_bool_value(entt::entity pin, bool) = 0;
        virtual bool  pin_bool_value(entt::entity pin) = 0;
        virtual void  pin_set_setting_bus_value(const std::string& node_name, const std::string& setting_name, const std::string& path) = 0;
        virtual void  pin_set_bus_from_file(entt::entity pin, const std::string& path) = 0;
        virtual void  pin_set_enumeration_value(entt::entity pin, const std::string& value) = 0;
        virtual void  pin_set_setting_enumeration_value(const std::string& node_name, const std::string& setting_name, const std::string& value) = 0;

        // string based interfaces
        virtual void pin_create_output(const std::string& node_name, const std::string& output_name, int channel) = 0;

        // connections
        virtual void connect_bus_out_to_bus_in(entt::entity node_out_id, entt::entity output_pin_id, entt::entity node_in_id) = 0;
        virtual void connect_bus_out_to_param_in(entt::entity output_node_id, entt::entity output_pin_id, entt::entity pin_id) = 0;
        virtual void disconnect(entt::entity connection_id) = 0;
    };


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
        Node() noexcept = default;
        Node(const std::string& n) noexcept : kind(n) {}
        ~Node() = default;

        bool play_controller = false;
        bool bang_controller = false;
        std::string kind;
        std::vector<entt::entity> pins;
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
    // Busses can connect to parameters to drive them.
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
        enum class Kind { ToBus, ToParam };

        Connection() = delete;

        explicit Connection(entt::entity id, entt::entity pin_from, entt::entity node_from, entt::entity pin_to, entt::entity node_to, Kind kind)
            : id(id), pin_from(pin_from), node_from(node_from), pin_to(pin_to), node_to(node_to), kind(kind) {}

        entt::entity id = entt::null;
        entt::entity pin_from = entt::null;
        entt::entity node_from = entt::null;
        entt::entity pin_to = entt::null;
        entt::entity node_to = entt::null;

        Kind kind = Kind::ToBus;
    };


    //--------------------------------------------------------------------------
    // Runtime
    //--------------------------------------------------------------------------


    struct Context
    {
        Context() = delete;
        explicit Context(Provider&);
        ~Context();

        Provider& provider;
        bool show_profiler = false;
        bool show_debug = false;
        bool show_demo = false;
        bool show_ids = false;
      
        bool run();

        // save and load do their work irrepsective of dirty state.
        // check needs_saving to determine if the user should be presented
        // with a save as dialog, or if save should not be called.
        // the Context is not responsible for the document on disk, only reading and writing,
        // so path is not tracked.
        bool needs_saving() const;
        void save(const std::string& path);
        void load(const std::string& path);
        void save_test(const std::string& path);
        void save_json(const std::string& path);
        void clear_all();

    private:
        struct State;
        State* _s;
    };

} }

#endif
