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
    for (auto& pt : m[p].dependences)
        if (check_circular_dependencies(m, pt, path))
            return 1;
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

void print_expression(vector<string>& str, expression& exp)
{
    print_operand(str, exp.ops[0]);
    cout << " " << operation_type_str[exp.type] << " ";
    print_operand(str, exp.ops[1]);
}

void print_instruction_list(analysis_state& state)
{
    for (auto i : state.instructions_list) {
        switch (i->type) {
            case UNARY:
                print_operand(state.str, i->result);
                cout << " = ";
                print_operand(state.str, i->exp.ops[0]);
                cout << endl;
                break;
            case BINARY:
                print_operand(state.str, i->result);
                cout << " = ";
                print_operand(state.str, i->exp.ops[0]);
                cout << " " << operation_type_str[i->exp.type] << " ";
                print_operand(state.str, i->exp.ops[1]);
                cout << endl;
                break;
            case COND:
                cout << "ifTrue ";
                print_operand(state.str, i->exp.ops[0]);
                cout << " " << operation_type_str[i->exp.type] << " ";
                print_operand(state.str, i->exp.ops[1]);
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
                print_operand(state.str, i->exp.ops[0]);
                cout << endl;
        }
    }
}

void set_tmp_names_for_bb(analysis_state& state, map<basic_block*, string>& tmp)
{
    tmp[state.entry_bb] = string("Entry");
    tmp[state.exit_bb] = string("Exit");
    int k = 1;
    for (auto bb : state.bb_list)
        if (bb != state.entry_bb && bb != state.exit_bb) {
            ostringstream ss;
            ss << "BB" << k++;
            tmp[bb] = ss.str();
        }
}

int OPTION_SETS(analysis_state& state, bool b)
{
    map<basic_block*, string> tmp_names_for_bb;
    if (b)
        set_tmp_names_for_bb(state, tmp_names_for_bb);

    // calculate definitions and expressions
    map<int, vector<instruction*> > var_to_ins_list;
    map<int, vector<int> > var_to_exp_list;
    for (auto ins : state.instructions_list)
        if (ins->type == UNARY || ins->type == BINARY) {
            get_index(state.definitions, ins, true);
            var_to_ins_list[ins->result.a].push_back(ins);
            if (ins->type == BINARY)
                get_index(state.expressions, ins->exp, true);
        }

    int def_count = state.definitions.size();
    int exp_count = state.expressions.size();
    for (int i = 0; i < exp_count; ++i) {
        var_to_exp_list[state.expressions[i].ops[0].a].push_back(i);
        var_to_exp_list[state.expressions[i].ops[1].a].push_back(i);
    }
    for (auto bb : state.bb_list) {
        bb->gen = bb->kill = bitvector(def_count);
        bb->e_gen = bb->e_kill = bb->e_in = bb->e_out = bitvector(exp_count);
        if (bb == state.entry_bb|| bb == state.exit_bb)
            continue;
        // calculate gen kill sets
        for (auto ins_it = bb->ins_list.rbegin(); ins_it != bb->ins_list.rend(); ++ins_it) {
            if ((*ins_it)->type != UNARY && (*ins_it)->type != BINARY)
                continue;
            bitvector tmp_gen(def_count);
            tmp_gen[get_index(state.definitions, *ins_it, false)] = true;
            bb->gen = bb->gen + (tmp_gen - bb->kill);
            for (auto ins : var_to_ins_list[(*ins_it)->result.a]) {
                if (ins == *ins_it)
                    continue;
                bb->kill[get_index(state.definitions, ins, false)] = true;
            }
        }
        // calculate e_gen e_kill sets
        for (auto ins : bb->ins_list)
            switch(ins->type) {
                case BINARY:
                    bb->e_gen[get_index(state.expressions, ins->exp, false)] = true;
                case UNARY:
                    for (auto ind : var_to_exp_list[ins->result.a]) {
                        bb->e_gen[ind] = false;
                        bb->e_kill[ind] = true;
                    }
            }
        if (!b)
            continue;
        cout << BB_NAME(bb) << ":" << endl;
        cout << "Gen    : ";
        for (int i = 0; i < def_count; ++i)
            if (bb->gen[i])
                cout << "(" << state.str[state.definitions[i]->result.a] << ", " <<
                    BB_NAME(state.definitions[i]->owner) << ") ";
        cout << endl;
        cout << "Kill   : ";
        for (int i = 0; i < def_count; ++i)
            if (bb->kill[i])
                cout << "(" << state.str[state.definitions[i]->result.a] << ", " <<
                    BB_NAME(state.definitions[i]->owner) << ") ";
        cout << endl;
        cout << "e_Gen  : ";
        for (int i = 0; i < exp_count; ++i)
            if (bb->e_gen[i]) {
                cout << "(";
                print_expression(state.str, state.expressions[i]);
                cout << ") ";
            }
        cout << endl;
        cout << "e_Kill : ";
        for (int i = 0; i < exp_count; ++i)
            if (bb->e_kill[i]) {
                cout << "(";
                print_expression(state.str, state.expressions[i]);
                cout << ") ";
            }
        cout << endl << endl;
    }
    return 0;
}

