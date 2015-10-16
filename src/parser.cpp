#include "parser.h"

using namespace std;

template<typename T>
int get_index(vector<T> &vec, T s, bool add = false)
{
    auto it = find(vec.begin(), vec.end(), s);
    if (it != vec.end())
        return it - vec.begin();
    else if (add) {
        vec.push_back(s);
        return vec.size() - 1;
    } else
        return -1;
}

int string_to_number(string s)
{
    stringstream ss(s);
    int num;
    ss >> num;
    return num;
}

bool is_number(const string& s)
{
    return !s.empty() && (s.find_first_not_of("0123456789") == string::npos || (s.find_first_not_of("0123456789") == 0 && s[0] == '-'));
}

bool is_array_element(const string& s, string& k1, string& k2)
{
    if (s.empty())
        return false;
    auto a = s.find_first_of("["), b = s.find_last_of("]");
    if (a == string::npos || b == string::npos || a == 0 || b < s.size() - 1)
        return false;
    k1 = s.substr(0, a);
    k2 = s.substr(a + 1, b - (a + 1));
    return true;
}

int parse_operand(vector<string>& str, const string& s, operand& op)
{
    string k1, k2;
    if (is_number(s)) {
        op.type = CONST;
        op.a = string_to_number(s);
    } else if (is_array_element(s, k1, k2)) {
        op.type = ARRAY;
        op.a = get_index(str, k1, true);
        if (is_number(k2)) {
            op.subtype = CONST;
            op.b = string_to_number(s);
        } else {
            op.subtype = VAR;
            op.b = get_index(str, k2, true);
        }
    } else {
        op.type = VAR;
        op.a = get_index(str, s, true);
    }
    return 0;
}

int parse_operation(const string& s, instruction* ins)
{
    for (auto it = operation_type_str.begin() ; it < operation_type_str.end(); ++it) {
        if (*it == s) {
            ins->subtype = static_cast<operation_type>(it - operation_type_str.begin());
            return 0;
        }
    }
    return 1;
}

