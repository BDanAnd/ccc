#ifndef HELPERS_H
#define HELPERS_H

#include "types.h"

void set_tmp_names_for_bb(analysis_state&, map<basic_block*, string>&);
void delete_ins(analysis_state&, instruction*);
int split_bbs(analysis_state&);
int merge_bbs(analysis_state&);
void replace_var(operand&, int, int, bool change_def = false);
int add_bb_before(analysis_state&, basic_block*);
int call_functions(analysis_state&, queue<func_ptr>, map<func_ptr, func_props>&, bool need_print = true);
int check_circular_dependencies(map<func_ptr, func_props>&, func_ptr, set<func_ptr>);
void free_memory(analysis_state&);

#endif // HELPERS_H
