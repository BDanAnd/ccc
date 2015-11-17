#include "printers.h"

using namespace std;

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


void print_instruction(vector<string>& str, instruction* i, bool singleline)
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


int OPTION_IR(analysis_state& state, bool b)
{
    if (!b)
        return 0;

    map<basic_block*, string> tmp_names_for_bb;
    set_tmp_names_for_bb(state, tmp_names_for_bb);

    for (auto bb : state.bb_list) {
        if (bb == state.entry_bb || bb == state.exit_bb)
            continue;
        cout << BB_NAME(bb) << endl;
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
        cout << endl;
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
