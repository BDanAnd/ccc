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

void print_instruction(vector<string>& str, instruction* i, bool singleline = false)
{
    switch (i->type) {
        case UNARY:
            print_operand(str, i->result);
            cout << " = ";
            print_operand(str, i->exp.ops[0]);
            break;
        case BINARY:
            print_operand(str, i->result);
            cout << " = ";
            print_operand(str, i->exp.ops[0]);
            cout << " " << operation_type_str[i->exp.type] << " ";
            print_operand(str, i->exp.ops[1]);
            break;
        case COND:
            cout << "ifTrue ";
            print_operand(str, i->exp.ops[0]);
            cout << " " << operation_type_str[i->exp.type] << " ";
            print_operand(str, i->exp.ops[1]);
            if (singleline)
                cout << " goto ...";
            else
                cout << endl << "goto BBx" << endl << "else" << endl << "goto BBx";
            break;
        case UNCOND:
            cout << "goto BBx";
            break;
        case LABEL:
            cout << "BBx:";
            break;
        case RETURN:
            cout << "return ";
            print_operand(str, i->exp.ops[0]);
    }
}

void print_instruction_list(analysis_state& state)
{
    for (auto i : state.instructions_list) {
        print_instruction(state.str, i);
        cout << endl;
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

void delete_ins(analysis_state& state, instruction* ins)
{
    auto itr = state.instructions_list.begin();
    while (*itr != ins) ++itr;
    state.instructions_list.erase(itr);
    itr = ins->owner->ins_list.begin();
    while (*itr != ins) ++itr;
    ins->owner->ins_list.erase(itr);
    delete ins;
}

int merge_bbs(analysis_state& state)
{
    bool change = true;
    while (change) {
        change = false;
        vector<basic_block*> new_bb_list;
        vector<basic_block*> deleted_bbs;
        for (auto bb : state.bb_list) {
            if (get_index(deleted_bbs, bb, false) > -1)
                continue;
            new_bb_list.push_back(bb);
            if (bb == state.entry_bb || bb == state.exit_bb)
                continue;
            while (bb->succ.size() == 1 &&
                   bb->succ[0]->pred.size() == 1 &&
                   bb->succ[0] != state.exit_bb) {
                auto next_bb = bb->succ[0];
                get_index(deleted_bbs, next_bb, true);
                bb->succ = next_bb->succ;
                int ind;
                for (auto succ_bb : bb->succ)
                    while ((ind = get_index(succ_bb->pred, next_bb, false)) > -1)
                        succ_bb->pred[ind] = bb;
                instruction* ins;
                if (!bb->ins_list.empty() &&
                    (ins = *(bb->ins_list.end() - 1))->type == UNCOND)
                    delete_ins(state, ins);
                for (auto ins : next_bb->ins_list) {
                    bb->ins_list.push_back(ins);
                    ins->owner = bb;
                }
                delete next_bb;
                change = true;
            }
        }
        state.bb_list = {};
        for (auto bb : new_bb_list)
            if (get_index(deleted_bbs, bb, false) == -1)
                state.bb_list.push_back(bb);
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

int OPTION_SETS(analysis_state& state, bool b)
{
    map<basic_block*, string> tmp_names_for_bb;
    if (b)
        set_tmp_names_for_bb(state, tmp_names_for_bb);

    // clear information
    state.definitions.clear();
    state.expressions.clear();
    state.du_chains.clear();

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
    int var_count = state.str.size();
    int exp_count = state.expressions.size();
    for (int i = 0; i < exp_count; ++i) {
        var_to_exp_list[state.expressions[i].ops[0].a].push_back(i);
        var_to_exp_list[state.expressions[i].ops[1].a].push_back(i);
    }
    for (auto bb : state.bb_list) {
        bb->gen = bb->kill = bb->in_rd = bb->out_rd = bitvector(def_count);
        bb->c_gen = bb->c_kill = bb->c_in = bb->c_out = bitvector(def_count);
        bb->use = bb->def = bb->in_lv = bb->out_lv = bitvector(var_count);
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
        // calculate use def sets
        for (auto ins : bb->ins_list) {
            if (ins->type == LABEL || ins->type == UNCOND)
                continue;

            if (ins->type == BINARY || ins->type == COND) {
                if (ins->exp.ops[1].type == ARRAY_OPERAND &&
                    ins->exp.ops[1].subtype == VAR_OPERAND &&
                    bb->def[ins->exp.ops[1].b] == false)
                    bb->use[ins->exp.ops[1].b] = true;
                if (ins->exp.ops[1].type != CONST_OPERAND &&
                    bb->def[ins->exp.ops[1].a] == false)
                    bb->use[ins->exp.ops[1].a] = true;
            }

            if (ins->exp.ops[0].type == ARRAY_OPERAND &&
                ins->exp.ops[0].subtype == VAR_OPERAND &&
                bb->def[ins->exp.ops[0].b] == false)
                bb->use[ins->exp.ops[0].b] = true;
            if (ins->exp.ops[0].type != CONST_OPERAND &&
                bb->def[ins->exp.ops[0].a] == false)
                bb->use[ins->exp.ops[0].a] = true;

            if (ins->type == UNARY || ins->type == BINARY) {
                if (ins->result.type == ARRAY_OPERAND &&
                    ins->result.subtype == VAR_OPERAND &&
                    bb->def[ins->result.b] == false)
                    bb->use[ins->result.b] = true;
                if (ins->result.type != CONST_OPERAND)
                    bb->def[ins->result.a] = true;
            }
        }
        // calculate e_gen e_kill sets
        int ind;
        for (auto ins : bb->ins_list)
            switch(ins->type) {
                case BINARY:
                    ind = get_index(state.expressions, ins->exp, false);
                    bb->e_gen[ind] = true;
                    bb->e_kill[ind] = false;
                case UNARY:
                    for (auto ind : var_to_exp_list[ins->result.a]) {
                        bb->e_gen[ind] = false;
                        bb->e_kill[ind] = true;
                    }
            }
        // modify var_to_ins_list before calculate
        var_to_ins_list.clear();
        for (auto ins : state.definitions)
            if (ins->type == UNARY &&
                ins->result.type == VAR_OPERAND &&
                ins->exp.ops[0].type == VAR_OPERAND) {
                var_to_ins_list[ins->result.a].push_back(ins);
                var_to_ins_list[ins->exp.ops[0].a].push_back(ins);
                }
        // calculate c_gen c_kill sets
        for (auto ins : bb->ins_list) {
            if (ins->type != UNARY &&
                ins->type != BINARY)
                continue;
            if (ins->result.type != VAR_OPERAND)
                continue;
            for (auto copy_ins : var_to_ins_list[ins->result.a]) {
                ind = get_index(state.definitions, copy_ins, false);
                bb->c_kill[ind] = true;
                bb->c_gen[ind] = false;
            }
            if (ins->type == BINARY ||
                ins->exp.ops[0].type != VAR_OPERAND)
                continue;
            ind = get_index(state.definitions, ins, false);
            bb->c_kill[ind] = false;
            bb->c_gen[ind] = true;
        }
        // fix c_kill
        for (int i = 0; i < def_count; ++i)
            if (bb->c_kill[i] && state.definitions[i]->owner == bb)
                bb->c_kill[i] = false;
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
        cout << "Use    : ";
        for (int i = 0; i < var_count; ++i)
            if (bb->use[i])
                cout << state.str[i] << " ";
        cout << endl;
        cout << "Def    : ";
        for (int i = 0; i < var_count; ++i)
            if (bb->def[i])
                cout << state.str[i] << " ";
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
        cout << endl;
        cout << "c_Gen  : ";
        for (int i = 0; i < def_count; ++i)
            if (bb->c_gen[i]) {
                cout << "(";
                print_operand(state.str, state.definitions[i]->result);
                cout << " = ";
                print_operand(state.str, state.definitions[i]->exp.ops[0]);
                cout << ", " << BB_NAME(state.definitions[i]->owner) << ") ";
            }
        cout << endl;
        cout << "c_Kill : ";
        for (int i = 0; i < def_count; ++i)
            if (bb->c_kill[i]) {
                cout << "(";
                print_operand(state.str, state.definitions[i]->result);
                cout << " = ";
                print_operand(state.str, state.definitions[i]->exp.ops[0]);
                cout << ", " << BB_NAME(state.definitions[i]->owner) << ") ";
            }
        cout << endl << endl;
    }
    // calculate c_in c_out
    bool change = true;
    while (change) {
        change = false;
        for (auto bb : state.bb_list) {
            if (bb == state.entry_bb)
                continue;
            bb->c_in = bitvector(def_count, !bb->pred.empty());
            for (auto bbp : bb->pred)
                bb->c_in = bb->c_in * bbp->c_out;
            bitvector out_new = bb->c_gen + (bb->c_in - bb->c_kill);
            if (!(out_new == bb->c_out)) {
                bb->c_out = out_new;
                change = true;
            }
        }
    }
    // calculate du_use du_def sets
    for (auto bb : state.bb_list)
        for (auto ins : bb->ins_list)
            switch (ins->type) {
                case UNARY:
                    if (ins->result.type == ARRAY_OPERAND && ins->result.subtype == VAR_OPERAND)
                        get_index(state.du_chains, {ins, ins->result.b}, true);
                case RETURN:
                    if (ins->exp.ops[0].type == VAR_OPERAND)
                        get_index(state.du_chains, {ins, ins->exp.ops[0].a}, true);
                    if (ins->exp.ops[0].type == ARRAY_OPERAND) {
                        get_index(state.du_chains, {ins, ins->exp.ops[0].a}, true);
                        if (ins->exp.ops[0].subtype == VAR_OPERAND)
                            get_index(state.du_chains, {ins, ins->exp.ops[0].b}, true);
                    }
                    break;
                case BINARY:
                case COND:
                    if (ins->exp.ops[0].type == VAR_OPERAND)
                        get_index(state.du_chains, {ins, ins->exp.ops[0].a}, true);
                    if (ins->exp.ops[1].type == VAR_OPERAND)
                        get_index(state.du_chains, {ins, ins->exp.ops[1].a}, true);
            }
    int us = state.du_chains.size();
    for (auto bb : state.bb_list) {
        bb->du_use = bb->du_def = bb->du_in = bb->du_out = bitvector(us);
        if (bb == state.entry_bb|| bb == state.exit_bb)
            continue;
        bitvector var_defs(state.str.size());
        for (auto ins : bb->ins_list) {
            switch (ins->type) {
                case UNARY:
                    if (ins->result.type == ARRAY_OPERAND &&
                        ins->result.subtype == VAR_OPERAND &&
                        var_defs[ins->result.b] == false)
                        bb->du_use[get_index(state.du_chains, {ins, ins->result.b}, false)] = true;
                case RETURN:
                    if (ins->exp.ops[0].type == VAR_OPERAND &&
                        var_defs[ins->exp.ops[0].a] == false)
                        bb->du_use[get_index(state.du_chains, {ins, ins->exp.ops[0].a}, false)] = true;
                    if (ins->exp.ops[0].type == ARRAY_OPERAND) {
                        if (var_defs[ins->exp.ops[0].a] == false)
                            bb->du_use[get_index(state.du_chains, {ins, ins->exp.ops[0].a}, false)] = true;
                        if (ins->exp.ops[0].subtype == VAR_OPERAND &&
                            var_defs[ins->exp.ops[0].b] == false)
                            bb->du_use[get_index(state.du_chains, {ins, ins->exp.ops[0].b}, false)] = true;
                    }
                    break;
                case BINARY:
                case COND:
                    if (ins->exp.ops[0].type == VAR_OPERAND &&
                        var_defs[ins->exp.ops[0].a] == false)
                        bb->du_use[get_index(state.du_chains, {ins, ins->exp.ops[0].a}, false)] = true;
                    if (ins->exp.ops[1].type == VAR_OPERAND &&
                        var_defs[ins->exp.ops[1].a] == false)
                        bb->du_use[get_index(state.du_chains, {ins, ins->exp.ops[1].a}, false)] = true;
            }
            if ((ins->type == UNARY || ins->type == BINARY) && ins->result.type != CONST_OPERAND)
                var_defs[ins->result.a] = true;
        }
        for (auto p : state.du_chains)
            if (var_defs[p.second] == true && p.first->owner != bb)
                bb->du_def[get_index(state.du_chains, p, false)] = true;
    }
    // calculate du_in du_out
    change = true;
    while (change) {
        change = false;
        for (auto bb : state.bb_list) {
            if (bb == state.exit_bb)
                continue;
            bb->du_out = bitvector(us);
            for (auto bbp : bb->succ)
                bb->du_out = bb->du_out + bbp->du_in;
            bitvector in_new = bb->du_use + (bb->du_out - bb->du_def);
            if (!(in_new == bb->du_in)) {
                bb->du_in = in_new;
                change = true;
            }
        }
    }
    if (b) {
        cout << "------" << endl;
        for (auto bb : state.bb_list) {
            cout << BB_NAME(bb) << endl;
            cout << "c_in   : ";
            for (int i = 0; i < def_count; ++i)
                if (bb->c_in[i]) {
                    cout << "(";
                    print_instruction(state.str, state.definitions[i]);
                    cout << ", " << BB_NAME(state.definitions[i]->owner) << ") ";
                }
            cout << endl;
            cout << "c_out  : ";
            for (int i = 0; i < def_count; ++i)
                if (bb->c_out[i]) {
                    cout << "(";
                    print_instruction(state.str, state.definitions[i]);
                    cout << ", " << BB_NAME(state.definitions[i]->owner) << ") ";
                }
            cout << endl;
            cout << "du_use : ";
            for (int i = 0; i < us; ++i)
                if (bb->du_use[i]) {
                    cout << "(";
                    print_instruction(state.str, state.du_chains[i].first, true);
                    cout << " ; " << state.str[state.du_chains[i].second] << ") ";
                }
            cout << endl;
            cout << "du_def : ";
            for (int i = 0; i < us; ++i)
                if (bb->du_def[i]) {
                    cout << "(";
                    print_instruction(state.str, state.du_chains[i].first, true);
                    cout << " ; " << state.str[state.du_chains[i].second] << ") ";
                }
            cout << endl;
            cout << "du_in  : ";
            for (int i = 0; i < us; ++i)
                if (bb->du_in[i]) {
                    cout << "(";
                    print_instruction(state.str, state.du_chains[i].first, true);
                    cout << " ; " << state.str[state.du_chains[i].second] << ") ";
                }
            cout << endl;
            cout << "du_out : ";
            for (int i = 0; i < us; ++i)
                if (bb->du_out[i]) {
                    cout << "(";
                    print_instruction(state.str, state.du_chains[i].first, true);
                    cout << " ; " << state.str[state.du_chains[i].second] << ") ";
                }
            cout << endl << endl;
        }
    }
    return 0;
}

int OPTION_RD(analysis_state& state, bool b)
{
    map<basic_block*, string> tmp_names_for_bb;
    if (b)
        set_tmp_names_for_bb(state, tmp_names_for_bb);

    int def_count = state.definitions.size();
    bool change = true;
    int iter_num = 0;
    while (change) {
        change = false;
        for (auto bb : state.bb_list) {
            if (bb == state.entry_bb)
                continue;
            bb->in_rd = bitvector(def_count);
            for (auto bbp : bb->pred)
                bb->in_rd = bb->in_rd + bbp->out_rd;
            bitvector out_new = bb->gen + (bb->in_rd - bb->kill);
            if (!(out_new == bb->out_rd)) {
                bb->out_rd = out_new;
                change = true;
            }
        }
        if (!b)
            continue;
        cout << endl << "Iter num: " << ++iter_num << endl;
        for (auto bb : state.bb_list) {
            cout << BB_NAME(bb) << ":" << endl;
            cout << "In_Rd  : ";
            for (int i = 0; i < def_count; ++i)
                if (bb->in_rd[i])
                    cout << "(" << state.str[state.definitions[i]->result.a] << ", " <<
                        BB_NAME(state.definitions[i]->owner) << ") ";
            cout << endl;
            cout << "Out_Rd : ";
            for (int i = 0; i < def_count; ++i)
                if (bb->out_rd[i])
                    cout << "(" << state.str[state.definitions[i]->result.a] << ", " <<
                        BB_NAME(state.definitions[i]->owner) << ") ";
            cout << endl;
        }
    }
    return 0;
}

int OPTION_LV(analysis_state& state, bool b)
{
    map<basic_block*, string> tmp_names_for_bb;
    if (b)
        set_tmp_names_for_bb(state, tmp_names_for_bb);

    int var_count = state.str.size();
    bool change = true;
    int iter_num = 0;
    while (change) {
        change = false;
        for (auto bb : state.bb_list) {
            if (bb == state.exit_bb)
                continue;
            bb->out_lv = bitvector(var_count);
            for (auto bbp : bb->succ)
                bb->out_lv = bb->out_lv + bbp->in_lv;
            bitvector in_new = bb->use + (bb->out_lv - bb->def);
            if (!(in_new == bb->in_lv)) {
                bb->in_lv = in_new;
                change = true;
            }
        }
        if (!b)
            continue;
        cout << endl << "Iter num: " << ++iter_num << endl;
        for (auto bb : state.bb_list) {
            cout << BB_NAME(bb) << ":" << endl;
            cout << "In_lv  : ";
            for (int i = 0; i < var_count; ++i)
                if (bb->in_lv[i])
                    cout << state.str[i] << " ";
            cout << endl;
            cout << "Out_lv : ";
            for (int i = 0; i < var_count; ++i)
                if (bb->out_lv[i])
                    cout << state.str[i] << " ";
            cout << endl;
        }
    }
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
            bb->e_in = bitvector(exp_count, !bb->pred.empty());
            for (auto bbp : bb->pred)
                bb->e_in = bb->e_in * bbp->e_out;
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
            bitvector def_vars(state.str.size());
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
                while (get_index(state.str, ss.str(), false) > -1) {
                    ss.clear();
                    ss.str("");
                    ++var_counter;
                    ss << "cse" << var_counter;
                }
                int var_id = get_index(state.str, ss.str(), true);
                for (auto edit_ins : ins_for_edit) {
                    if (edit_ins == ins)
                        continue;

                    // create new instruction
                    instruction* insert_ins = new instruction;
                    insert_ins->owner = edit_ins->owner;
                    insert_ins->type = UNARY;
                    insert_ins->result.type = VAR_OPERAND;
                    insert_ins->result.a = edit_ins->result.a;
                    insert_ins->exp.ops[0].type = VAR_OPERAND;
                    insert_ins->exp.ops[0].a = var_id;

                    // fix old instruction
                    edit_ins->result.a = var_id;

                    // insert new instruction
                    auto itr = state.instructions_list.begin();
                    while (*itr != edit_ins) ++itr;
                    state.instructions_list.insert(++itr, insert_ins);
                    itr = edit_ins->owner->ins_list.begin();
                    while (*itr != edit_ins) ++itr;
                    edit_ins->owner->ins_list.insert(++itr, insert_ins);
                }
                // fix instruction
                ins->type = UNARY;
                ins->exp.ops[0].type = VAR_OPERAND;
                ins->exp.ops[0].a = var_id;
            }
        }
    }
    if (b)
        OPTION_FG(state, true);
    return 0;
}

void replace_var(operand& op, int before, int after, bool change_def = false)
{
    if (change_def && op.type == VAR_OPERAND && op.a == before)
        op.a = after;
    if (op.type == ARRAY_OPERAND) {
        if (change_def && op.a == before)
            op.a = after;
        if (op.subtype == VAR_OPERAND && op.b == before)
            op.b = after;
    }
}

int OPTION_CP(analysis_state& state, bool b)
{
    auto ins_list = state.definitions;
    bool need_delete = false;
    for (auto ins : ins_list) {
        if (need_delete)
            OPTION_SETS(state, false);
        need_delete = false;
        if (get_index(state.definitions, ins, false) == -1)
            continue;
        int us = state.du_chains.size();
        // only "VAR = VAR"
        if (!(ins->type == UNARY &&
              ins->result.type == VAR_OPERAND &&
              ins->exp.ops[0].type == VAR_OPERAND))
            continue;
        vector<instruction*> uses;
        for (int i = 0;  i < us; ++i)
            if (ins->owner->du_out[i] && state.du_chains[i].second == ins->result.a)
                get_index(uses, state.du_chains[i].first, true);
        need_delete = !uses.empty();
        for (auto uses_ins : uses) {
            if (uses_ins->owner->c_in[get_index(state.definitions, ins, false)] == false) {
                need_delete = false;
                break;
            }
            for (auto ins_s : uses_ins->owner->ins_list) {
                if (ins_s == uses_ins)
                    break;
                if ((ins_s->type == UNARY || ins_s->type == BINARY) &&
                    ins_s->result.type == VAR_OPERAND &&
                    (ins_s->result.a == ins->result.a || ins_s->result.a == ins->exp.ops[0].a)) {
                    need_delete = false;
                    break;
                }
            }
            if (!need_delete)
                break;
        }
        if (need_delete) {
            int before = ins->result.a;
            int after = ins->exp.ops[0].a;
            for (auto ins : uses)
                switch(ins->type) {
                    case UNARY:
                        replace_var(ins->result, before, after, false);
                    case RETURN:
                        replace_var(ins->exp.ops[0], before, after, true);
                        break;
                    case BINARY:
                    case COND:
                        /* simplification: in such instructions there can't be ARRAY_OPERAND */
                        replace_var(ins->result, before, after, false);
                        replace_var(ins->exp.ops[0], before, after, true);
                        replace_var(ins->exp.ops[1], before, after, true);
                }
            delete_ins(state, ins);
            state.changed = true;
        }
    }
    if (b)
        OPTION_FG(state, true);
    return 0;
}

int calculate_dominators(analysis_state& state, bool b)
{
    map<basic_block*, string> tmp_names_for_bb;
    if (b)
        set_tmp_names_for_bb(state, tmp_names_for_bb);

    int bb_count = state.bb_list.size();
    for (auto bb : state.bb_list)
        bb->dom = bitvector(bb_count, true);
    state.entry_bb->dom = bitvector(bb_count);
    state.entry_bb->dom[get_index(state.bb_list, state.entry_bb, false)] = true;
    bool change = true;
    int iter_num = 0;
    while (change) {
        change = false;
        for (auto bb : state.bb_list) {
            if (bb == state.entry_bb)
                continue;
            bitvector tmp = bitvector(bb_count, !bb->pred.empty());
            for (auto pred_bb : bb->pred)
                tmp = tmp * pred_bb->dom;
            tmp[get_index(state.bb_list, bb, false)] = true;
            if (!(tmp == bb->dom)) {
                bb->dom = tmp;
                change = true;
            }
        }
        if (!b)
            continue;
        cout << endl << "Iter num: " << ++iter_num << endl;
        for (auto bb : state.bb_list) {
            cout << BB_NAME(bb) << " dom - ";
            for (int i = 0; i < bb_count; ++i)
                if (bb->dom[i])
                    cout << BB_NAME(state.bb_list[i]) << " ";
            cout << endl;
        }
    }
    if (b)
        cout << endl;
    return 0;
}

map<basic_block*, bitvector> get_natural_loops(analysis_state& state, bool b)
{
    map<basic_block*, string> tmp_names_for_bb;
    if (b)
        set_tmp_names_for_bb(state, tmp_names_for_bb);

    int bb_count = state.bb_list.size();
    map<basic_block*, bitvector> natural_loops;
    for (auto bb : state.bb_list)
        for (auto succ_bb : bb->succ) {
            int ind = get_index(state.bb_list, succ_bb, false);
            if (bb->dom[ind]) {
                bitvector loop(bb_count);
                stack<basic_block*> st;
                loop[ind] = true;
                if (loop[ind = get_index(state.bb_list, bb, false)] != true) {
                    loop[ind] = true;
                    st.push(bb);
                }
                while (!st.empty()) {
                    auto bbs = st.top();
                    st.pop();
                    for (auto bbp : bbs->pred)
                        if (loop[ind = get_index(state.bb_list, bbp, false)] != true) {
                            loop[ind] = true;
                            st.push(bbp);
                        }
                }
                if (natural_loops.count(succ_bb))
                    natural_loops[succ_bb] = natural_loops[succ_bb] + loop;
                else
                    natural_loops[succ_bb] = loop;
            }
        }
    if (b)
        for (auto loop: natural_loops) {
            cout << "Header: " << BB_NAME(loop.first) << " Loop: ";
            for (int i = 0; i < bb_count; ++i)
                if (loop.second[i])
                    cout << BB_NAME(state.bb_list[i]) << " ";
            cout << endl;
        }
    return natural_loops;
}

vector<instruction*> search_invariant_calculations(analysis_state& state, bitvector loop, bool b)
{
    map<basic_block*, string> tmp_names_for_bb;
    if (b)
        set_tmp_names_for_bb(state, tmp_names_for_bb);

    map<int, vector<instruction*> > var_to_ins_list;
    for (auto ins : state.instructions_list)
        if (ins->type == UNARY || ins->type == BINARY)
            var_to_ins_list[ins->result.a].push_back(ins);

    vector<instruction*> invariant_ins;
    vector<basic_block*> loop_bb;
    for (int i = 0; i < state.bb_list.size(); ++i)
        if (loop[i])
            loop_bb.push_back(state.bb_list[i]);
    bool change = true;
    while (change) {
        change = false;
        for (auto bb : loop_bb) {
            auto ud_chain = bb->in_rd;
            for (auto ins : bb->ins_list) {
                // only "VAR = VAR x VAR"
                if (ins->type == BINARY) {
                    if (get_index(invariant_ins, ins, false) > -1)
                        continue;
                    bool is_invariant = true;
                    for (int i = 0; i < 2; ++i) {
                        instruction* only_rd = NULL;
                        if (ins->exp.ops[i].type == VAR_OPERAND)
                            for (auto insl : var_to_ins_list[ins->exp.ops[i].a])
                                if (ud_chain[get_index(state.definitions, insl, false)] &&
                                    loop[get_index(state.bb_list, insl->owner, false)])
                                        if (only_rd == NULL &&
                                            get_index(invariant_ins, insl, false) > -1) {
                                            only_rd = insl;
                                        } else {
                                            is_invariant = false;
                                            break;
                                        }
                        if (!is_invariant)
                            break;
                    }
                    if (is_invariant) {
                        change = true;
                        invariant_ins.push_back(ins);
                    }
                }
                // fix ud_chain
                if (ins->type == UNARY || ins->type == BINARY)
                    for (auto insp : var_to_ins_list[ins->result.a])
                        ud_chain[get_index(state.definitions, insp, false)] = (insp == ins);
            }
        }
    }
    if (b)
        for (auto ins : invariant_ins) {
            print_instruction(state.str, ins, true);
            cout << endl;
        }
    return invariant_ins;
}

vector<tuple<instruction*, int, int, int, int> > search_induction_vars(analysis_state& state,
                                                                       bitvector loop,
                                                                       vector<instruction*> invariant_ins,
                                                                       bool b)
{
    map<basic_block*, string> tmp_names_for_bb;
    if (b)
        set_tmp_names_for_bb(state, tmp_names_for_bb);

    vector<basic_block*> loop_bb;
    for (int i = 0; i < state.bb_list.size(); ++i)
        if (loop[i])
            loop_bb.push_back(state.bb_list[i]);

    // parameters: instruction, variable, i, c, d
    // variable -> (i, c, d)
    vector<tuple<instruction*, int, int, int, int> > ind_vars;
    // search basic induction variables
    /* TODO: add use invariant_ins */
    for (auto bb : loop_bb)
        for (auto ins : bb->ins_list)
            // only "VAR = VAR +- CONST"
            if (ins->type == BINARY &&
                (ins->exp.type == PLUS || ins->exp.type == MINUS) &&
                ins->exp.ops[0].type == VAR_OPERAND &&
                ins->result.a == ins->exp.ops[0].a &&
                ins->exp.ops[1].type == CONST_OPERAND)
                get_index(ind_vars, make_tuple(ins, ins->result.a, ins->result.a, 1, 0), true);

    map<int, vector<instruction*> > var_to_ins_list;
    for (auto bb : loop_bb)
        for (auto ins : bb->ins_list)
            if (ins->type == UNARY || ins->type == BINARY)
                var_to_ins_list[ins->result.a].push_back(ins);

    vector<instruction*> ins_list;
    for (auto el : var_to_ins_list) {
        // only one time defined variables
        if (el.second.size() != 1)
            continue;
        auto ins = el.second[0];
        if (ins->type != BINARY)
            continue;
        if (ins->exp.ops[0].type == VAR_OPERAND && ins->exp.ops[1].type == VAR_OPERAND ||
            ins->exp.ops[0].type == CONST_OPERAND && ins->exp.ops[1].type == CONST_OPERAND)
            continue;
        if (ins->exp.type == DIVIDE &&
            ins->exp.ops[0].type == CONST_OPERAND &&
            ins->exp.ops[1].type == VAR_OPERAND)
            continue;
        int var = (ins->exp.ops[0].type == VAR_OPERAND) ? ins->exp.ops[0].a : ins->exp.ops[1].a;
        if (ins->result.a == var)
            continue;
        ins_list.push_back(ins);
    }

    bool change = true;
    while (change) {
        change = false;
        auto ins_it = ins_list.begin();
        while (ins_it != ins_list.end()) {
            int var = ((*ins_it)->exp.ops[0].type == VAR_OPERAND) ?
                       (*ins_it)->exp.ops[0].a :
                       (*ins_it)->exp.ops[1].a;
            auto ind_var = find_if(ind_vars.begin(), ind_vars.end(),
                                   [var](tuple<instruction*, int, int, int, int> a){
                                        return get<1>(a) == var;
                                   });
            if (ind_var == ind_vars.end()) {
                ++ins_it;
                continue;
            }
            int con = ((*ins_it)->exp.ops[0].type == CONST_OPERAND) ?
                       (*ins_it)->exp.ops[0].a :
                       (*ins_it)->exp.ops[1].a;
            int c = get<3>(*ind_var), d = get<4>(*ind_var);
            if (get<2>(*ind_var) != var) { // not basic induction variable
                // calculate ud_chain
                auto ud_chain = (*ins_it)->owner->in_rd;
                auto lst = (*ins_it)->owner->ins_list;
                auto it = lst.begin();
                while (*it != *ins_it) {
                    if ((*it)->type == UNARY || (*it)->type == BINARY)
                        for (auto insp : var_to_ins_list[(*it)->result.a])
                            ud_chain[get_index(state.definitions, insp, false)] = (insp == *it);
                    ++it;
                }

                // check defs j which outside L
                bool failed = false;
                for (int i = 0; i < state.definitions.size(); ++i)
                    if (ud_chain[i] &&
                        state.definitions[i]->result.a == var &&
                        loop[get_index(state.bb_list, state.definitions[i]->owner, false)] == false) {
                        ins_it = ins_list.erase(ins_it);
                        failed = true;
                        break;
                    }
                if (failed)
                    continue;

                //check no def i between def j and def k
                bool simple_check = true;
                if ((*ins_it)->owner == get<0>(*ind_var)->owner) {
                    it = lst.begin();
                    while (*it != *ins_it && *it != get<0>(*ind_var))
                        ++it;
                    simple_check = (*it == get<0>(*ind_var));
                }

                if (simple_check) { // defs j and k in one basic block
                    ++it;
                    while (*it != *ins_it)
                        if (((*it)->type == UNARY || (*it)->type == BINARY) &&
                            (*it)->result.a == get<2>(*ind_var)) {
                            ins_it = ins_list.erase(ins_it);
                            failed = true;
                            break;
                        }
                    if (failed)
                        continue;
                } else {
                    // check that before def k in bb_k no def i
                    it = lst.begin();
                    while (*it != *ins_it)
                        if (((*it)->type == UNARY || (*it)->type == BINARY) &&
                            (*it)->result.a == get<2>(*ind_var)) {
                            ins_it = ins_list.erase(ins_it);
                            failed = true;
                            break;
                        }
                    if (failed)
                        continue;
                    lst = get<0>(*ind_var)->owner->ins_list;
                    it = lst.begin();
                    while (*it != get<0>(*ind_var))
                        ++it;

                    // check that after def j in bb_j no def i
                    while (it != lst.end())
                        if (((*it)->type == UNARY || (*it)->type == BINARY) &&
                            (*it)->result.a == get<2>(*ind_var)) {
                            ins_it = ins_list.erase(ins_it);
                            failed = true;
                            break;
                        }
                    if (failed)
                        continue;

                    // check that no defs i between
                    auto ind_j = get_index(state.definitions, get<0>(*ind_var), false);
                    queue<basic_block*> bb_to_check;
                    for (auto bb : (*ins_it)->owner->pred)
                        if (bb->out_rd[ind_j])
                            if (bb == get<0>(*ind_var)->owner) {
                             bb_to_check = {};
                             break;
                            } else
                                bb_to_check.push(bb);
                    while (!bb_to_check.empty()) {
                        basic_block* bb = bb_to_check.front();
                        bb_to_check.pop();
                        for (int i = 0; i < state.definitions.size(); ++i)
                            if (bb->out_rd[i] &&
                                state.definitions[i]->owner == bb &&
                                state.definitions[i]->result.a == get<2>(*ind_var)) {
                                ins_it = ins_list.erase(ins_it);
                                failed = true;
                                break;
                            }
                        if (failed)
                            break;
                        for (auto bbs : bb->pred)
                            if (bbs->out_rd[ind_j])
                                if (bbs == get<0>(*ind_var)->owner) {
                                    bb_to_check = {};
                                    break;
                            } else
                                bb_to_check.push(bb);

                    }
                    if (failed)
                        continue;
                }

                // fix var
                var = get<2>(*ind_var);
            }
            if ((*ins_it)->exp.type == MULTIPLY)
                get_index(ind_vars, make_tuple(*ins_it, (*ins_it)->result.a, var, c * con, d), true);
            else if ((*ins_it)->exp.type == DIVIDE)
                /* TODO check - maybe some problems */
                get_index(ind_vars, make_tuple(*ins_it, (*ins_it)->result.a, var, c / con, d), true);
            else {// PLUS or MINUS
                int m = ((*ins_it)->exp.type ==PLUS) ? 1 : -1;
                if ((*ins_it)->exp.ops[0].type == VAR_OPERAND)
                    get_index(ind_vars, make_tuple(*ins_it, (*ins_it)->result.a, var, c, d + m * con), true);
                else
                    get_index(ind_vars, make_tuple(*ins_it, (*ins_it)->result.a, var, m * c, d + con), true);
            }
            change = true;
            ins_it = ins_list.erase(ins_it);
        }
    }
    if (b) {
        cout << "Ins in BB - {var -> (i, c, d)}" << endl;
        for (auto var_info : ind_vars) {
            print_instruction(state.str, get<0>(var_info), true);
            cout << " in " << BB_NAME(get<0>(var_info)->owner) <<
                    " - {" << state.str[get<1>(var_info)] <<
                    " -> (" << state.str[get<2>(var_info)] <<
                    ", " << get<3>(var_info) <<
                    ", " << get<4>(var_info) <<
                    ")}" << endl;
        }
    }
    return ind_vars;
}

int add_bb_before(analysis_state& state, basic_block* bb)
{
    basic_block* preh = new basic_block;
    int ind;
    state.bb_list.push_back(preh);
    preh->pred = bb->pred;
    bb->pred = {preh};
    preh->succ = {bb};
    for (auto pred_bb : preh->pred)
        while ((ind = get_index(pred_bb->succ, bb, false)) > -1)
            pred_bb->succ[ind] = preh;
    return 0;
}

int OPTION_SR(analysis_state& state, bool b)
{
    /* TODO fix that calls */
    auto loops = get_natural_loops(state, false);
    // add preheaders
    for (auto loop : loops)
        add_bb_before(state, loop.first);
    OPTION_SETS(state, false);
    OPTION_RD(state, false);
    calculate_dominators(state, false);
    loops = get_natural_loops(state, false);
    vector <pair<basic_block*, bitvector> > loops_vect;
    for_each(loops.begin(), loops.end(),
        [&loops_vect](const pair<basic_block*, bitvector>& p){loops_vect.push_back(p);});
    sort(loops_vect.begin(), loops_vect.end(),
        [](const pair<basic_block*, bitvector>& a, const pair<basic_block*, bitvector>& b) -> bool {
        return (a.second * b.second == a.second);});

    int var_counter = 0;
    for (auto loop : loops_vect) {
        /* TODO fix that calls */
        OPTION_SETS(state, false);
        OPTION_RD(state, false);

        auto invariant_ins = search_invariant_calculations(state, loop.second, false);
        auto induction_vars = search_induction_vars(state, loop.second, invariant_ins, false);

        map<int, pair<vector<instruction*>, map<pair<int, int>, vector<instruction*> > > > conv_ind_vars;
        for (auto ind_var : induction_vars)
            if (get<1>(ind_var) == get<2>(ind_var))
                conv_ind_vars[get<1>(ind_var)].first.push_back(get<0>(ind_var));
            else
                conv_ind_vars[get<2>(ind_var)].second[make_pair(get<3>(ind_var), get<4>(ind_var))].push_back(get<0>(ind_var));

        for (auto i : conv_ind_vars) {
            for (auto j : i.second.second) {
                ++var_counter;
                ostringstream ss;
                ss << "sr" << var_counter;
                while (get_index(state.str, ss.str(), false) > -1) {
                    ss.clear();
                    ss.str("");
                    ++var_counter;
                    ss << "sr" << var_counter;
                }
                int var_id = get_index(state.str, ss.str(), true);
                for (auto ins : j.second) {
                    ins->type = UNARY;
                    ins->exp.ops[0].type = VAR_OPERAND;
                    ins->exp.ops[0].a = var_id;
                }
                for (auto ins : i.second.first) {
                    // create new instruction
                    instruction* insert_ins = new instruction;
                    insert_ins->owner = ins->owner;
                    insert_ins->type = BINARY;
                    insert_ins->result.type = VAR_OPERAND;
                    insert_ins->result.a = var_id;
                    insert_ins->exp.ops[0].type = VAR_OPERAND;
                    insert_ins->exp.ops[0].a = var_id;
                    insert_ins->exp.ops[1].type = CONST_OPERAND;
                    int con = ins->exp.ops[1].a * j.first.first * (ins->exp.type == PLUS ? 1 : -1);
                    insert_ins->exp.type = (con > 0) ? PLUS : MINUS;
                    insert_ins->exp.ops[1].a = abs(con);

                    // insert new instruction
                    auto itr = state.instructions_list.begin();
                    while (*itr != ins) ++itr;
                    state.instructions_list.insert(++itr, insert_ins);
                    itr = ins->owner->ins_list.begin();
                    while (*itr != ins) ++itr;
                    ins->owner->ins_list.insert(++itr, insert_ins);

                    /* TODO add s into conv_ind_vars or not? */
                }

                // after insert preheaders loop.first is preheader
                auto preheader = loop.first;

                // create new instruction
                instruction* insert_ins = new instruction;
                insert_ins->owner = preheader;
                insert_ins->result.type = VAR_OPERAND;
                insert_ins->result.a = var_id;
                int c = j.first.first;
                if (c == 1) {
                    insert_ins->type = UNARY;
                    insert_ins->exp.ops[0].type = VAR_OPERAND;
                    insert_ins->exp.ops[0].a = i.first;
                } else {
                    insert_ins->type = BINARY;
                    insert_ins->exp.type = MULTIPLY;
                    insert_ins->exp.ops[0].type = CONST_OPERAND;
                    insert_ins->exp.ops[0].a = j.first.first;
                    insert_ins->exp.ops[1].type = VAR_OPERAND;
                    insert_ins->exp.ops[1].a = i.first;
                }

                // insert new instruction
                /* TODO insert in right place of instructions_list */
                state.instructions_list.push_back(insert_ins);
                preheader->ins_list.push_back(insert_ins);

                int d = j.first.second;
                if (d) {
                    // create new instruction
                    insert_ins = new instruction;
                    insert_ins->owner = preheader;
                    insert_ins->result.type = VAR_OPERAND;
                    insert_ins->result.a = var_id;
                    insert_ins->type = BINARY;
                    insert_ins->exp.type = (d > 0) ? PLUS : MINUS;
                    insert_ins->exp.ops[0].type = VAR_OPERAND;
                    insert_ins->exp.ops[0].a = var_id;
                    insert_ins->exp.ops[1].type = CONST_OPERAND;
                    insert_ins->exp.ops[1].a = abs(d);

                    // insert new instruction
                    /* TODO insert in right place of instructions_list */
                    state.instructions_list.push_back(insert_ins);
                    preheader->ins_list.push_back(insert_ins);
                }

                state.changed = true;
            }
        }
    }
    merge_bbs(state);
    if (b)
        OPTION_FG(state, true);
    return 0;
}

int OPTION_IVE(analysis_state& state, bool b)
{
    map<basic_block*, string> tmp_names_for_bb;
    if (b)
        set_tmp_names_for_bb(state, tmp_names_for_bb);

    auto loops = get_natural_loops(state, false);
    for (auto loop : loops) {
        auto invariant_ins = search_invariant_calculations(state, loop.second, false);

        /* TODO add realization */

        // state.changed = true;
    }
    return 0;
}

int split_bbs(analysis_state& state)
{
    vector<basic_block*>  new_bb_list;
    for (auto bb : state.bb_list) {
        new_bb_list.push_back(bb);
        if (bb == state.entry_bb || bb == state.exit_bb || bb->ins_list.size() < 2)
            continue;
        auto itr = bb->ins_list.begin();
        auto old_succ = bb->succ;
        bb->succ.clear();
        ++itr; // first ins not modified
        basic_block* old_bb = bb;
        basic_block* new_bb;
        while (itr != bb->ins_list.end()) {
            new_bb = new basic_block;
            new_bb_list.push_back(new_bb);
            new_bb->pred.push_back(old_bb);
            old_bb->succ.push_back(new_bb);
            new_bb->ins_list.push_back(*itr);
            (*itr)->owner = new_bb;
            ++itr;
            old_bb = new_bb;
        }
        bb->ins_list.erase(bb->ins_list.begin() + 1, bb->ins_list.end());
        new_bb->succ = old_succ;
        int ind;
        for (auto succ_bb : new_bb->succ)
            while ((ind = get_index(succ_bb->pred, bb, false)) > -1)
                succ_bb->pred[ind] = new_bb;
    }
    state.bb_list = new_bb_list;
    return 0;
}

int OPTION_TASK1(analysis_state& state, bool b)
{
    state.changed = true;
    while (state.changed) {
        state.changed = false;

        OPTION_SETS(state, false);
        OPTION_AE(state, false);
        OPTION_CSE(state, false);

        split_bbs(state);
        OPTION_SETS(state, false);
        OPTION_CP(state, false);
        merge_bbs(state);

        OPTION_SETS(state, false);
        OPTION_RD(state, false);
        calculate_dominators(state, false);
        OPTION_SR(state, false);

        OPTION_SETS(state, false);
        OPTION_RD(state, false);
        OPTION_LV(state, false);
        calculate_dominators(state, false);
        OPTION_IVE(state, false);
    }
    if (b)
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
                << "\t-h,-help\tShow this help list\n"
                << "\t-u,-usage\tShow a short usage message\n"
                << "\t< <INPUTFILE>\tRead from INPUTFILE\n"
                << "\t> <OUTPUTFILE>\tWrite to OUTPUTFILE\n"
                /* TODO: add dfst */
                << "\t-dfst\t\tUse DFST algorithm for BBs numeration (not work)\n"
                << "\t-ALL\t\tPrint all (union of all the following flags)\n"
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
