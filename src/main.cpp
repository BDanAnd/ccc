#include "types.h"
#include "parser.h"

using namespace std;

void free_memory(analysis_state& state)
{
    for (const auto& p : state.bb_list)
        delete p;
    for (const auto& p : state.instructions_list)
        delete p;
}

int check_circular_dependencies(map<func_ptr, func_props>& m, func_ptr p, set<func_ptr> path)
{
    if (path.find(p) != path.end())
        return 1;
    path.insert(p);
    for (auto& pt : m[p].dependences) {
        if (check_circular_dependencies(m, pt, path))
            return 1;
    }
    return 0;
}

void print_operand(vector<string>& str, operand& op)
{
    switch (op.type) {
        case CONST_OPERAND:
            cout << op.a;
            break;
        case VAR_OPERAND:
            cout << str[op.a];
            break;
        case ARRAY_OPERAND:
            cout << str[op.a] << "[";
            switch (op.subtype) {
                case CONST_OPERAND:
                    cout << op.b << "]";
                    break;
                case VAR_OPERAND:
                    cout << str[op.b] << "]";
            }
    }
}

void print_instruction_list(analysis_state& state)
{
    for (auto i : state.instructions_list) {
        switch (i->type) {
            case UNARY:
                print_operand(state.str, i->ops[0]);
                cout << " = ";
                print_operand(state.str, i->ops[1]);
                cout << endl;
                break;
            case BINARY:
                print_operand(state.str, i->ops[0]);
                cout << " = ";
                print_operand(state.str, i->ops[1]);
                cout << " " << operation_type_str[i->subtype] << " ";
                print_operand(state.str, i->ops[2]);
                cout << endl;
                break;
            case COND:
                cout << "ifTrue ";
                print_operand(state.str, i->ops[1]);
                cout << " " << operation_type_str[i->subtype] << " ";
                print_operand(state.str, i->ops[2]);
                cout << endl << "goto BBx" << endl << "else" << endl << "goto BBx" << endl;
                break;
            case UNCOND:
                cout << "goto BBx" << endl;
                break;
            case LABEL:
                cout << "BBx:" << endl;
                break;
            case RETURN:
                cout << "return ";
                print_operand(state.str, i->ops[1]);
                cout << endl;
        }
    }
}

int OPTION_IR(analysis_state& state, bool)
{
    map<basic_block*, string> tmp_names_for_bb;
    int k = 1;
    for (auto bb : state.bb_list) {
        if (bb->name < 0) {
            ostringstream ss;
            ss << "BB" << k++;
            tmp_names_for_bb[bb] = ss.str();
        }
    }

    for (auto bb : state.bb_list) {
        if (bb == state.entry_bb || bb == state.exit_bb)
            continue;
        cout << ((bb->name < 0) ? tmp_names_for_bb[bb] : state.str[bb->name]) << ":" << endl;
        for (auto i : bb->ins_list) {
            switch (i->type) {
                case UNARY:
                    print_operand(state.str, i->ops[0]);
                    cout << " = ";
                    print_operand(state.str, i->ops[1]);
                    cout << endl;
                    break;
                case BINARY:
                    print_operand(state.str, i->ops[0]);
                    cout << " = ";
                    print_operand(state.str, i->ops[1]);
                    cout << " " << operation_type_str[i->subtype] << " ";
                    print_operand(state.str, i->ops[2]);
                    cout << endl;
                    break;
                case COND:
                    cout << "ifTrue ";
                    print_operand(state.str, i->ops[1]);
                    cout << " " << operation_type_str[i->subtype] << " ";
                    print_operand(state.str, i->ops[2]);
                    cout << endl <<
                    "goto " <<
                    ((bb->succ[0]->name < 0) ? tmp_names_for_bb[bb->succ[0]] : state.str[bb->succ[0]->name])
                    << endl << "else" << endl <<
                    "goto " <<
                    ((bb->succ[1]->name < 0) ? tmp_names_for_bb[bb->succ[1]] : state.str[bb->succ[1]->name])
                    << endl;
                    break;
                case UNCOND:
                    cout << "goto " <<
                    ((bb->succ[0]->name < 0) ? tmp_names_for_bb[bb->succ[0]] : state.str[bb->succ[0]->name])
                    << endl;
                    break;
                case RETURN:
                    cout << "return ";
                    print_operand(state.str, i->ops[1]);
                    cout << endl;
            }
        }
    }
    return 0;
}

int OPTION_G(analysis_state&, bool)
{
    cout << "OPTION_G" << endl;
    return 0;
}

int OPTION_FG(analysis_state&, bool)
{
    cout << "OPTION_FG" << endl;
    return 0;
}

int main(int argc, char* argv[])
{
    analysis_state state;

    //parse args
    int use_dfst = 0, all = 0;
    #define GENERATE_FLAGS
    #include "options-wrapper.h"

    char *input = NULL, *output = NULL;
    for (;;) {
        static struct option longopts[] =
        {
            { "help", no_argument, 0, 'h' },
            { "usage", no_argument, 0, 'u' },
            { "dfst", no_argument, &use_dfst, 1 },
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
                << "\t-h,-help\t\tShow this help list\n"
                << "\t-u,-usage\t\tShow a short usage message\n"
                << "\t< <INPUTFILE>\t\tRead from INPUTFILE\n"
                << "\t> <OUTPUTFILE>\t\tWrite to OUTPUTFILE\n"
                << "\t-dfst\t\t\tUse DFST algorithm for BBs numeration\n"
                << "\t-ALL\t\t\tPrint all (union of all the following flags)\n"
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

    if (parse_input(state)) {
        free_memory(state);
        return 1;
    }

    // information about all functions (opts and helpers)
    map<func_ptr, func_props> all_functions;
    #define GENERATE_FUNCTIONS_PROPS
    #include "options-wrapper.h"

    // check circular dependences
    for (auto& func : all_functions) {
        if (check_circular_dependencies(all_functions, func.first, {})) {
            cerr << "Error: Circular dependency found" << endl;
            free_memory(state);
            return 1;
        }
    }

    #define GENERATE_NECESSARY_FUNCTIONS
    #include "options-wrapper.h"
    queue<func_ptr> necessary_functions;
    for (auto& func : all_functions) {
        if (func.second.used)
            necessary_functions.push(func.first);
    }

    // call functions for the selected opts in the right order
    while (!necessary_functions.empty()) {
        func_ptr cur_func = necessary_functions.front();
        necessary_functions.pop();
        bool can_done = true;
        for (auto& func : all_functions[cur_func].dependences) {
            if (!all_functions[func].used) {
                all_functions[func].used = true;
                necessary_functions.push(func);
            }
            if (!all_functions[func].done)
                can_done = false;
        }
        if (can_done) {
            if (cur_func(state, all_functions[cur_func].need_print)) {
                free_memory(state);
                return 1;
            }
            all_functions[cur_func].done = true;
        } else
            necessary_functions.push(cur_func);
    }

    free_memory(state);
    return 0;
}
