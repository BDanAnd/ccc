#include "helpers.h"

using namespace std;

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

void replace_var(operand& op, int before, int after, bool change_def)
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

int call_functions(analysis_state& state,
                   queue<func_ptr> necessary_functions,
                   map<func_ptr, func_props>& all_functions,
                   bool need_print)
{
    while (!necessary_functions.empty()) {
        func_ptr cur_func = necessary_functions.front();
        necessary_functions.pop();
        if (all_functions[cur_func].done)
            continue;
        bool can_done = true;
        for (auto& func : all_functions[cur_func].dependences)
            if (!all_functions[func].done) {
                necessary_functions.push(func);
                can_done = false;
            }
        if (can_done) {
            bool print = need_print && all_functions[cur_func].used;
            if (print)
                cout << "------OPT \"" << all_functions[cur_func].name << "\" STARTS" << endl;
            if (cur_func(state, print))
                return 1;
            if (print)
                cout << "------OPT \"" << all_functions[cur_func].name << "\" ENDS" << endl;
            all_functions[cur_func].done = true;
        } else
            necessary_functions.push(cur_func);
    }
    return 0;
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

void free_memory(analysis_state& state)
{
    for (const auto& p : state.bb_list)
        delete p;
    for (const auto& p : state.instructions_list)
        delete p;
}
