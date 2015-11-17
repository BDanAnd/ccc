#include "optimizations.h"

using namespace std;

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
    return 0;
}

int OPTION_CP(analysis_state& state, bool b)
{
    /* TODO fix that calls */
    split_bbs(state);
    OPTION_SETS(state, false);

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

    /* TODO fix that call */
    merge_bbs(state);

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
    OPTION_CD(state, false);

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
    merge_bbs(state); // merge preheaders
    return 0;
}

int OPTION_IVE(analysis_state& state, bool b)
{
    auto loops = get_natural_loops(state, false);
    for (auto loop : loops) {
        auto invariant_ins = search_invariant_calculations(state, loop.second, false);

        /* TODO add realization */

        // state.changed = true;
    }
    return 0;
}
