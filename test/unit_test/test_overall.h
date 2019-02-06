#ifndef __TEST_OVERALL_09562649_9C15_413C_81EA_6B6EE98E9CBB_H__
#define __TEST_OVERALL_09562649_9C15_413C_81EA_6B6EE98E9CBB_H__

#include <gtest/gtest.h>

void OpenTestScript( const char *filename );
void OverallTest(float fps = 25, int width=3840, int height=2160,
    double start=0, double end=60);

TEST(OverallTest, xxx)
{
    OverallTest();
}


#endif // __TEST_OVERALL_09562649_9C15_413C_81EA_6B6EE98E9CBB_H__