int OPTION_IR(analysis_state& state, bool b)
{
    if (!b)
        return 0;

    map<basic_block*, string> tmp_names_for_bb;
    set_tmp_names_for_bb(state, tmp_names_for_bb);

    for (auto bb : state.bb_list) {
        if (bb == state.entry_bb || bb == state.exit_bb)
            continue;
        cout << BB_NAME(bb) << ":" << endl;
        for (auto i : bb->ins_list) {
            switch (i->type) {
                case UNARY:
                    print_operand(state.str, i->result);
                    cout << " = ";
                    print_operand(state.str, i->exp.ops[0]);
                    cout << endl;
                    break;
                case BINARY:
                    print_operand(state.str, i->result);
                    cout << " = ";
                    print_operand(state.str, i->exp.ops[0]);
                    cout << " " << operation_type_str[i->exp.type] << " ";
                    print_operand(state.str, i->exp.ops[1]);
                    cout << endl;
                    break;
                case COND:
                    cout << "ifTrue ";
                    print_operand(state.str, i->exp.ops[0]);
                    cout << " " << operation_type_str[i->exp.type] << " ";
                    print_operand(state.str, i->exp.ops[1]);
                    cout << endl << "goto " << BB_NAME(bb->succ[0]) << endl
                        << "else" << endl << "goto " << BB_NAME(bb->succ[1]) << endl;
                    break;
                case UNCOND:
                    cout << "goto " << BB_NAME(bb->succ[0]) << endl;
                    break;
                case RETURN:
                    cout << "return ";
                    print_operand(state.str, i->exp.ops[0]);
                    cout << endl;
            }
        }
    }
    return 0;
}

int OPTION_G(analysis_state& state, bool b)
{
    if (!b)
        return 0;

    map<basic_block*, string> tmp_names_for_bb;
    set_tmp_names_for_bb(state, tmp_names_for_bb);

    cout << "digraph G {" << endl;
    for (auto bb : state.bb_list)
        for (auto bs : bb->succ)
            cout << "    " << BB_NAME(bb) << " -> " << BB_NAME(bs) << ";" << endl;
    cout << "}" << endl;
    return 0;
}

int OPTION_FG(analysis_state& state, bool b)
{
    if (!b)
        return 0;

    map<basic_block*, string> tmp_names_for_bb;
    set_tmp_names_for_bb(state, tmp_names_for_bb);

    cout << "digraph G {" << endl << "    node [shape = rectangle]" << endl;
    for (auto bb : state.bb_list) {
        cout << "    " << (unsigned long) bb << " [label=\"";
        cout << BB_NAME(bb) << "\\n";
        for (auto i : bb->ins_list) {
            switch (i->type) {
                case UNARY:
                    print_operand(state.str, i->result);
                    cout << " = ";
                    print_operand(state.str, i->exp.ops[0]);
                    cout << "\\n";
                    break;
                case BINARY:
                    print_operand(state.str, i->result);
                    cout << " = ";
                    print_operand(state.str, i->exp.ops[0]);
                    cout << " " << operation_type_str[i->exp.type] << " ";
                    print_operand(state.str, i->exp.ops[1]);
                    cout << "\\n";
                    break;
                case COND:
                    cout << "ifTrue ";
                    print_operand(state.str, i->exp.ops[0]);
                    cout << " " << operation_type_str[i->exp.type] << " ";
                    print_operand(state.str, i->exp.ops[1]);
                    cout << "\\ngoto " << BB_NAME(bb->succ[0]) <<
                        "\\nelse\\ngoto " << BB_NAME(bb->succ[1]) << "\\n";
                    break;
                case UNCOND:
                    cout << "goto " << BB_NAME(bb->succ[0]) << "\\n";
                    break;
                case RETURN:
                    cout << "return ";
                    print_operand(state.str, i->exp.ops[0]);
                    cout << "\\n";
            }
        }
        cout << "\"];" << endl;
    }
    for (auto bb : state.bb_list)
        for (auto bs : bb->succ)
            cout << "    " << (unsigned long) bb << " -> " << (unsigned long) bs <<
                ((bb == bs) ? " [dir=back];" : ";") << endl;
    cout << "}" << endl;
    return 0;
}

