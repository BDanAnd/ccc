#include "types.h"

#include "parser.h"
#include "helpers.h"
#include "printers.h"
#include "analyzes.h"
#include "optimizations.h"

using namespace std;

int main(int argc, char* argv[])
{
    analysis_state state;

    //parse args
    int use_dfst = 0, all = 0, user_friendly = 0;
    #define GENERATE_FLAGS
    #include "options-wrapper.h"

    char *input = NULL, *output = NULL;
    for (;;) {
        static struct option longopts[] =
        {
            { "help", no_argument, 0, 'h' },
            { "usage", no_argument, 0, 'u' },
            { "dfst", no_argument, &use_dfst, 1 },
            { "friendly", no_argument, &user_friendly, 1 },
            { "ALL", no_argument, &all, 1 },
            #define GENERATE_OPTIONS
            #include "options-wrapper.h"
            { 0, 0, 0, 0 }
        };
        int optidx = 0;
        int c = getopt_long_only(argc, argv, "hu", longopts, &optidx);
        if (c == -1)
            break;
        switch (c) {
            case 0:
                break;
            case 'h':
                cerr << "Usage: " << argv[0] <<
                #define GENERATE_USAGE
                #include "options-wrapper.h"
                << "\n"
                << "Options:\n"
                << "\t-h,-help\tShow this help list\n"
                << "\t-u,-usage\tShow a short usage message\n"
                << "\t< <INPUTFILE>\tRead from INPUTFILE\n"
                << "\t> <OUTPUTFILE>\tWrite to OUTPUTFILE\n"
                /* TODO: add dfst */
                << "\t-dfst\t\tUse DFST algorithm for BBs numeration (not work)\n"
                << "\t-friendly\tEnable user friendly input\n"
                << "\t-ALL\t\tUnion of all the following options\n"
                #define GENERATE_HELP
                #include "options-wrapper.h"
                << endl;
                return 0;
            case 'u':
                cerr << "Usage: " << argv[0] <<
                #define GENERATE_USAGE
                #include "options-wrapper.h"
                << endl;
                return 0;
            case '?':
                cerr << "Try '" << argv[0] << " -help' or '" << argv[0] << " -usage' for more information" << endl;
                return 1;
            default:
                return 1;
        }
    }
    if (!(all
        #define GENERATE_OR_FLAGS
        #include "options-wrapper.h"
        )) {
        cerr << "Error: No any requests (output opts)\nTry '" << argv[0] << " -help' or '" << argv[0] << " -usage' for more information" << endl;
        return 1;
    }

    if (all)
        #define GENERATE_ASSIGN_FLAGS
        #include "options-wrapper.h"
        1;

    if (parse_input(state, user_friendly)) {
        free_memory(state);
        return 1;
    }

    // information about all functions (opts and helpers)
    map<func_ptr, func_props> all_functions;
    #define GENERATE_FUNCTIONS_PROPS
    #include "options-wrapper.h"
    #define GENERATE_NECESSARY_FUNCTIONS
    #include "options-wrapper.h"

    queue<func_ptr> printer_functions,
                    analysis_functions;
    vector<func_ptr> optimization_functions;

    for (auto& func : all_functions) {
        // check circular dependences
        if (check_circular_dependencies(all_functions, func.first, {})) {
            cerr << "Error: Circular dependency found" << endl;
            free_memory(state);
            return 1;
        }
        // fill dependents
        for (auto& depfunc : func.second.dependences)
            all_functions[depfunc].dependents.push_back(func.first);
        // check dependences and invalidated for illegal funcs
        switch (func.second.type) {
            case PRINTER:
                if (func.second.used)
                    printer_functions.push(func.first);
                if (!func.second.dependences.empty() || !func.second.invalidated.empty()) {
                    cerr << "Error: Printer option can't have dependences or invalidate" << endl;
                    free_memory(state);
                    return 1;
                }
                break;
            case ANALYSIS:
                if (func.second.used)
                    analysis_functions.push(func.first);
                for (auto& depfunc : func.second.dependences)
                    if (all_functions[depfunc].type != ANALYSIS) {
                        cerr << "Error: Analysis option may depend only on analysis options" << endl;
                        free_memory(state);
                        return 1;
                    }
                if (!func.second.invalidated.empty()) {
                    cerr << "Error: Analysis option can't invalidate" << endl;
                    free_memory(state);
                    return 1;
                }
                break;
            case OPTIMIZATION:
                if (func.second.used)
                    optimization_functions.push_back(func.first);
                for (auto& depfunc : func.second.dependences)
                    if (all_functions[depfunc].type != ANALYSIS) {
                            cerr << func.second.name << endl;
                        cerr << "Error: Optimization option may depend only on analysis options" << endl;
                        free_memory(state);
                        return 1;
                    }
                for (auto& invfunc : func.second.invalidated)
                    if (all_functions[invfunc].type != ANALYSIS) {
                        cerr << "Error: Optimization option may invalidate only analysis options" << endl;
                        free_memory(state);
                        return 1;
                    }
        }
    }

    if (!optimization_functions.empty())
        cout << "------BEFORE OPTIMIZATION------" << endl;
    if (!printer_functions.empty())
        if (call_functions(state, printer_functions, all_functions)) {
            free_memory(state);
            return 1;
        }
    if (!analysis_functions.empty())
        if (call_functions(state, analysis_functions, all_functions)) {
            free_memory(state);
            return 1;
        }
    if (!optimization_functions.empty()) {
        state.changed = true;
        while (state.changed) {
            state.changed = false;
            for (auto fptr : optimization_functions) {
                queue<func_ptr> funcs;
                for (auto func : all_functions[fptr].dependences)
                    funcs.push(func);
                // call undone dependences
                if (call_functions(state, funcs, all_functions, false)) {
                    free_memory(state);
                    return 1;
                }
                // call optimization
                if (fptr(state, false)) {
                    free_memory(state);
                    return 1;
                }
                // invalidate funcs
                funcs = {};
                for (auto func : all_functions[fptr].invalidated) {
                    all_functions[func].done = false;
                    funcs.push(func);
                }
                while (!funcs.empty()) {
                    func_ptr cur_func = funcs.front();
                    funcs.pop();
                    for (auto func : all_functions[cur_func].dependents) {
                        all_functions[func].done = false;
                        funcs.push(func);
                    }
                }
            }
        }
        cout << "------AFTER OPTIMIZATION------" << endl;
        for (auto& func : all_functions)
            func.second.done = false;
        if (!printer_functions.empty())
            if (call_functions(state, printer_functions, all_functions)) {
                free_memory(state);
                return 1;
            }
        if (!analysis_functions.empty())
            if (call_functions(state, analysis_functions, all_functions)) {
                free_memory(state);
                return 1;
            }
    }

    free_memory(state);
    return 0;
}
