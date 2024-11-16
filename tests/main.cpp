#include <limits>
#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"
#include "Luau/Common.h"

LUAU_FASTFLAG(LuauSolverV2)
LUAU_FASTFLAG(LuauNewSolverPopulateTableLocations)
LUAU_FASTFLAG(LuauNewSolverPrePopulateClasses)

int main(int argc, const char** argv)
{
    doctest::Context context;
    context.applyCommandLine(argc, argv);

    if (doctest::parseFlag(argc, argv, "--new-solver"))
    {
        FFlag::LuauSolverV2.value = true;

        // These are required flags for internal functionality.
        FFlag::LuauNewSolverPopulateTableLocations.value = true;
        FFlag::LuauNewSolverPrePopulateClasses.value = true;
    }

    int test_result = context.run(); // run queries, or run tests unless --no-run

    if (context.shouldExit()) // honor query flags and --exit
        return test_result;

    return test_result; // combine the 2 results
}