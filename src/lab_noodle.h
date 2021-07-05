
#ifndef included_noodle_h
#define included_noodle_h

#include <entt/entt.hpp>
#include <functional>
#include <string>
#include <map>

typedef struct { entt::entity id; } ln_Context;
typedef struct { entt::entity id; } ln_Connection;
typedef struct { entt::entity id; bool valid; } ln_Node;
typedef struct { entt::entity id; bool valid; } ln_Pin;

struct cmp_ln_Node {
    bool operator()(const ln_Node& a, const ln_Node& b) const {
        return a.id < b.id;
    }
};

namespace lab { namespace noodle {

    struct vec2 { float x, y; };

    struct ProviderHarness;

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
    struct NoodleNode
    {
        NoodleNode() = default;
        NoodleNode(const std::string& n, ln_Node id) noexcept : kind(n), id(id) {}
        ~NoodleNode() = default;

        ln_Node id;
        bool play_controller = false;
        bool bang_controller = false;
        std::string kind;
        std::vector<ln_Pin> pins;
    };

    // Some nodes may have overridden draw methods, such as the LabSound 
    // AnalyserNode. If such exists, the associated NodeRender will have a 
    // functor to call.
    // overriding by entity allows nodes of the same type to have different
    // draw overrides

    /// @TODO add a render delegate, so that a void* doesn't have to be passed
    /// where a DrawList would go
    struct NodeRender
    {
        // entity, ul_ws, lr_ws, scale, drawlist (typically ImGui::DrawList)
        std::function<void(ln_Node, vec2, vec2, float, void*)> render;
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
        ln_Pin       pin_id = { entt::null, false };
        ln_Node      node_id = { entt::null, false };
        std::string  value_as_string;
        char const* const* names = nullptr; // if an DataType is Enumeration, they'll be here
    };

    // PinEdit provides a mechanism by which a pin can
    // accept a value in the form of a string, or get a value as a string.
    // A custom edit routine could go here as well, as for the renderer.
    /// @TODO hook them up
    struct PinEdit
    {
        std::function<void(ln_Pin, std::string)> set;
        std::function<std::string(ln_Pin)> get;
    };

    // Schematic nodes may be connected from a pin on one node to a pin on another
    struct Connection
    {
        enum class Kind { ToBus, ToParam };

        Connection() = delete;

        explicit Connection(ln_Connection id, ln_Pin pin_from, ln_Node node_from, ln_Pin pin_to, ln_Node node_to, Kind kind)
            : id(id), pin_from(pin_from), node_from(node_from), pin_to(pin_to), node_to(node_to), kind(kind) {}

        ln_Connection id = { entt::null };
        ln_Pin  pin_from = { entt::null, false };
        ln_Node node_from = { entt::null, false };
        ln_Pin  pin_to = { entt::null, false };
        ln_Node node_to = { entt::null, false };

        Kind kind = Kind::ToBus;
    };

    class Provider
    {
        friend struct Work;
        friend struct ProviderHarness;
        std::map<std::string, ln_Node> _name_to_entity;

    public:

        std::map<ln_Node, NoodleNode, cmp_ln_Node> _noodleNodes;

        virtual ~Provider() = default;
        
        inline ln_Node copy(ln_Node n)
        {
            if (registry().valid(n.id))
                return n;
            return ln_Node{ entt::null, false };
        }

        inline ln_Pin copy(ln_Pin n)
        {
            if (registry().valid(n.id))
                return n;
            return ln_Pin{ entt::null, false };
        }

        virtual entt::registry& registry() const = 0;
        virtual ln_Context create_runtime_context(ln_Node id) = 0;

        void associate(ln_Node node, const std::string& name)
        {
            _name_to_entity[name] = node;
        }
        ln_Node entity_for_node_named(const std::string& name)
        {
            auto it = _name_to_entity.find(name);
            if (it == _name_to_entity.end())
                return ln_Node{ entt::null, false };
            
            bool valid = registry().valid(it->second.id);
            if (!valid)
            {
                _name_to_entity.erase(it);
                return ln_Node{ entt::null, false };
            }
            
            return { it->second.id, true };
        }
        void clear_entity_node_associations()
        {
            _name_to_entity.clear();
        }

