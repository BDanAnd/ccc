//This method is taken from QEMU

#if defined(GENERATE_FLAGS)
#define DEF(option, opt_type, opt_help, dependences, invalidated) int FLAG_##option = 0;
#define DEFHEAD(str)

#elif defined(GENERATE_OPTIONS)
#define DEF(option, opt_type, opt_help, dependences, invalidated) { #option, no_argument, &FLAG_##option, 1 },
#define DEFHEAD(str)

#elif defined(GENERATE_USAGE)
" [-i INPUTFILE] [-o OUTPUTFILE] [-h] [-help] [-u] [-usage] [-dfst] [-friendly] [-ALL]"
#define DEF(option, opt_type, opt_help, dependences, invalidated) " [-"#option"]"
#define DEFHEAD(str)

#elif defined(GENERATE_HELP)
#define DEF(option, opt_type, opt_help, dependences, invalidated) "\t-"#option "\t\t" opt_help "\n"
#define DEFHEAD(str) "  " str "\n"

#elif defined(GENERATE_OR_FLAGS)
#define DEF(option, opt_type, opt_help, dependences, invalidated) || (opt_type != OPTIMIZATION ? FLAG_##option : 0)
#define DEFHEAD(str)

#elif defined(GENERATE_ASSIGN_FLAGS)
#define DEF(option, opt_type, opt_help, dependences, invalidated) FLAG_##option =
#define DEFHEAD(str)

#elif defined(GENERATE_FUNCTIONS_PROPS)
#define dv ,
#define DEF(option, opt_type, opt_help, dependences, invalidated) all_functions[OPTION_##option] = {opt_type, false, false, #option, dependences, {}, invalidated};
#define DEFHEAD(str)

#elif defined(GENERATE_NECESSARY_FUNCTIONS)
#define DEF(option, opt_type, opt_help, dependences, invalidated) if (FLAG_##option) all_functions[OPTION_##option].used = true;
#define DEFHEAD(str)

#else
#error "options-wrapper.h included with no option defined"
#endif

#include "options.h"

#undef DEF
#undef dv
#undef DEFHEAD
#undef GENERATE_FLAGS
#undef GENERATE_OPTIONS
#undef GENERATE_USAGE
#undef GENERATE_HELP
#undef GENERATE_OR_FLAGS
#undef GENERATE_ASSIGN_FLAGS
#undef GENERATE_FUNCTIONS_PROPS
#undef GENERATE_NECESSARY_FUNCTIONS
