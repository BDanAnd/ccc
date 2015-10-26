#ifndef TYPES_H
#define TYPES_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <getopt.h>
#include <vector>
#include <map>
#include <queue>
#include <set>
#include <algorithm>
#include <iterator>
#include <string>
#include <cassert>
#include "bitvector.h"

using namespace std;

enum operand_type {
    CONST_OPERAND,
    VAR_OPERAND,
    ARRAY_OPERAND
};

//type != ARRAY_OPERAND -> "subtype" and "b" undefined
//type (subtype) = CONST_OPERAND -> "a" ("b") immediate value
//type (subtype) = VAR_OPERAND -> "a" ("b") index in string vector
struct operand
{
    operand_type type;
    operand_type subtype;
    int a, b;
    const bool operator == (const operand& r) const {
        return ( type == r.type && a == r.a && ( (type == ARRAY_OPERAND) ? subtype == r.subtype && b == r.b : 1 ) );
    }
};

enum instruction_type {
    UNARY,
    BINARY,
    COND,
    UNCOND,
    LABEL,
    RETURN
};

enum operation_type {
    PLUS,      // "+"
    MINUS,     // "-"
    DIVIDE,    // "/"
    MULTIPLY,  // "*"
    EQUAL,     // "="
    NOTEQUAL,  // "!="
    LESS,      // "<"
    GREATER,   // ">"
    NOTLESS,   // "=>"
    NOTGREATER // "<="
};


static vector<string> operation_type_str = {
    string("+"),
    string("-"),
    string("/"),
    string("*"),
    string("="),
    string("!="),
    string("<"),
    string(">"),
    string(">="),
    string("<=")
};

struct basic_block;

struct expression
{
    operation_type type;
    operand ops[2];
    const bool operator < (const expression& r) const {
        return ( type < r.type );
    }
    const bool operator == (const expression& r) const {
        return ( type == r.type && ops[0] == r.ops[0] && ops[1] == r.ops[1] );
    }
};

// result - left part; exp - right part
// type = COND -> exp.ops[0] and exp.ops[1] valid
// type = RETURN -> exp.ops[0] valid
// type = UNARY -> result and exp.ops[0] valid
// type = BINARY or COND -> exp.type valid
struct instruction
{
    instruction_type type;

    operand result;
    expression exp;

    int string_num; // line numbers from input
    basic_block* owner = NULL;
};

struct basic_block
{
    int name = -1;
    vector<instruction*> ins_list;
    vector<basic_block*> succ, pred; // sets of successor and predecessor

    // for variables
    bitvector gen, kill;
    bitvector use, def;
    bitvector in_lv, out_lv;

    // for expressions
    bitvector e_gen, e_kill, e_in, e_out;
};

// temporary
#define BB_NAME(bb) ((bb->name < 0) ? tmp_names_for_bb[bb] : state.str[bb->name])

struct analysis_state
{
    bool instruction_string_num_valid = false; // false -> string_num undefined

    vector<string> str; // all var names
    vector<instruction*> instructions_list; // all instructions
    vector<basic_block*> bb_list; // all basic blocks
    vector<instruction*> definitions;
    vector<expression> expressions;

    basic_block* entry_bb = NULL;
    basic_block* exit_bb = NULL;
};

typedef int (*func_ptr)(analysis_state&, bool);

struct func_props
{
    bool done;
    bool used;
    bool need_print;
    string name;
    vector<func_ptr> dependences;
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

#endif // TYPES_H
