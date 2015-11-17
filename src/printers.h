#ifndef PRINTERS_H
#define PRINTERS_H

#include "types.h"
#include "helpers.h"

void print_operand(vector<string>&, operand&);
void print_expression(vector<string>&, expression&);
void print_instruction(vector<string>&, instruction*, bool singleline = false);
int OPTION_IR(analysis_state&, bool);
int OPTION_G(analysis_state&, bool);
int OPTION_FG(analysis_state&, bool);

#endif // PRINTERS_H