int OPTION_AE(analysis_state& state, bool b)
{
    map<basic_block*, string> tmp_names_for_bb;
    if (b)
        set_tmp_names_for_bb(state, tmp_names_for_bb);

    int exp_count = state.expressions.size();
    for (auto bb : state.bb_list) {
        if (bb == state.entry_bb)
            continue;
        bb->e_out = bitvector(exp_count, true) - bb->e_kill;
    }
    bool change = true;
    int iter_num = 0;
    while (change) {
        change = false;
        for (auto bb : state.bb_list) {
            if (bb == state.entry_bb)
                continue;
            bb->e_in = bitvector(exp_count, true);
            for (auto bbp : bb->pred)
                bb->e_in = bb->e_in * bbp->e_out;
            if (bb->pred.empty())
                bb->e_in = bitvector(exp_count, false);
            bitvector out_new = bb->e_gen + (bb->e_in - bb->e_kill);
            if (!(out_new == bb->e_out)) {
                bb->e_out = out_new;
                change = true;
            }
        }
        if (!b)
            continue;
        cout << endl << "Iter num: " << ++iter_num << endl;
        for (auto bb : state.bb_list) {
            cout << BB_NAME(bb) << ":" << endl;
            cout << "e_In  : ";
            for (int i = 0; i < exp_count; ++i)
                if (bb->e_in[i]) {
                    cout << "(";
                    print_expression(state.str, state.expressions[i]);
                    cout << ") ";
                }
            cout << endl;
            cout << "e_Out : ";
            for (int i = 0; i < exp_count; ++i)
                if (bb->e_out[i]) {
                    cout << "(";
                    print_expression(state.str, state.expressions[i]);
                    cout << ") ";
                }
            cout << endl;
        }
    }
    return 0;
}

int OPTION_CSE(analysis_state& state, bool b)
{
    bool change = true;
    int var_counter = 0;
    while (change) {
        change = false;
        for (auto bb : state.bb_list) {
            bitvector def_vars(state.str.size(), 0);
            for (auto ins : bb->ins_list) {
                if (ins->type == UNARY) {
                    def_vars[ins->result.a] = true;
                    continue;
                }
                if (ins->type != BINARY)
                    continue;
                if (ins->exp.ops[0].type == VAR_OPERAND && def_vars[ins->exp.ops[0].a])
                    continue;
                if (ins->exp.ops[1].type == VAR_OPERAND && def_vars[ins->exp.ops[1].a])
                    continue;
                if (bb->e_in[get_index(state.expressions, ins->exp, false)] == false)
                    continue;
                change = true;
                queue<basic_block*> tmp_bb_list;
                for (auto bbt : bb->pred)
                    tmp_bb_list.push(bbt);
                vector<basic_block*> seen_bb;
                vector<instruction*> ins_for_edit;
                while (!tmp_bb_list.empty()) {
                    basic_block* cur_bb = tmp_bb_list.front();
                    tmp_bb_list.pop();
                    if (get_index(seen_bb, cur_bb, false) > -1)
                        continue;
                    get_index(seen_bb, cur_bb, true);

                    bool found = false;
                    for (auto ins_it = cur_bb->ins_list.rbegin(); ins_it != cur_bb->ins_list.rend(); ++ins_it)
                        if ((*ins_it)->type == BINARY && (*ins_it)->exp == ins->exp) {
                            ins_for_edit.push_back(*ins_it);
                            found = true;
                            break;
                        }
                    if (!found)
                        for (auto bbt : cur_bb->pred)
                            tmp_bb_list.push(bbt);
                }
                assert(!ins_for_edit.empty());
                ++var_counter;
                ostringstream ss;
                ss << "cse" << var_counter;
                int var_id = get_index(state.str, ss.str(), true);
                for (auto ins : ins_for_edit) {
                    // create new instruction
                    instruction* insert_ins = new instruction;
                    insert_ins->owner = ins->owner;
                    insert_ins->type = UNARY;
                    insert_ins->result.type = VAR_OPERAND;
                    insert_ins->result.a = ins->result.a;
                    insert_ins->exp.ops[0].type = VAR_OPERAND;
                    insert_ins->exp.ops[0].a = var_id;

                    // fix old instruction
                    ins->result.a = var_id;

                    // insert new instruction
                    auto itr = state.instructions_list.begin();
                    while (*itr != ins) ++itr;
                    state.instructions_list.insert(++itr, insert_ins);
                    itr = ins->owner->ins_list.begin();
                    while (*itr != ins) ++itr;
                    ins->owner->ins_list.insert(++itr, insert_ins);
                }
                ins->type = UNARY;
                ins->exp.ops[0].type = VAR_OPERAND;
                ins->exp.ops[0].a = var_id;
            }
        }
    }
    OPTION_FG(state, true);
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
    for (auto& func : all_functions)
        if (check_circular_dependencies(all_functions, func.first, {})) {
            cerr << "Error: Circular dependency found" << endl;
            free_memory(state);
            return 1;
        }

    #define GENERATE_NECESSARY_FUNCTIONS
    #include "options-wrapper.h"
    queue<func_ptr> necessary_functions;
    for (auto& func : all_functions)
        if (func.second.used)
            necessary_functions.push(func.first);

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
            cout << "------OPT \"" << all_functions[cur_func].name << "\" STARTS" << endl;
            if (cur_func(state, all_functions[cur_func].need_print)) {
                free_memory(state);
                return 1;
            }
            cout << "------OPT \"" << all_functions[cur_func].name << "\" ENDS" << endl;
            all_functions[cur_func].done = true;
        } else
            necessary_functions.push(cur_func);
    }

    free_memory(state);
    return 0;
}
