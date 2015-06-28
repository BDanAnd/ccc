#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <algorithm>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>
#include "bitvector.h"

using namespace std;

enum instype
{
    OP, //unary, binary
    LABEL,
    IF,
    ELSE,
    EXIT_JUMP,
    LABEL_JUMP
};

struct ins
{
    int ins_id;
    string str, ins_label;
    instype type;
    int id, l_id, r_id1, r_id2;
    int bb_id;
    int old_l_id;
};

 /*
instype:   id:
LABEL      label_id
LABEL_JUMP label_id
other      undef (-1)
*/

struct phi
{
    int var_id, old_id;
    vector<int> var_ids;
};

struct bb
{
    int name_id;
    int first_ins, last_ins;
    bitvector gen, kill, use, def, in_rd, out_rd, in_lv, out_lv, dom, df;
    vector<int> pred, succ;
    int idom;
    vector<phi> phi_list;
    vector<int> succdom;
};

vector<ins> ins_list;
vector<string> labels_names;
vector<string> bb_names;
vector<string> var_names;
vector<bb> bbs;
vector<tuple<int, int> > all_def;
int ENTRY_ID, EXIT_ID;

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

template <typename T>
string NumberToString(T Number)
{
    ostringstream ss;
    ss << Number;
    return ss.str();
}

int DFST(vector<bool> &a, vector<int>& b, int num = 1)
{
    for (auto i : b) {
        if (a[i])
            continue;
        bb_names[i] = string("BB") + NumberToString(num);
        a[i] = true;
        num = DFST(a, bbs[i].succ, num + 1);
    }
    return num;
}

void loops_search(int i, bitvector& a)
{
    if (!a[i]) {
        a[i] = true;
        for (auto j : bbs[i].pred)
            loops_search(j, a);
    }
}

class print_var_bb_names
{

private:
    bitvector c_;

public:
    print_var_bb_names(bitvector c) : c_(c) {}

    friend std::ostream& operator<<(std::ostream& os, const print_var_bb_names& mp)
    {
        for (int i = 0; i < all_def.size(); ++i)
            if (mp.c_[i]) {
                auto t = all_def[i];
                os << "(" << var_names[get<1>(t)] << ", " << bb_names[ins_list[get<0>(t)].bb_id] << ") ";
            }
        return os;
    }
};

class print_var_names
{

private:
    bitvector c_;

public:
    print_var_names(bitvector c) : c_(c) {}

    friend std::ostream& operator<<(std::ostream& os, const print_var_names& mp)
    {
        for (int i = 0; i < var_names.size(); ++i)
            if (mp.c_[i]) os << var_names[i] << " ";
        return os;
    }
};

class print_bb_names
{

private:
    bitvector c_;

public:
    print_bb_names(bitvector c) : c_(c) {}

    friend std::ostream& operator<<(std::ostream& os, const print_bb_names& mp)
    {
        for (int i = 0; i < bb_names.size(); ++i)
            if (mp.c_[i]) os << bb_names[i] << " ";
        return os;
    }
};

vector<int> var_counter;
vector<vector<int> > var_stack;

int newname(int id)
{
    int i = var_counter[id];
    var_counter[id] += 1;
    var_stack[id].push_back(get_index(var_names, var_names[id] + string("_") + NumberToString(i), true));
    return var_stack[id].back();
}

void rename(int bb_id)
{
    for (auto &i : bbs[bb_id].phi_list)
        i.var_id = newname(i.old_id);
    if (bb_id != ENTRY_ID && bb_id != EXIT_ID)
        for (int i = bbs[bb_id].first_ins; i <= bbs[bb_id].last_ins; ++i) {
            if (ins_list[i].r_id1 > -1)
                ins_list[i].r_id1 = var_stack[ins_list[i].r_id1].back();
            if (ins_list[i].r_id2 > -1)
                ins_list[i].r_id2 = var_stack[ins_list[i].r_id2].back();
            if (ins_list[i].l_id > -1)
                ins_list[i].l_id = newname(ins_list[i].l_id);
        }
    for (auto i : bbs[bb_id].succ)
        for (auto &j : bbs[i].phi_list)
            j.var_ids.push_back(var_stack[j.old_id].back());
    for (auto i : bbs[bb_id].succdom)
        rename(i);
    for (auto i : bbs[bb_id].phi_list)
        var_stack[i.old_id].pop_back();
    if (bb_id != ENTRY_ID && bb_id != EXIT_ID)
        for (int i = bbs[bb_id].first_ins; i <= bbs[bb_id].last_ins; ++i)
            if (ins_list[i].old_l_id > -1)
                var_stack[ins_list[i].old_l_id].pop_back();
}

