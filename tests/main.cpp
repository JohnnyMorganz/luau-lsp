#include <limits>
#define DOCTEST_CONFIG_NO_UNPREFIXED_OPTIONS
#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"
#include "Luau/Common.h"

LUAU_FASTFLAG(LuauSolverV2)
LUAU_DYNAMIC_FASTINT(LuauTypeSolverRelease)

int main(int argc, const char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        if (argv[i][0] != '-')
            continue;
        if (strcmp(argv[i], "--new-solver") == 0)
        {
            FFlag::LuauSolverV2.value = true;
            DFInt::LuauTypeSolverRelease.value = std::numeric_limits<int>::max();
            break;
        }
    }

    doctest::Context context(argc, argv);
    int test_result = context.run(); // run queries, or run tests unless --no-run

    if (context.shouldExit()) // honor query flags and --exit
        return test_result;

    return test_result; // combine the 2 results
}