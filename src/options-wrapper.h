//This method is taken from QEMU

#if defined(GENERATE_FLAGS)
#define DEF(option, opt_help) int FLAG_##option = 0;

#elif defined(GENERATE_OPTIONS)
#define DEF(option, opt_help) { #option, no_argument, &FLAG_##option, 1 },

#elif defined(GENERATE_USAGE)
" [-i INPUTFILE] [-o OUTPUTFILE] [-h] [-help] [-u] [-usage] [-dfst] [-ALL]"
#define DEF(option, opt_help) " [-"#option"]"

#elif defined(GENERATE_HELP)
#define DEF(option, opt_help) "\t-"#option "\t\t" opt_help "\n"

#elif defined(GENERATE_OR_FLAGS)
#define DEF(option, opt_help) || FLAG_##option

#elif defined(GENERATE_ASSIGN_FLAGS)
#define DEF(option, opt_help) FLAG_##option =

#else
#error "options-wrapper.h included with no option defined"
#endif

#include "options.def"

#undef DEF
#undef GENERATE_FLAGS
#undef GENERATE_OPTIONS
#undef GENERATE_USAGE
#undef GENERATE_HELP
#undef GENERATE_OR_FLAGS
#undef GENERATE_ASSIGN_FLAGS