        // node creation and deletion
        virtual char const* const* node_names() const = 0;
        virtual ln_Node node_create(const std::string& name, ln_Node id) = 0;
        virtual void node_delete(ln_Node node) = 0;

        // node access
        virtual float node_get_timing(ln_Node node) = 0;
        virtual float node_get_self_timing(ln_Node node) = 0;
        virtual void  node_start_stop(ln_Node node, float when) = 0;
        virtual void  node_bang(ln_Node node) = 0;

        virtual ln_Pin node_input_with_index(ln_Node node, int output) = 0;
        virtual ln_Pin node_output_named(ln_Node node, const std::string& output_name) = 0;
        virtual ln_Pin node_output_with_index(ln_Node node, int output) = 0;
        virtual ln_Pin node_param_named(ln_Node node, const std::string& output_name) = 0;

        // pins
        virtual void  pin_set_param_value(const std::string& node_name, const std::string& param_name, float) = 0;
        virtual void  pin_set_setting_float_value(const std::string& node_name, const std::string& setting_name, float) = 0;
        virtual void  pin_set_float_value(ln_Pin pin, float) = 0;
        virtual float pin_float_value(ln_Pin pin) = 0;
        virtual void  pin_set_setting_int_value(const std::string& node_name, const std::string& setting_name, int) = 0;
        virtual void  pin_set_int_value(ln_Pin pin, int) = 0;
        virtual int   pin_int_value(ln_Pin pin) = 0;
        virtual void  pin_set_setting_bool_value(const std::string& node_name, const std::string& setting_name, bool) = 0;
        virtual void  pin_set_bool_value(ln_Pin pin, bool) = 0;
        virtual bool  pin_bool_value(ln_Pin pin) = 0;
        virtual void  pin_set_setting_bus_value(const std::string& node_name, const std::string& setting_name, const std::string& path) = 0;
        virtual void  pin_set_bus_from_file(ln_Pin pin, const std::string& path) = 0;
        virtual void  pin_set_enumeration_value(ln_Pin pin, const std::string& value) = 0;
        virtual void  pin_set_setting_enumeration_value(const std::string& node_name, const std::string& setting_name, const std::string& value) = 0;

        // string based interfaces
        virtual void pin_create_output(const std::string& node_name, const std::string& output_name, int channel) = 0;

        // connections
        virtual void connect_bus_out_to_bus_in(ln_Node node_out_id, ln_Pin output_pin_id, ln_Node node_in_id) = 0;
        virtual void connect_bus_out_to_param_in(ln_Node output_node_id, ln_Pin output_pin_id, ln_Pin pin_id) = 0;
        virtual void disconnect(ln_Connection connection_id) = 0;
    };

    //--------------------------------------------------------------------------
    // Runtime
    //--------------------------------------------------------------------------


    struct ProviderHarness
    {
        ProviderHarness() = delete;
        explicit ProviderHarness(Provider&);
        ~ProviderHarness();

        Provider& provider;
        bool show_profiler = false;
        bool show_debug = false;
        bool show_demo = false;
        bool show_ids = false;
      
        bool run();

        // save and load do their work irrespective of dirty state.
        // check needs_saving to determine if the user should be presented
        // with a save as dialog, or if save should not be called.
        // the Context is not responsible for the document on disk, only reading and writing,
        // so path is not tracked.
        bool needs_saving() const;
        void save(const std::string& path);
        void load(const std::string& path);
        void export_cpp(const std::string& path);
        void save_test(const std::string& path);
        void save_json(const std::string& path);
        void clear_all();

    private:
        struct State;
        State* _s;
    };

} }  // lab::noodle

#endif
