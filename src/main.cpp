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
        bb->gen = bb->kill = bitvector(def_count);
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

int OPTION_CP(analysis_state& state, bool b)
{
    // calculate c_in c_out
    int def_count = state.definitions.size();
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
    vector<pair<instruction*, int> > U;
    for (auto bb : state.bb_list)
        for (auto ins : bb->ins_list)
            switch (ins->type) {
                case UNARY:
                    if (ins->result.type == ARRAY_OPERAND && ins->result.subtype == VAR_OPERAND)
                        get_index(U, {ins, ins->result.b}, true);
                case RETURN:
                    if (ins->exp.ops[0].type == VAR_OPERAND)
                        get_index(U, {ins, ins->exp.ops[0].a}, true);
                    if (ins->exp.ops[0].type == ARRAY_OPERAND) {
                        get_index(U, {ins, ins->exp.ops[0].a}, true);
                        if (ins->exp.ops[0].subtype == VAR_OPERAND)
                            get_index(U, {ins, ins->exp.ops[0].b}, true);
                    }
                    break;
                case BINARY:
                case COND:
                    if (ins->exp.ops[0].type == VAR_OPERAND)
                        get_index(U, {ins, ins->exp.ops[0].a}, true);
                    if (ins->exp.ops[1].type == VAR_OPERAND)
                        get_index(U, {ins, ins->exp.ops[1].a}, true);
            }
    int us = U.size();
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
                        bb->du_use[get_index(U, {ins, ins->result.b}, false)] = true;
                case RETURN:
                    if (ins->exp.ops[0].type == VAR_OPERAND &&
                        var_defs[ins->exp.ops[0].a] == false)
                        bb->du_use[get_index(U, {ins, ins->exp.ops[0].a}, false)] = true;
                    if (ins->exp.ops[0].type == ARRAY_OPERAND) {
                        if (var_defs[ins->exp.ops[0].a] == false)
                            bb->du_use[get_index(U, {ins, ins->exp.ops[0].a}, false)] = true;
                        if (ins->exp.ops[0].subtype == VAR_OPERAND &&
                            var_defs[ins->exp.ops[0].b] == false)
                            bb->du_use[get_index(U, {ins, ins->exp.ops[0].b}, false)] = true;
                    }
                    break;
                case BINARY:
                case COND:
                    if (ins->exp.ops[0].type == VAR_OPERAND &&
                        var_defs[ins->exp.ops[0].a] == false)
                        bb->du_use[get_index(U, {ins, ins->exp.ops[0].a}, false)] = true;
                    if (ins->exp.ops[1].type == VAR_OPERAND &&
                        var_defs[ins->exp.ops[1].a] == false)
                        bb->du_use[get_index(U, {ins, ins->exp.ops[1].a}, false)] = true;
            }
            if ((ins->type == UNARY || ins->type == BINARY) && ins->result.type != CONST_OPERAND)
                var_defs[ins->result.a] = true;
        }
        for (auto p : U)
            if (var_defs[p.second] == true && p.first->owner != bb)
                bb->du_def[get_index(U, p, false)] = true;
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
    vector<instruction*> for_delete;
    for (auto ins : state.definitions) {
        // only "VAR = VAR"
        if (!(ins->type == UNARY &&
              ins->result.type == VAR_OPERAND &&
              ins->exp.ops[0].type == VAR_OPERAND))
            continue;
        vector<instruction*> uses;
        for (int i = 0;  i < us; ++i)
            if (ins->owner->du_out[i] && U[i].second == ins->result.a)
                get_index(uses, U[i].first, true);
        bool need_delete = !uses.empty();
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
                        if (ins->result.type == ARRAY_OPERAND &&
                            ins->result.subtype == VAR_OPERAND &&
                            ins->result.b == before)
                            {ins->result.b = after;cout << "fix" << endl;}
                    case RETURN:
                        if (ins->exp.ops[0].type == VAR_OPERAND &&
                            ins->exp.ops[0].a == before)
                            ins->exp.ops[0].a = after;
                        if (ins->exp.ops[0].type == ARRAY_OPERAND) {
                            if (ins->exp.ops[0].a == before)
                                ins->exp.ops[0].a = after;
                            if (ins->exp.ops[0].subtype == VAR_OPERAND &&
                                ins->exp.ops[0].b == before)
                                ins->exp.ops[0].b = after;
                        }
                        break;
                    case BINARY:
                    case COND:
                        if (ins->exp.ops[0].type == VAR_OPERAND &&
                            ins->exp.ops[0].a == before)
                            ins->exp.ops[0].a = after;
                        if (ins->exp.ops[1].type == VAR_OPERAND &&
                            ins->exp.ops[1].a == before)
                            ins->exp.ops[1].a = after;
                }
            get_index(for_delete, ins, true);

            // broke current ins
            ins->result.type = CONST_OPERAND;
            ins->exp.ops[0].type = CONST_OPERAND;
        }
    }
    if (b) {
        map<basic_block*, string> tmp_names_for_bb;
        set_tmp_names_for_bb(state, tmp_names_for_bb);
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
                    print_instruction(state.str, U[i].first, true);
                    cout << " ; " << state.str[U[i].second] << ") ";
                }
            cout << endl;
            cout << "du_def : ";
            for (int i = 0; i < us; ++i)
                if (bb->du_def[i]) {
                    cout << "(";
                    print_instruction(state.str, U[i].first, true);
                    cout << " ; " << state.str[U[i].second] << ") ";
                }
            cout << endl;
            cout << "du_in  : ";
            for (int i = 0; i < us; ++i)
                if (bb->du_in[i]) {
                    cout << "(";
                    print_instruction(state.str, U[i].first, true);
                    cout << " ; " << state.str[U[i].second] << ") ";
                }
            cout << endl;
            cout << "du_out : ";
            for (int i = 0; i < us; ++i)
                if (bb->du_out[i]) {
                    cout << "(";
                    print_instruction(state.str, U[i].first, true);
                    cout << " ; " << state.str[U[i].second] << ") ";
                }
            cout << endl << endl;
        }
    }
    for (auto ins : for_delete) {
        auto itr = state.instructions_list.begin();
        while (*itr != ins) ++itr;
        state.instructions_list.erase(itr);
        itr = ins->owner->ins_list.begin();
        while (*itr != ins) ++itr;
        ins->owner->ins_list.erase(itr);
        delete ins;
    }
    if (b)
        OPTION_FG(state, true);
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

int merge_bbs(analysis_state& state)
{
    vector<basic_block*>  new_bb_list;
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
            for (auto ins : next_bb->ins_list) {
                bb->ins_list.push_back(ins);
                ins->owner = bb;
            }
            delete next_bb;
        }
    }
    state.bb_list = new_bb_list;
    return 0;
}

int OPTION_TASK1(analysis_state& state, bool b)
{
    OPTION_SETS(state, false);
    OPTION_AE(state, false);
    OPTION_CSE(state, false);
    split_bbs(state);
    OPTION_SETS(state, false);
    OPTION_CP(state, false);
    merge_bbs(state);
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
                << "\t-dfst\t\tUse DFST algorithm for BBs numeration (not work)\n" //fixme
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
