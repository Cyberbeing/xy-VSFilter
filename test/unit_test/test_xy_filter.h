#ifndef __TEST_XY_FILTER_0B6AC4E9_AA51_4EF9_B255_90792DC07DB9_H__
#define __TEST_XY_FILTER_0B6AC4E9_AA51_4EF9_B255_90792DC07DB9_H__

#include <gtest/gtest.h>
#include <wtypes.h>
#include <math.h>
#include <xmmintrin.h>
#include "xy_malloc.h"

void xy_filter_c(float *dst, int width, int height, int stride, const float *filter, int filter_width);
void xy_filter_one_line_c(float *dst, int width, const float *filter, int filter_width)
{
    xy_filter_c(dst, width, 1, ((width*4)+15)&~15, filter, filter_width);
}
void xy_filter_sse(float *dst, int width, int height, int stride, const float *filter, int filter_width);
void xy_filter_one_line_sse(float *dst, int width, const float *filter, int filter_width)
{
    xy_filter_sse(dst, width, 1, ((width*4)+15)&~15, filter, filter_width);
}

template<typename T>
std::ostream& GetBufString(std::ostream& os, T* buf, int len)
{
    for (int i=0;i<len;i++)
    {
        os<<buf[i]<<" ";
    }
    return os;
}

// Usable AlmostEqual function

bool AlmostEqual2sComplement(float A, float B, int maxUlps)
{

    // Make sure maxUlps is non-negative and small enough that the

    // default NAN won't compare as equal to anything.

    assert(maxUlps > 0 && maxUlps < 4 * 1024 * 1024);

    int aInt = *(int*)&A;

    // Make aInt lexicographically ordered as a twos-complement int

    if (aInt < 0)

        aInt = 0x80000000 - aInt;

    // Make bInt lexicographically ordered as a twos-complement int

    int bInt = *(int*)&B;

    if (bInt < 0)
        bInt = 0x80000000 - bInt;

    int intDiff = abs(aInt - bInt);

    if (intDiff <= maxUlps)
        return true;

    return false;

}

class XyFilterTestData
{
public:
    static const int MAX_FILTER_LENGTH = 256;
    static const int MAX_DATA_BUFF_SIZE = 512*512;
    static const int MAX_BUFF_SIZE = MAX_DATA_BUFF_SIZE + 2*MAX_FILTER_LENGTH;

    float *buff_base;

    float *filter_f;

    float *data;
    int w, h, pitch;
    int filter_width;
    int ex_filter_width;

    XyFilterTestData():buff_base(NULL),filter_f(NULL),data(NULL)
    {
        buff_base = (float*)xy_malloc(MAX_BUFF_SIZE*sizeof(float));
        data = buff_base + MAX_FILTER_LENGTH;
        filter_f = data + MAX_DATA_BUFF_SIZE;
    }
    ~XyFilterTestData()
    {
        xy_free(buff_base);buff_base=NULL;
    }

    const XyFilterTestData& copy (const XyFilterTestData& rhs)
    {
        w = rhs.w;
        h = rhs.h;
        pitch = rhs.pitch;
        filter_width = rhs.filter_width;
        ex_filter_width = rhs.ex_filter_width;

        memcpy(buff_base, rhs.buff_base, MAX_BUFF_SIZE*sizeof(float));        
        return *this;
    }
    bool operator== (const XyFilterTestData& rhs) const
    {
        for (int i=0;i<MAX_BUFF_SIZE;i++)
        {
            if (!AlmostEqual2sComplement(buff_base[i], rhs.buff_base[i],8))
            {
                std::cerr<<"Not almost equal at: "<<i+buff_base-data
                    <<" lhs:"<<buff_base[i]
                    <<" rhs:"<<rhs.buff_base[i]
                    <<std::endl;
                return false;
            }
        }
        return true;
    }

    void FillRandData(int w, int h, int filter_width)
    {
        this->w = w;
        this->h = h;
        this->filter_width = filter_width;
        this->ex_filter_width = ((filter_width+3)&~3);        
        this->pitch = ((w+ex_filter_width+3)&~3)*sizeof(float);

        ASSERT(this->pitch*this->h <= MAX_DATA_BUFF_SIZE);

        for (int i=0;i<h;i++)
        {
            float *data2 = (float*)((PUINT8)data + i*pitch);
            for (int j=0;j<w+filter_width;j++)
            {
                data2[j] = rand()&0xFF;
            }
        }
        float sum = 0;
        for (int i=0;i<filter_width;i++)
        {
            filter_f[i] = abs((rand()&0xFFFFF)+0xFFFFF);
            sum += filter_f[i];
        }
        for (int i=0;i<filter_width;i++)
        {
            filter_f[i] /= sum;
        }
        for (int i=filter_width;i<ex_filter_width;i++)
        {
            filter_f[i] = 0;
        }
    }
private:
    XyFilterTestData(const XyFilterTestData&);
    const XyFilterTestData& operator= (const XyFilterTestData& rhs);
};

std::ostream& operator<<( std::ostream& os, const XyFilterTestData& data )
{
    os<<"filter:"<<std::endl;
    GetBufString<const float>(os, data.filter_f, data.filter_width)<<" | ";
    GetBufString<const float>(os, data.filter_f+data.filter_width, data.ex_filter_width - data.filter_width)<<std::endl;
    os<<"data:"<<std::endl;
    GetBufString<const float>(os, data.data-data.ex_filter_width, data.ex_filter_width)<<" | ";
    GetBufString<const float>(os, data.data, data.w-data.ex_filter_width)<<" | ";
    GetBufString<const float>(os, data.data+data.w-data.ex_filter_width, data.ex_filter_width)<<std::endl;

    return os;
}

class XyFilterTest : public ::testing::Test 
{
public:
    XyFilterTestData data0, data1, data2;
protected:
    virtual void SetUp() 
    {
    }
};

#define LOG_VAR(x) " "#x" "<<x<<" "

TEST_F(XyFilterTest, c_vs_sse)
{
    int LOOP_NUM = 10;
    for(int WIDTH=4;WIDTH<256;WIDTH+=4)
        for (int FILTER_WIDTH=3;FILTER_WIDTH<32;FILTER_WIDTH++)
        {
            for(int i=0;i<LOOP_NUM;i++)
            {
                data0.FillRandData(WIDTH, 1, FILTER_WIDTH);
                data1.copy(data0);
                data2.copy(data0);
                xy_filter_one_line_c(data1.data, data1.w, data1.filter_f, data1.ex_filter_width);
                xy_filter_one_line_sse(data2.data, data2.w, data2.filter_f, data2.ex_filter_width);
                ASSERT_EQ(true, data2.operator==(data1))
                    <<LOG_VAR(i)<<LOG_VAR(WIDTH)<<LOG_VAR(FILTER_WIDTH)<<std::endl
                    <<"data0:"<<data0
                    <<"data1:"<<data1
                    <<"data2:"<<data2;
            }
        }
}


#endif // __TEST_XY_FILTER_0B6AC4E9_AA51_4EF9_B255_90792DC07DB9_H__
