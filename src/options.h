DEF(IR, "Print IR with BB labels", {})
DEF(G, "Print digraph for graphviz dot", {})
DEF(FG, "Print full digraph for graphviz dot", {})
DEF(SETS, "Print sets for all BBs", {})
DEF(RD, "Print reaching definitions analysis", {OPTION_SETS})
DEF(LV, "Print live variable analysis", {OPTION_SETS})
DEF(AE, "Print available expressions for all BBs", {OPTION_SETS})
DEF(CSE, "Print FG after CSE optimization", {OPTION_AE})
DEF(CP, "Print FG after copy propagation optimization", {OPTION_SETS})
DEF(SR, "Print FG after strength reduction optimization", {OPTION_RD dv calculate_dominators})
DEF(IVE, "Print FG after induction vars elimination optimization (not work)", {OPTION_RD dv OPTION_LV dv calculate_dominators})

DEF(TASK1, "Print FG after CSE, CP and SR", {})
