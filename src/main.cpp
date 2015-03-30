#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <algorithm>
#include <unistd.h>
#include <getopt.h>
#include "bitvector.h"

using namespace std;

vector<string> bb_names;
vector<string> var_names;
vector<tuple<string, int> > ins_names;
vector<tuple<int, int> > all_def;

struct ins
{
    int ins_id, l_id, r_id1, r_id2;
};

struct bb
{
    int name_id;
    bitvector gen, kill, use, def, in_rd, out_rd, in_lv, out_lv;
    vector<ins> ins_list;
    vector<int> pred, succ;
};

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
    unsigned a = s.find_first_of("["), b = s.find_last_of("]");
    if (a == string::npos || b == string::npos || a == 0 || b < s.size() - 1)
        return false;
    k1 = s.substr(0, a);
    k2 = s.substr(a + 1, b - (a + 1));
    return true;
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
                os << "(" << var_names[get<1>(t)] << ", " << bb_names[get<1>(ins_names[get<0>(t)])] << ") ";
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

int main(int argc, char* argv[])
{
    //parse args
    int print_graph = 0, print_sets = 0, print_serialize = 0, print_rd = 0, print_lv = 0;
    char *input = NULL, *output = NULL;
    for (;;) {
        static struct option longopts[] =
        {
            { "help", no_argument, 0, 'h' },
            { "usage", no_argument, 0, 'u' },
            { "G", no_argument, &print_graph, 1 },
            { "sets", no_argument, &print_sets, 1 },
            { "serialize", no_argument, &print_serialize, 1 },
            { "RD", no_argument, &print_rd, 1 },
            { "LV", no_argument, &print_lv, 1 },
            { 0,0,0,0 }
        };
        int optidx = 0;
        int c = getopt_long_only(argc, argv, "hui:o:", longopts, &optidx);
        if (c == -1)
            break;
        #define all_coms " [-i INPUTFILE] [-o OUTPUTFILE] [-h] [-help] [-u] [-usage] [-G] [-sets] [-serialize] [-RD] [-LV]"
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
                << "\t-G\t\t\tPrint digraph for graphviz dot\n"
                << "\t-sets\t\t\tPrint gen, kill, use, def sets for all BBs\n"
                << "\t-serialize\t\tPrint serialized info about BBs\n"
                << "\t-RD\t\t\tPrint reaching definitions analysis\n"
                << "\t-LV\t\t\tPrint live variable analysis"
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
    if (!(print_graph || print_sets || print_serialize || print_rd || print_lv)) {
        cerr << "Error: No any requests (output opts)\nTry '" << argv[0] << " -help' or '" << argv[0] << " -usage' for more information" << endl;
        return 1;
    }

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

    vector<bb> bbs;

    //add entry and exit bbs
    int ENTRY_ID = get_index(bb_names, string("entry"), true);
    int EXIT_ID = get_index(bb_names, string("exit"), true);
    bbs.resize(2);
    bbs[0].name_id = ENTRY_ID;
    bbs[1].name_id = EXIT_ID;

    //bbs input
    int cur_bb;
    bool fl = true, errfl = false;
    for (string line; fl && getline(cin, line);) {
        istringstream iss(line);
        vector<string> tokens{istream_iterator<string>{iss}, istream_iterator<string>{}};
        if ((tokens.size() > 0 && tokens[0].compare("goto") == 0) || tokens.size() == 2)
            //tokens.size() == 2 - skip labels, else, ifTrue
            continue;
        string k1, k2;
        switch (tokens.size()) {
            case 1:
                //bb label or transition to graph description
                if (tokens[0].compare("GRAPH") == 0) {
                    fl = false;
                    break;
                }
                cur_bb = get_index(bb_names, tokens[0], true);
                bbs.resize(cur_bb + 1);
                bbs[cur_bb].name_id = cur_bb;
                break;
            case 4:
                //unary operation
                if (is_array_element(tokens[0], k1, k2)) //in left part array element k1[k2]
                bbs[cur_bb].ins_list.push_back({get_index(ins_names, make_tuple(tokens[3], cur_bb), true),
                                                    is_number(k1) ? -1 : get_index(var_names, k1, true),
                                                    is_number(k2) ? -1 : get_index(var_names, k2, true),
                                                    is_number(tokens[2]) ? -1 : get_index(var_names, tokens[2], true)});
                else if (is_array_element(tokens[2], k1, k2)) //in right part array element k1[k2]
                    bbs[cur_bb].ins_list.push_back({get_index(ins_names, make_tuple(tokens[3], cur_bb), true),
                                                    get_index(var_names, tokens[0], true),
                                                    is_number(k1) ? -1 : get_index(var_names, k1, true),
                                                    is_number(k2) ? -1 : get_index(var_names, k2, true)});
                else
                    bbs[cur_bb].ins_list.push_back({get_index(ins_names, make_tuple(tokens[3], cur_bb), true),
                                                    get_index(var_names, tokens[0], true),
                                                    is_number(tokens[2]) ? -1 : get_index(var_names, tokens[2], true),
                                                    -1});
                break;
            case 6:
                //binary operation
                bbs[cur_bb].ins_list.push_back({get_index(ins_names, make_tuple(tokens[5], cur_bb), true),
                                                get_index(var_names, tokens[0], true),
                                                is_number(tokens[3]) ? -1 : get_index(var_names, tokens[3], true),
                                                is_number(tokens[4]) ? -1 : get_index(var_names, tokens[4], true)});
                break;
            case 5:
                //ifTrue with 2 variables in condition
                bbs[cur_bb].ins_list.push_back({get_index(ins_names, make_tuple(tokens[4], cur_bb), true),
                                                -1,
                                                is_number(tokens[1]) ? -1 : get_index(var_names, tokens[1], true),
                                                is_number(tokens[3]) ? -1 : get_index(var_names, tokens[3], true)});
                break;
            case 3:
                //return command
                bbs[cur_bb].ins_list.push_back({get_index(ins_names, make_tuple(tokens[2], cur_bb), true),
                                                -1,
                                                is_number(tokens[1]) ? -1 : get_index(var_names, tokens[1], true),
                                                -1});
                break;
            case 0:
                //empty
                break;
            default:
                //some error
                cout << tokens.size() << ":'" << line << "'" << endl;
                errfl = true;
        }
    }
    if (errfl)
        return 1;

    if (bbs.size() > 2)
        bbs[0].succ.push_back(2);//First real bb
    else
        cerr << "Warning: Empty input code" << endl;

    //graph input
    string b1, b2;
    while (cin >> b1 >> b2) {
        int a = get_index(bb_names, b1, false), b = get_index(bb_names, b2, false);
        if (a < 0 || b < 0)
            return 1;
        bbs[b].pred.push_back(a);
        bbs[a].succ.push_back(b);
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
    for (auto i : bbs)
        for (auto j : i.ins_list) {
            get_index(all_def, make_tuple(j.ins_id, j.l_id), true);
            var_to_ins[j.l_id].push_back(j.ins_id);
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
        for (auto j = i.ins_list.rbegin(); j != i.ins_list.rend(); ++j) {
            if (j->l_id < 0) //skip operations without left part
                continue;
            bitvector tmp_gen(c);
            tmp_gen[get_index(all_def, make_tuple(j->ins_id, j->l_id), false)] = true;
            i.gen = i.gen + (tmp_gen - i.kill);
            for (auto k : var_to_ins[j->l_id]) {
                if (k == j->ins_id)
                        continue;
                i.kill[get_index(all_def, make_tuple(k, j->l_id), false)] = true;
            }
        }
        //calculate use def
        for (auto j : i.ins_list) {
            if (j.r_id1 > -1 && i.def[j.r_id1] == false)
                i.use[j.r_id1] = true;
            if (j.r_id2 > -1 && i.def[j.r_id2] == false)
                i.use[j.r_id2] = true;
            if (j.l_id > -1)
                i.def[j.l_id] = true;
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
            if (i.name_id == ENTRY_ID)
                continue;
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
            if (i.name_id = ENTRY_ID || i.name_id == EXIT_ID)
                continue;//sets for this trivial
            cout << bb_names[i.name_id] << ":" << endl;
            cout << "In_lv : " << print_var_names(i.in_lv) << endl;
            cout << "Out_lv: " << print_var_names(i.out_lv) << endl;
        }
    }
    return 0;
}
