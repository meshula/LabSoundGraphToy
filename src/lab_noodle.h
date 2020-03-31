
#ifndef included_noodle_h
#define included_noodle_h

#include <string>

namespace lab { namespace noodle {

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
