#ifndef OPTIMIZATIONS_H
#define OPTIMIZATIONS_H

#include "types.h"
#include "helpers.h"
#include "analyzes.h"

int OPTION_CSE(analysis_state&, bool);
int OPTION_CP(analysis_state&, bool);
int OPTION_SR(analysis_state&, bool);
int OPTION_IVE(analysis_state&, bool);

#endif // OPTIMIZATIONS_H
