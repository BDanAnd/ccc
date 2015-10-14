#ifndef TYPES_H
#define TYPES_H

#include <vector>

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

// 0 element - left part; 1 and 2 elements - right part
// type = COND -> 1 and 2 elements valid
// type = RETURN -> 0 element valid
// type = BINARY or COND -> subtype valid
struct instruction
{
    instruction_type type;
    operation_type subtype;

    operand ops[3];

    int string_num; // line numbers from input
};

struct basic_block
{
    vector<instruction*> ins_list;
};

struct analysis_state
{
    bool instruction_string_num_valid = true; // false -> string_num undefined

    vector<string> str; // store all string representations
    vector<instruction*> instructions_list; //store all instructions
    vector<basic_block*> bb_list; // store all basic blocks
};

#endif // TYPES_H
