//This method is taken from QEMU

#if defined(GENERATE_FLAGS)
#define DEF(option, opt_help, dependences) int FLAG_##option = 0;

#elif defined(GENERATE_OPTIONS)
#define DEF(option, opt_help, dependences) { #option, no_argument, &FLAG_##option, 1 },

#elif defined(GENERATE_USAGE)
" [-i INPUTFILE] [-o OUTPUTFILE] [-h] [-help] [-u] [-usage] [-dfst] [-ALL]"
#define DEF(option, opt_help, dependences) " [-"#option"]"

#elif defined(GENERATE_HELP)
#define DEF(option, opt_help, dependences) "\t-"#option "\t\t" opt_help "\n"

#elif defined(GENERATE_OR_FLAGS)
#define DEF(option, opt_help, dependences) || FLAG_##option

#elif defined(GENERATE_ASSIGN_FLAGS)
#define DEF(option, opt_help, dependences) FLAG_##option =

#elif defined(GENERATE_FUNCTIONS_PROPS)
#define dv ,
#define DEF(option, opt_help, dependences) all_functions[OPTION_##option] = {false, false, false, dependences};

#elif defined(GENERATE_NECESSARY_FUNCTIONS)
#define DEF(option, opt_help, dependences) if (FLAG_##option) all_functions[OPTION_##option].used = all_functions[OPTION_##option].need_print = true;

#else
#error "options-wrapper.h included with no option defined"
#endif

#include "options.h"

#undef DEF
#undef dv
#undef GENERATE_FLAGS
#undef GENERATE_OPTIONS
#undef GENERATE_USAGE
#undef GENERATE_HELP
#undef GENERATE_OR_FLAGS
#undef GENERATE_ASSIGN_FLAGS
#undef GENERATE_FUNCTIONS_PROPS
#undef GENERATE_NECESSARY_FUNCTIONS