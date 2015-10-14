#include <iostream>
#include <fstream>
#include <getopt.h>
#include "types.h"

using namespace std;

int main(int argc, char* argv[])
{
    //parse args
    int use_dfst = 0, all = 0;
    #define GENERATE_FLAGS
    #include "options-wrapper.h"

    char *input = NULL, *output = NULL;
    for (;;) {
        static struct option longopts[] =
        {
            { "help", no_argument, 0, 'h' },
            { "usage", no_argument, 0, 'u' },
            { "dfst", no_argument, &use_dfst, 1 },
            { "ALL", no_argument, &all, 1 },
            #define GENERATE_OPTIONS
            #include "options-wrapper.h"
            { 0, 0, 0, 0 }
        };
        int optidx = 0;
        int c = getopt_long_only(argc, argv, "hui:o:", longopts, &optidx);
        if (c == -1)
            break;
        switch (c) {
            case 0:
                break;
            case 'h':
                cerr << "Usage: " << argv[0] <<
                #define GENERATE_USAGE
                #include "options-wrapper.h"
                << "\n"
                << "Options:\n"
                << "\t-h,-help\t\tShow this help list\n"
                << "\t-u,-usage\t\tShow a short usage message\n"
                << "\t-i <INPUTFILE>\t\tRead from INPUTFILE\n"
                << "\t-o <OUTPUTFILE>\t\tWrite to OUTPUTFILE\n"
                << "\t-dfst\t\t\tUse DFST algorithm for BBs numeration\n"
                << "\t-ALL\t\t\tPrint all (union of all the following flags)\n"
                #define GENERATE_HELP
                #include "options-wrapper.h"
                << endl;
                return 0;
            case 'u':
                cerr << "Usage: " << argv[0] <<
                #define GENERATE_USAGE
                #include "options-wrapper.h"
                << endl;
                return 0;
            case 'i':
                if (input)
                    cerr << "Warning: set new input file '" << optarg << "'" << endl;
                input = optarg;
                break;
            case 'o':
                if (output)
                    cerr << "Warning: set new output file '" << optarg << "'" << endl;
                output = optarg;
                break;
            case '?':
                cerr << "Try '" << argv[0] << " -help' or '" << argv[0] << " -usage' for more information" << endl;
                return 1;
            default:
                return 1;
        }
    }
    if (!(all
        #define GENERATE_OR_FLAGS
        #include "options-wrapper.h"
        )) {
        cerr << "Error: No any requests (output opts)\nTry '" << argv[0] << " -help' or '" << argv[0] << " -usage' for more information" << endl;
        return 1;
    }

    if (all)
        #define GENERATE_ASSIGN_FLAGS
        #include "options-wrapper.h"
        1;

    //redirect streams
    ifstream in;
    ofstream out;
    if (input) {
        in.open(input);
        cin.rdbuf(in.rdbuf());
    }
    if (output) {
        out.open(output);
        cout.rdbuf(out.rdbuf());
    }

    return 0;
}
