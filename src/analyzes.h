#ifndef ANALYZES_H
#define ANALYZES_H

#include "types.h"
#include "helpers.h"
#include "printers.h"

int OPTION_SETS(analysis_state&, bool);
int OPTION_RD(analysis_state&, bool);
int OPTION_LV(analysis_state&, bool);
int OPTION_AE(analysis_state&, bool);
int OPTION_CD(analysis_state&, bool);
int OPTION_NL(analysis_state&, bool);
vector<instruction*> search_invariant_calculations(analysis_state&, bitvector, bool);
vector<tuple<instruction*, int, int, int, int> > search_induction_vars(analysis_state&, bitvector, vector<instruction*>, bool);

#endif // ANALYZES_H