int main(int argc, char* argv[])
{
    //parse args
    int use_dfst = 0, all = 0, print_ir = 0,
        print_graph = 0, print_sets = 0, print_serialize = 0,
        print_rd = 0, print_lv = 0, print_io = 0,
        print_dce = 0, print_dc = 0, print_nl = 0,

        print_id = 1, print_df = 1, /*some other flags*/ print_ssa = 1;

    char *input = NULL, *output = NULL;
    for (;;) {
        static struct option longopts[] =
        {
            { "help", no_argument, 0, 'h' },
            { "usage", no_argument, 0, 'u' },
            { "dfst", no_argument, &use_dfst, 1 },
            { "ALL", no_argument, &all, 1 },
            { "IR", no_argument, &print_ir, 1 },
            { "G", no_argument, &print_graph, 1 },
            { "sets", no_argument, &print_sets, 1 },
            { "serialize", no_argument, &print_serialize, 1 },
            { "RD", no_argument, &print_rd, 1 },
            { "LV", no_argument, &print_lv, 1 },
            { "IO", no_argument, &print_io, 1 },
            { "dce", no_argument, &print_dce, 1 },
            { "DC", no_argument, &print_dc, 1 },
            { "NL", no_argument, &print_nl, 1 },
            { 0,0,0,0 }
        };
        int optidx = 0;
        int c = getopt_long_only(argc, argv, "hui:o:", longopts, &optidx);
        if (c == -1)
            break;
#define all_coms " [-i INPUTFILE] [-o OUTPUTFILE] [-h] \\
[-help] [-u] [-usage] [-dfst] [-ALL] [-IR] [-G] [-sets] \\
[-serialize] [-RD] [-LV] [-IO] [-dce] [-DC] [-NL]"
        switch (c) {
            case 0:
                break;
            case 'h':
                cerr << "Usage: " << argv[0] << all_coms << "\n"
                << "Options:\n"
                << "\t-h,-help\t\tShow this help list\n"
                << "\t-u,-usage\t\tShow a short usage message\n"
                << "\t-i <INPUTFILE>\t\tRead from INPUTFILE\n"
                << "\t-o <OUTPUTFILE>\t\tWrite to OUTPUTFILE\n"
                << "\t-dfst\t\t\tUse DFST algorithm for BBs numeration\n"
                << "\t-ALL\t\t\tPrint all (union of all the following flags)\n"
                << "\t-IR\t\t\tPrint IR with BB labels\n"
                << "\t-G\t\t\tPrint digraph for graphviz dot\n"
                << "\t-sets\t\t\tPrint gen, kill, use, def sets for all BBs\n"
                << "\t-serialize\t\tPrint serialized info about BBs\n"
                << "\t-RD\t\t\tPrint reaching definitions analysis\n"
                << "\t-LV\t\t\tPrint live variable analysis\n"
                << "\t-IO\t\t\tPrint Input Output sets for all BBs\n"
                << "\t-dce\t\t\tPrint IR dead code and IR without dead code\n"
                << "\t-DC\t\t\tPrint dominator sets for all BBs\n"
                << "\t-NL\t\t\tPrint natural loops"
                << endl;
                return 0;
            case 'u':
                cerr << "Usage: " << argv[0] << all_coms << endl;
                return 0;
            case 'i':
                if (input)
                    cerr << "Warning: set new input file '" << optarg << "'" << endl;
                input = optarg;
                break;
            case 'o':
                if (output)
                    cerr << "Warning: set new output file '" << optarg << "'" << endl;
                output = optarg;
                break;
            case '?':
                cerr << "Try '" << argv[0] << " -help' or '" << argv[0] << " -usage' for more information" << endl;
                return 1;
            default:
                return 1;
        }
    }
    if (!(all || print_ir || print_graph || print_sets || print_serialize || print_rd || print_lv || print_io || print_dce || print_dc || print_nl)) {
        cerr << "Error: No any requests (output opts)\nTry '" << argv[0] << " -help' or '" << argv[0] << " -usage' for more information" << endl;
        return 1;
    }
    
//DEBUG
all = 0;   
print_id = 0, print_df = 0, print_ssa = 0; 
    
    if (all)
        print_ir = print_graph = print_sets = print_serialize = print_rd = print_lv = print_io = print_dce = print_dc = print_nl = 1;

    //redirect streams
    ifstream in;
    ofstream out;
    if (input) {
        in.open(input);
        cin.rdbuf(in.rdbuf());
    }
    if (output) {
        out.open(output);
        cout.rdbuf(out.rdbuf());
    }

    //add entry and exit bbs
    ENTRY_ID = get_index(bb_names, string("entry"), true);
    EXIT_ID = get_index(bb_names, string("exit"), true);
    bbs.resize(2);
    bbs[0].name_id = ENTRY_ID;
    bbs[1].name_id = EXIT_ID;

    //ins input
    map<int, int> labels_to_ins_id;
    bool errfl = false;
    int cur_ins;
    for (string line; getline(cin, line);) {
        cur_ins = ins_list.size();
        istringstream iss(line);
        vector<string> tokens{istream_iterator<string>{iss}, istream_iterator<string>{}};
        string k1, k2;
        switch (tokens.size()) {
            case 0:
                //empty
                break;
            case 2:
                //else, label or ifTrue without parameters
                if (tokens[0].compare("else") == 0) {
                    //ins[cur_ins - 2] must exist and have IF type
                    if (cur_ins - 2 < 0 || ins_list[cur_ins - 2].type != IF) {
                        cerr << "Error: unexpected command 'else' in a line '" << line << "'" << endl;
                        return 1;
                    }
                    ins_list.push_back({cur_ins, line, tokens[1], ELSE, -1, -1, -1, -1});
                } else if (tokens[0].compare("ifTrue") == 0)
                    ins_list.push_back({cur_ins, line, tokens[1], IF, -1, -1, -1, -1});
                else {
                    int tmp;
                    ins_list.push_back({cur_ins, line, tokens[1], LABEL, tmp = get_index(labels_names, tokens[0], true), -1, -1, -1});
                    labels_to_ins_id[tmp] = cur_ins;
                }
                break;
            case 3:
                //goto, return
                if (tokens[0].compare("goto") == 0)
                    ins_list.push_back({cur_ins, line, tokens[2], LABEL_JUMP, get_index(labels_names, tokens[1], true), -1, -1 , -1});
                else
                    ins_list.push_back({cur_ins, line, tokens[2], EXIT_JUMP, -1, -1,
                                        is_number(tokens[1]) ? -1 : get_index(var_names, tokens[1], true), -1});
                break;
            case 4:
                //unary operation
                if (is_array_element(tokens[0], k1, k2)) //in left part array element k1[k2]
                    ins_list.push_back({cur_ins, line, tokens[3], OP, -1,
                                        is_number(k1) ? -1 : get_index(var_names, k1, true),
                                        is_number(k2) ? -1 : get_index(var_names, k2, true),
                                        is_number(tokens[2]) ? -1 : get_index(var_names, tokens[2], true)});
                else if (is_array_element(tokens[2], k1, k2)) //in right part array element k1[k2]
                    ins_list.push_back({cur_ins, line, tokens[3], OP, -1,
                                        is_number(tokens[0]) ? -1 : get_index(var_names, tokens[0], true),
                                        is_number(k1) ? -1 : get_index(var_names, k1, true),
                                        is_number(k2) ? -1 : get_index(var_names, k2, true)});
                else
                    ins_list.push_back({cur_ins, line, tokens[3], OP, -1,
                                        is_number(tokens[0]) ? -1 : get_index(var_names, tokens[0], true),
                                        is_number(tokens[2]) ? -1 : get_index(var_names, tokens[2], true),
                                        -1});
                break;
            case 5:
                //ifTrue with 2 variables in condition
                ins_list.push_back({cur_ins, line, tokens[4], IF, -1, -1,
                                    is_number(tokens[1]) ? -1 : get_index(var_names, tokens[1], true),
                                    is_number(tokens[3]) ? -1 : get_index(var_names, tokens[3], true)});
                break;
            case 6:
                //binary operation
                ins_list.push_back({cur_ins, line, tokens[5], OP, -1,
                                    is_number(tokens[0]) ? -1 : get_index(var_names, tokens[0], true),
                                    is_number(tokens[3]) ? -1 : get_index(var_names, tokens[3], true),
                                    is_number(tokens[4]) ? -1 : get_index(var_names, tokens[4], true)});
                break;
            default:
                //some error
                cerr << tokens.size() << ":'" << line << "'" << endl;
                errfl = true;
        }
    }
    if (errfl)
        return 1;
    if (ins_list.size() == 0) {
        cerr << "Error: empty intermediate representation" << endl;
        return 1;
    }

    //partition into bbs
    vector<int> leaders;
    bool next_leader = true;
    for (auto i : ins_list) {
        switch (i.type) {
            case OP:
            case IF:
            case LABEL:
                if (next_leader)
                    get_index(leaders, i.ins_id, true);
            case ELSE:
                next_leader = false;
                break;
            case EXIT_JUMP:
                if (next_leader)
                    get_index(leaders, i.ins_id, true);
                next_leader = true;
                break;
            case LABEL_JUMP:
                if (next_leader)
                    get_index(leaders, i.ins_id, true);
                else
                    get_index(leaders, labels_to_ins_id[i.id], true);
                next_leader = true;
        }
    }
    assert(leaders.size() != 0);
    sort(leaders.begin(), leaders.end());
    for (int i = 0; i < leaders.size() - 1; ++i)
        bbs.push_back({i + 2, leaders[i], leaders[i + 1] - 1});
    bbs.push_back({leaders.size() + 1, leaders[leaders.size() - 1], ins_list.size() - 1});
    bbs[ENTRY_ID].succ.push_back(2);//First real bb
    bbs[2].pred.push_back(ENTRY_ID);
    for (auto &i : bbs) {
        if (i.name_id == ENTRY_ID || i.name_id == EXIT_ID)
            continue;
        bool fall_through = false;
        int check_insns[2], check_count = 0;
        //if (goto|return)
        if (i.last_ins - 1 >= i.first_ins && ins_list[i.last_ins - 1].type == IF) {
            check_insns[check_count++] = i.last_ins;
            fall_through = true;
        //if (goto|return) else (goto|return)
        } else if (i.last_ins - 2 >= i.first_ins && ins_list[i.last_ins - 1].type == ELSE) {
            check_insns[check_count++] = i.last_ins;
            check_insns[check_count++] = i.last_ins - 2;
        //(goto|return)
        } else if (ins_list[i.last_ins].type == EXIT_JUMP || ins_list[i.last_ins].type == LABEL_JUMP)
            check_insns[check_count++] = i.last_ins;
        //other
        else
            fall_through = true;
        for (int j = 0; j < check_count; ++j) {
            if (ins_list[check_insns[j]].type == LABEL_JUMP) {
                auto k = lower_bound(leaders.begin(), leaders.end(), labels_to_ins_id[ins_list[check_insns[j]].id]) - leaders.begin();
                i.succ.push_back(k + 2);
                bbs[k + 2].pred.push_back(i.name_id);
            } else if (ins_list[check_insns[j]].type == EXIT_JUMP) {
                i.succ.push_back(EXIT_ID);
                bbs[EXIT_ID].pred.push_back(i.name_id);
            }
        }
        if (fall_through) {
            int tmp = i.name_id == bbs.size() - 1 ? EXIT_ID : i.name_id + 1;
            i.succ.push_back(tmp);
            bbs[tmp].pred.push_back(i.name_id);
        }
    }

    //set BB labels
    if (use_dfst) {
        bb_names.resize(bbs.size());
        vector<bool> visited;
        visited.assign(bbs.size(), false);
        visited[ENTRY_ID] = visited[EXIT_ID] = true;
        DFST(visited, bbs[ENTRY_ID].succ);
    } else
        for (int i = 2; i < bbs.size(); ++i)
            bb_names.push_back(string("BB") + NumberToString(i - 1));

    //set bb_id and old_l_id for each ins
    for (int i = 2; i < bbs.size(); ++i)
        for (int j = bbs[i].first_ins; j <= bbs[i].last_ins; ++j) {
            ins_list[j].bb_id = bbs[i].name_id;
            ins_list[j].old_l_id = ins_list[j].l_id;
        }

    //print IR with BB labels
    if (print_ir)
        for (auto i : bbs) {
            if (i.name_id == ENTRY_ID || i.name_id == EXIT_ID)
                continue;
            cout << bb_names[i.name_id] << endl;
            for (int j = i.first_ins; j <= i.last_ins; ++j)
                cout << ins_list[j].str << endl;
            cout << endl;
        }

    //print digraph for graphviz dot
    if (print_graph) {
        cout << "digraph G {" << endl;
        for (auto &i : bbs)
            for (auto &j : i.succ)
                cout << "	" << bb_names[i.name_id] << " -> " << bb_names[j] << ";" << endl;
        cout << "}" << endl << endl;
    }

    //calculate all definitions vector
    map<int, vector<int> > var_to_ins;
    for (auto i : ins_list) {
        if (i.l_id < 0)
            continue;
        get_index(all_def, make_tuple(i.ins_id, i.l_id), true);
        var_to_ins[i.l_id].push_back(i.ins_id);
    }

    //calculate gen kill use def sets
    int c = all_def.size();
    int t = var_names.size();
    for (auto &i : bbs) {
        i.gen = i.kill = i.in_rd = i.out_rd = bitvector(c);
        i.use = i.def = i.in_lv = i.out_lv = bitvector(t);
        if (i.name_id == ENTRY_ID || i.name_id == EXIT_ID)
            continue;//sets must be init
         //calculate gen kill
        for (auto j = i.last_ins; j >= i.first_ins; --j) {
            if (ins_list[j].l_id < 0) //skip operations without left part
                continue;
            bitvector tmp_gen(c);
            tmp_gen[get_index(all_def, make_tuple(ins_list[j].ins_id, ins_list[j].l_id), false)] = true;
            i.gen = i.gen + (tmp_gen - i.kill);
            for (auto k : var_to_ins[ins_list[j].l_id]) {
                if (k == ins_list[j].ins_id)
                        continue;
                i.kill[get_index(all_def, make_tuple(k, ins_list[j].l_id), false)] = true;
            }
        }
        //calculate use def
        for (auto j = i.first_ins; j <= i.last_ins; ++j) {
            if (ins_list[j].r_id1 > -1 && i.def[ins_list[j].r_id1] == false)
                i.use[ins_list[j].r_id1] = true;
            if (ins_list[j].r_id2 > -1 && i.def[ins_list[j].r_id2] == false)
                i.use[ins_list[j].r_id2] = true;
            if (ins_list[j].l_id > -1)
                i.def[ins_list[j].l_id] = true;
        }
        if (print_sets == 0)
            continue;
        cout << bb_names[i.name_id] << ":" << endl;
        cout << "Gen   : " << print_var_bb_names(i.gen) << endl;
        cout << "Kill  : " << print_var_bb_names(i.kill) << endl;
        cout << "Use   : " << print_var_names(i.use) << endl;
        cout << "Def   : " << print_var_names(i.def) << endl << endl;
    }

    //print information about all bbs
    if (print_serialize) {
        cout << "baseBlocks = [" << endl;
        bool o_tmp1 = false, o_tmp2 = false;
        for (auto i : bbs) {
            if (o_tmp1)
                cout << "," << endl;
            else
                o_tmp1 = true;
            cout << "    {" << endl;
            cout << "        'letter' : '" << bb_names[i.name_id] << "'," << endl;
            cout << "        'pred' : [";
            o_tmp2 = false;
            for (auto j : i.pred) {
                if (o_tmp2)
                    cout << ", ";
                else
                    o_tmp2 = true;
                cout << j;
            }
            cout << "]," << endl;
            cout << "        'succ' : [";
            o_tmp2 = false;
            for (auto j : i.succ) {
                if (o_tmp2)
                    cout << ", ";
                else
                    o_tmp2 = true;
                cout << j;
            }
            cout << "]," << endl;
            cout << "        'assign' : [";
            o_tmp2 = false;
            for (int j = 0; j < var_names.size(); ++j) {
                if (i.def[j] == false)
                    continue;
                if (o_tmp2)
                    cout << ", ";
                else
                    o_tmp2 = true;
                cout << "'" << var_names[j] << "'";
            }
            cout << "]," << endl;
            cout << "        'access' : [";
                    o_tmp2 = false;
            for (int j = 0; j < var_names.size(); ++j) {
                if (i.use[j] == false)
                    continue;
                if (o_tmp2)
                    cout << ", ";
                else
                    o_tmp2 = true;
                cout << "'" << var_names[j] << "'";
            }
            cout << "]" << endl;
            cout << "    }";
        }
        cout << endl<< "]" << endl << endl;
    }

    //rd analysis
    bool change = true;
    int iter_num = 0;
    if (print_rd)
        cout << "RD analysis:" << endl;
    while (change) {
        change = false;
        for (auto &i : bbs) {
            if (i.name_id == ENTRY_ID)
                continue;
            i.in_rd = bitvector(c);
            for (auto j : i.pred)
                i.in_rd = i.in_rd + bbs[j].out_rd;
            bitvector out_new = i.gen + (i.in_rd - i.kill);
            if (!(out_new == i.out_rd)) {
                i.out_rd = out_new;
                change = true;
            }
        }
        if (print_rd == 0)
            continue;
        cout << endl << "Iter num: " << ++iter_num << endl;
        for (auto &i : bbs) {
            cout << bb_names[i.name_id] << ":" << endl;
            cout << "In_rd : " << print_var_bb_names(i.in_rd) << endl;
            cout << "Out_rd: " << print_var_bb_names(i.out_rd) << endl;
        }
    }
    if (print_rd)
        cout << endl;

    //lv analysis
    change = true;
    iter_num = 0;
    if (print_lv)
        cout << "LV analysis:" << endl;
    while (change) {
        change = false;
        for (auto &i : bbs) {
            if (i.name_id == EXIT_ID)
                continue;
            i.out_lv = bitvector(t);
            for (auto j : i.succ)
                i.out_lv = i.out_lv + bbs[j].in_lv;
            bitvector in_new = i.use + (i.out_lv - i.def);
            if (!(in_new == i.in_lv)) {
                i.in_lv = in_new;
                change = true;
            }
        }
        if (print_lv == 0)
            continue;
        cout << endl << "Iter num: " << ++iter_num << endl;
        for (auto &i : bbs) {
            cout << bb_names[i.name_id] << ":" << endl;
            cout << "In_lv : " << print_var_names(i.in_lv) << endl;
            cout << "Out_lv: " << print_var_names(i.out_lv) << endl;
        }
    }
    if (print_lv)
        cout << endl;

    //print Input Output sets
    if (print_io)
        for (auto &i : bbs) {
            cout << bb_names[i.name_id] << ":" << endl;
            cout << "Input : " << print_var_bb_names(i.in_rd) << endl;
            cout << "Output: " << print_var_names(i.out_lv) << endl << endl;
        }

    //simplest dead code elimination - experimental
    bitvector use_ins = bitvector(ins_list.size());
    for (auto i : bbs) {
        if (i.name_id == ENTRY_ID || i.name_id == EXIT_ID)
            continue;
        bitvector tmp = i.out_lv;
        for (auto j = i.last_ins; j >= i.first_ins; --j) {
            if (ins_list[j].type != OP)
                use_ins[j] = true;
            if (ins_list[j].l_id > -1 && tmp[ins_list[j].l_id]) {
                use_ins[j] = true;
                tmp[ins_list[j].l_id] = false;
            }
            if (ins_list[j].r_id1 > -1 && use_ins[j])
                tmp[ins_list[j].r_id1] = true;
            if (ins_list[j].r_id2 > -1 && use_ins[j])
                tmp[ins_list[j].r_id2] = true;
        }
    }
    if (print_dce) {
        cout << "IR dead code:" << endl;
        for (auto i : ins_list)
            if (!use_ins[i.ins_id])
                cout << i.str << endl;
        cout << endl;
        cout << "IR without dead code:" << endl;
        for (auto i : ins_list)
            if (use_ins[i.ins_id])
                cout << i.str << endl;
        cout << endl;
    }

    //calculate dominator sets
    int p = bbs.size();
    (bbs[ENTRY_ID].dom = bitvector(p))[ENTRY_ID] = true;
    for (auto &i : bbs) {
        if (i.name_id == ENTRY_ID)
            continue;
        i.dom = bitvector(p, true);
    }
    change = true;
    iter_num = 0;
    if (print_dc)
        cout << "Dominator computing:" << endl;
    while (change) {
        change = false;
        for (auto &i : bbs) {
            if (i.name_id == ENTRY_ID)
                continue;
            bitvector tmp = bitvector(p, true);
            for (auto j : i.pred)
                tmp = tmp * bbs[j].dom;
            tmp[i.name_id] = true;
            if (!(tmp == i.dom)) {
                i.dom = tmp;
                change = true;
            }
        }
        if (print_dc == 0)
            continue;
        cout << endl << "Iter num: " << ++iter_num << endl;
        for (auto &i : bbs)
            cout << bb_names[i.name_id] << " dom: " << print_bb_names(i.dom) << endl;
    }
    if (print_dc)
        cout << endl;

    //search natural loops
    vector<bitvector> natural_loops;
    for (auto &i : bbs)
        for (auto &j : i.succ)
            if (i.dom[j]) {
                bitvector loop(p, false);
                loop[j] = true;
                loops_search(i.name_id, loop);
                get_index(natural_loops, loop, true);
            }
    if (print_nl) {
        cout << "Natural loops:" << endl;
        for (auto i : natural_loops)
            cout << print_bb_names(i) << endl;
        if (natural_loops.size() == 0)
            cout << "None" << endl;
        cout << endl;
    }

    //calculate immediate dominator
    if (print_id)
        cout << "Immediate dominator computing:" << endl;
    for (auto &i : bbs)
        i.idom = -1;
    for (auto &i : bbs) {
        if (i.name_id == ENTRY_ID)
            continue;
        bitvector tmp = i.dom;
        tmp[i.name_id] = false;
        vector<int> dom = tmp;
        for (auto j : dom)
            if (bbs[j].dom == tmp) {
                i.idom = j;
                break;
            }
        if (print_id)
            cout << bb_names[i.name_id] << " idom: " << bb_names[i.idom] << endl;
    }
    if (print_id)
        cout << endl;

    //set succ for dominator tree
    for (auto i : bbs)
        if (i.idom > -1)
            bbs[i.idom].succdom.push_back(i.name_id);

    //calculate dominance frontier
    for (auto &i : bbs) {
        i.df = bitvector(p);
        if (i.pred.size() < 2)
            continue;
        for (auto p : i.pred) {
            int r = p;
            while (r != i.idom) {
                bbs[r].df[i.name_id] = true;
                r = bbs[r].idom;
            }
        }
    }
    if (print_df) {
        cout << "Dominance frontier sets:" << endl;
        for (auto i : bbs)
            cout << bb_names[i.name_id] << ": " << print_bb_names(i.df) << endl;
        cout << endl;
    }

    //calculate globals, blocks sets
    bitvector globals(t);
    vector<bitvector> blocks;
    blocks.assign(t, bitvector(p));
    for (auto i : bbs) {
        if (i.name_id == ENTRY_ID || i.name_id ==EXIT_ID)
            continue;
        bitvector def_tmp(t);
        for (int j = i.first_ins; j <= i.last_ins; ++j) {
            if (ins_list[j].l_id == -1)
                continue;
            if (ins_list[j].r_id1 > -1 && def_tmp[ins_list[j].r_id1] == false)
                globals[ins_list[j].r_id1] = true;
            if (ins_list[j].r_id2 > -1 && def_tmp[ins_list[j].r_id2] == false)
                globals[ins_list[j].r_id2] = true;
            def_tmp[ins_list[j].l_id] = true;
            blocks[ins_list[j].l_id][i.name_id] = true;
        }
    }

    //insert phi
    for (auto i : (vector<int>)globals) {
        bitvector worklist = blocks[i];
        bitvector used_blocks(p);
        vector<int> tmp = worklist;
        int k = 1;
        while (!tmp.empty()) {
            int b = tmp.front();
            for (auto d : (vector<int>)bbs[b].df) {
                if (used_blocks[d])
                    continue;
                bbs[d].phi_list.push_back({i, i});
                used_blocks[d] = worklist[d] = true;
            }
            worklist[b] = false;
            tmp = worklist;
        }
    }

    //rename vars
    var_counter.assign(t, 0);
    var_stack.resize(t);
    for (auto i : (vector<int>)bbs[ENTRY_ID].out_lv)
        newname(i);
    rename(ENTRY_ID);

    //print semi-pruned SSA form without deadcode
    if (print_ssa)
        for (auto i : bbs) {
            if (i.name_id == ENTRY_ID || i.name_id == EXIT_ID)
                continue;
            cout << bb_names[i.name_id] << endl;
            for (int j = i.first_ins; j <= i.last_ins; ++j)
                if (ins_list[j].type == LABEL)
                    cout << ins_list[j].str << endl;
                else
                    break;
            for (auto j : i.phi_list) {
                    cout << var_names[j.var_id] << " = phi(";
                    int s = j.var_ids.size();
                    for (int k = 0; k < s; ++k) {
                        cout << var_names[j.var_ids[k]];
                        if (k < s - 1)
                            cout << ", ";
                    }
                    cout << ")" << endl;
            }
            for (int j = i.first_ins; j <= i.last_ins; ++j) {
                if (ins_list[j].type == LABEL)
                    continue;
                istringstream iss(ins_list[j].str);
                vector<string> tokens{istream_iterator<string>{iss}, istream_iterator<string>{}};
                string k1, k2;
                switch (ins_list[j].type) {
                    case EXIT_JUMP:
                            cout << tokens[0] << " "
                            << (is_number(tokens[1]) ? tokens[1] : var_names[ins_list[j].r_id1]) << " "
                            << tokens[2] << endl;
                            break;
                    case IF:
                        cout << tokens[0] << " "
                        << (is_number(tokens[1]) ? tokens[1] : var_names[ins_list[j].r_id1]) << " "
                        << tokens[2] << " "
                        << (is_number(tokens[3]) ? tokens[3] : var_names[ins_list[j].r_id2]) << " "
                        << tokens[4] << endl;
                        break;
                    case OP:
                        if (tokens.size() == 4) {//unary
                            if (is_array_element(tokens[0], k1, k2)) //in left part array element k1[k2]
                                cout << (is_number(k1) ? k1 : var_names[ins_list[j].l_id]) << "["
                                << (is_number(k2) ? k2 : var_names[ins_list[j].r_id1]) << "] "
                                << tokens[1] << " "
                                << (is_number(tokens[2]) ? tokens[2] : var_names[ins_list[j].r_id2]) << " "
                                << tokens[3] << endl;
                            else if (is_array_element(tokens[2], k1, k2)) //in right part array element k1[k2]
                                cout << (is_number(tokens[0]) ? tokens[0] : var_names[ins_list[j].l_id]) << " "
                                << tokens[1] << " "
                                << (is_number(k1) ? k1 : var_names[ins_list[j].r_id1]) << "["
                                << (is_number(k2) ? k2 : var_names[ins_list[j].r_id2]) << "] "
                                << tokens[3] << endl;
                            else
                                cout << (is_number(tokens[0]) ? tokens[0] : var_names[ins_list[j].l_id]) << " "
                                << tokens[1] << " "
                                << (is_number(tokens[2]) ? tokens[2] : var_names[ins_list[j].r_id1]) << " "
                                << tokens[3] << endl;
                        } else //binary
                            cout << (is_number(tokens[0]) ? tokens[0] : var_names[ins_list[j].l_id]) << " "
                                << tokens[1] << " " << tokens[2] << " "
                                << (is_number(tokens[3]) ? tokens[3] : var_names[ins_list[j].r_id1]) << " "
                                << (is_number(tokens[4]) ? tokens[4] : var_names[ins_list[j].r_id2]) << " "
                                << tokens[5] << endl;
                        break;
                    default:
                        cout << ins_list[j].str << endl;
                }
            }
            cout << endl;
        }

//TEX format
cout << "\\documentclass{article}\n\\usepackage{amsmath}\n\\usepackage[left=25mm, top=5mm, right=90mm, bottom=5mm, nohead, nofoot]{geometry}\n\\begin{document}\n";
    if (1)
        for (auto i : bbs) {
            if (i.name_id == ENTRY_ID || i.name_id == EXIT_ID)
                continue;
            cout << bb_names[i.name_id] << endl << endl;
            for (int j = i.first_ins; j <= i.last_ins; ++j) {
                istringstream iss(ins_list[j].str);
                vector<string> tokens{istream_iterator<string>{iss}, istream_iterator<string>{}};
                if (ins_list[j].type == LABEL)
                    cout << "\\textbf{" << tokens[0] << ":" << "\\hfill{" << tokens[1] << "}}" << endl << endl;
                else
                    break;
            }
            for (auto j : i.phi_list) {
                    cout << "\\(" << var_names[j.var_id] << " = \\phi(";
                    int s = j.var_ids.size();
                    for (int k = 0; k < s; ++k) {
                        cout << var_names[j.var_ids[k]];
                        if (k < s - 1)
                            cout << ", ";
                    }
                    cout << ")\\)" << endl << endl;
            }
            for (int j = i.first_ins; j <= i.last_ins; ++j) {
                if (ins_list[j].type == LABEL)
                    continue;
                istringstream iss(ins_list[j].str);
                vector<string> tokens{istream_iterator<string>{iss}, istream_iterator<string>{}};
                string k1, k2;
                switch (ins_list[j].type) {
                    case EXIT_JUMP:
                            cout << tokens[0] << " \\("
                            << (is_number(tokens[1]) ? tokens[1] : var_names[ins_list[j].r_id1]) << "\\hfill{"
                            << tokens[2] << "}\\)" << endl << endl;
                            break;
                    case IF:
                        cout << tokens[0] << " \\("
                        << (is_number(tokens[1]) ? tokens[1] : var_names[ins_list[j].r_id1]) << " "
                        << tokens[2] << " "
                        << (is_number(tokens[3]) ? tokens[3] : var_names[ins_list[j].r_id2]) << " "
                        << "\\hfill{" << tokens[4] << "}\\)" << endl << endl;
                        break;
                    case OP:
                        if (tokens.size() == 4) {//unary
                            if (is_array_element(tokens[0], k1, k2)) //in left part array element k1[k2]
                                cout << "\\(" << (is_number(k1) ? k1 : var_names[ins_list[j].l_id]) << "["
                                << (is_number(k2) ? k2 : var_names[ins_list[j].r_id1]) << "] "
                                << tokens[1] << " "
                                << (is_number(tokens[2]) ? tokens[2] : var_names[ins_list[j].r_id2]) << " "
                                << "\\hfill{" << tokens[3] << "}\\)" << endl << endl;
                            else if (is_array_element(tokens[2], k1, k2)) //in right part array element k1[k2]
                                cout << "\\(" << (is_number(tokens[0]) ? tokens[0] : var_names[ins_list[j].l_id]) << " "
                                << tokens[1] << " "
                                << (is_number(k1) ? k1 : var_names[ins_list[j].r_id1]) << "["
                                << (is_number(k2) ? k2 : var_names[ins_list[j].r_id2]) << "] "
                                << "\\hfill{" << tokens[3] << "}\\)" << endl << endl;
                            else
                                cout << "\\(" << (is_number(tokens[0]) ? tokens[0] : var_names[ins_list[j].l_id]) << " "
                                << tokens[1] << " "
                                << (is_number(tokens[2]) ? tokens[2] : var_names[ins_list[j].r_id1]) << " "
                                << "\\hfill{" << tokens[3] << "}\\)" << endl << endl;
                        } else //binary
                            cout << "\\(" << (is_number(tokens[0]) ? tokens[0] : var_names[ins_list[j].l_id]) << " "
                                << tokens[1] << " " << tokens[2] << ",\\ "
                                << (is_number(tokens[3]) ? tokens[3] : var_names[ins_list[j].r_id1]) << ", "
                                << (is_number(tokens[4]) ? tokens[4] : var_names[ins_list[j].r_id2]) << " "
                                << "\\hfill{" << tokens[5] << "}\\)" << endl << endl;
                        break;
                    case LABEL_JUMP:
                        //cout << "\\quad \\textbf{" << ins_list[j].str << "}" << endl << endl;
                        cout << "\\quad \\textbf{" << tokens[0] << "\\ " << tokens[1] << "\\hfill{" << tokens[2] << "}}" << endl << endl; 
                        break;
                    case ELSE:
                        cout << tokens[0] << "\\hfill{" << tokens[1] << "}" << endl << endl; 
                        break;
                    default:
                        cout << ins_list[j].str << endl << endl;
                }
            }
            cout << "\\vspace{5mm}" << endl << endl;
        }
    cout << "\\end{document}" << endl << endl;

    return 0;
}
