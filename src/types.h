#ifndef TYPES_H
#define TYPES_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <getopt.h>
#include <vector>
#include <map>
#include <algorithm>
#include <iterator>
#include <string>

using namespace std;

enum operand_type {
    CONST,
    VAR,
    ARRAY
};

//type != ARRAY -> "subtype" and "b" undefined
//type (subtype) = CONST -> "a" ("b") immediate value
//type (subtype) = VAR -> "a" ("b") index in string vector
struct operand
{
    operand_type type;
    operand_type subtype;
    int a, b;
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
    string("=>"),
    string("<=")
};

// ops[0] - left part; ops[1] and ops[2] - right part
// type = COND -> ops[1] and ops[2] valid
// type = RETURN -> ops[1] valid
// type = BINARY or COND -> subtype valid
// type = COND -> label_id valid
// type = UNCOND -> label_id[0] valid
struct instruction
{
    instruction_type type;
    operation_type subtype;

    operand ops[3];

    int string_num; // line numbers from input

    int label_id[2];
};

struct basic_block
{
    int name = -1;
    vector<instruction*> ins_list;
    vector<basic_block*> succ, pred; // sets of successor and predecessor
    int label_id = -1; // fallthrough helper for empty or noncond blocks
};

struct analysis_state
{
    bool instruction_string_num_valid = false; // false -> string_num undefined

    vector<string> str; // all string representations
    vector<instruction*> instructions_list; // all instructions
    vector<basic_block*> bb_list; // all basic blocks

    basic_block* entry_bb = NULL;
    basic_block* exit_bb = NULL;
};

#endif // TYPES_H
