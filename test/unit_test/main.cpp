#include <afx.h>
#include <gtest/gtest.h>
#define XY_UNIT_TEST
//#include "test_interlaced_uv_alphablend.h"
#include "test_subsample_and_interlace.h"
#include "test_alphablend.h"
#include "test_instrinsics_macro.h"


int main(int argc, char ** argv)
{
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}