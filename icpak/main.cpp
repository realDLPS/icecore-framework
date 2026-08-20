#include <iostream>
#include <string>
#include <windows.h>

//#include "src/icpaker.hpp"


#if defined(BUILD_TEST)
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../doctest/doctest/doctest.h"
#include "src/icpak_types.hpp"
#else
int main(void)
{
    return paker();
}
#endif