int parse_input(analysis_state& state)
{
    int mode = 0; // tracking of multiline instructions
    vector<string> label_str;
    vector<int> active_labels_id;
    instruction *ins;
    for (string line; getline(cin, line);) {
        istringstream iss(line);
        vector<string> tokens{istream_iterator<string>{iss}, istream_iterator<string>{}};
        switch (tokens.size()) {
            case 0:
                // empty
                break;
            case 2:
                // else, label
                if (tokens[0].compare("else") == 0) {
                    if (mode != 2) {
                        cerr << tokens.size() << ":'" << line << "' - 'else' unexpected" << endl;
                        return 1;
                    }
                    mode = 3;
                } else { // label
                    if (mode) {
                        cerr << tokens.size() << ":'" << line << "' - 'label' unexpected" << endl;
                        return 1;
                    }
                    ins = new instruction;
                    ins->type = LABEL;
                    ins->label_id[0] = get_index(label_str, tokens[0], true);
                    state.instructions_list.push_back(ins);
                }
                break;
            case 3:
                // goto, return
                if (tokens[0].compare("return") == 0) {
                    if (mode) {
                        cerr << tokens.size() << ":'" << line << "' - 'return' unexpected" << endl;
                        return 1;
                    }
                    ins = new instruction;
                    ins->type = RETURN;
                    if (parse_operand(state.str, tokens[1], ins->ops[1])) {
                        cerr << tokens.size() << ":'" << line << "' - parse error" << endl;
                        delete ins;
                        return 1;
                    }
                    state.instructions_list.push_back(ins);
                } else if (tokens[0].compare("goto") == 0) {
                    switch (mode) {
                        case 0:
                            ins = new instruction;
                            ins->type = UNCOND;
                            ins->label_id[0] = get_index(label_str, tokens[1], true);
                            get_index(active_labels_id, ins->label_id[0], true);
                            state.instructions_list.push_back(ins);
                            break;
                        case 1:
                            ins->label_id[0] = get_index(label_str, tokens[1], true); // edit last ifTrue
                            get_index(active_labels_id, ins->label_id[0], true);
                            mode = 2;
                            break;
                        case 2:
                            cerr << tokens.size() << ":'" << line << "' - 'goto' unexpected" << endl;
                            return 1;
                        case 3:
                            ins->label_id[1] = get_index(label_str, tokens[1], true); // edit last ifTrue
                            get_index(active_labels_id, ins->label_id[1], true);
                            mode = 0;
                    }
                } else {
                    cerr << tokens.size() << ":'" << line << "' - wrong" << endl;
                    return 1;
                }
                break;
            case 4:
                // unary
                if (mode) {
                    cerr << tokens.size() << ":'" << line << "' - 'unary' unexpected" << endl;
                    return 1;
                }
                ins = new instruction;
                ins->type = UNARY;
                if (parse_operand(state.str, tokens[0], ins->ops[0]) ||
                    parse_operand(state.str, tokens[2], ins->ops[1])) {
                    cerr << tokens.size() << ":'" << line << "' - parse error" << endl;
                    delete ins;
                    return 1;
                }
                state.instructions_list.push_back(ins);
                break;
            case 5:
                // ifTrue
                if (mode) {
                    cerr << tokens.size() << ":'" << line << "' - 'ifTrue' unexpected" << endl;
                    return 1;
                }
                ins = new instruction;
                ins->type = COND;
                if (parse_operand(state.str, tokens[1], ins->ops[1]) ||
                    parse_operand(state.str, tokens[3], ins->ops[2]) ||
                    parse_operation(tokens[2], ins)) {
                    cerr << tokens.size() << ":'" << line << "' - parse error" << endl;
                    delete ins;
                    return 1;
                }
                state.instructions_list.push_back(ins);
                mode = 1;
                break;
            case 6:
                // binary
                if (mode) {
                    cerr << tokens.size() << ":'" << line << "' - 'binary' unexpected" << endl;
                    return 1;
                }
                ins = new instruction;
                ins->type = BINARY;
                if (parse_operand(state.str, tokens[0], ins->ops[0]) ||
                    parse_operand(state.str, tokens[3], ins->ops[1]) ||
                    parse_operand(state.str, tokens[4], ins->ops[2]) ||
                    parse_operation(tokens[2], ins)) {
                    cerr << tokens.size() << ":'" << line << "' - parse error" << endl;
                    delete ins;
                    return 1;
                }
                state.instructions_list.push_back(ins);
                break;
            default:
                // some error
                cerr << tokens.size() << ":'" << line << "'" << endl;
                return 1;
        }
    }
    if (state.instructions_list.size() == 0) {
        cerr << "Error: empty intermediate representation" << endl;
        return 1;
    }
    if (mode) {
        cerr << "Error: incomplete code" << endl;
        return 1;
    }

    // create bbs
    state.entry_bb = new basic_block;
    state.entry_bb->name = get_index(state.str, string("Entry"), true);
    state.bb_list.push_back(state.entry_bb);
    state.exit_bb = new basic_block;
    state.exit_bb->name = get_index(state.str, string("Exit"), true);
    state.bb_list.push_back(state.exit_bb);

    // partition into bbs
    basic_block* cur_bb = new basic_block, tmp_bb;
    map<int, basic_block*> labels_id_to_bb;
	int ind;
    bool need_delete_cur_bb = true;
    for (const auto ins : state.instructions_list) {
        switch (ins->type) {
            case UNARY:
            case BINARY:
                cur_bb->ins_list.push_back(ins);
                need_delete_cur_bb = false;
                break;
            case COND:
            case UNCOND:
            case RETURN:
                cur_bb->ins_list.push_back(ins);
                state.bb_list.push_back(cur_bb);
                cur_bb = new basic_block;
                need_delete_cur_bb = true;
                break;
            case LABEL:
                if ((ind = get_index(active_labels_id, ins->label_id[0], false)) > -1) {
                    if (!need_delete_cur_bb) {
						cur_bb->label_id = active_labels_id[ind];
                        state.bb_list.push_back(cur_bb);
                        cur_bb = new basic_block;
                    }
                    need_delete_cur_bb = false;
					labels_id_to_bb[active_labels_id[ind]] = cur_bb;
                }
        }
    }
    if (need_delete_cur_bb) {
        delete cur_bb;
    } else {
        cur_bb->succ.push_back(state.exit_bb); // last bb -> not complete -> fallthrough
        state.bb_list.push_back(cur_bb);
    }
    if (state.bb_list.size() > 2) {
        state.entry_bb->succ.push_back(state.bb_list[2]); // fisrt real bb
    } else {
        cerr << "Error: empty list of basic blocks" << endl;
        return 1;
    }

    // fill succ sets
    for (auto bb : state.bb_list) {
        int count = bb->ins_list.size();
        if (!count) {
            if (bb->label_id > -1)
                bb->succ.push_back(labels_id_to_bb[bb->label_id]);
        } else {
            switch (bb->ins_list[count - 1]->type) {
                case UNARY:
                case BINARY:
                    if (bb->label_id > -1)
                        bb->succ.push_back(labels_id_to_bb[bb->label_id]);
                    break;
                case UNCOND:
                    bb->succ.push_back(labels_id_to_bb[bb->ins_list[count - 1]->label_id[0]]);
                    break;
                case COND:
                    bb->succ.push_back(labels_id_to_bb[bb->ins_list[count - 1]->label_id[0]]);
                    bb->succ.push_back(labels_id_to_bb[bb->ins_list[count - 1]->label_id[1]]);
                    break;
                case RETURN:
                    bb->succ.push_back(state.exit_bb);
            }
        }
    }

    // fill pred sets
    for (const auto& v: state.bb_list)
        for (const auto& p : v->succ)
            p->pred.push_back(v);
    return 0;
}
