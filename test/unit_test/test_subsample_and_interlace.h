#ifndef __TEST_SUBSAMPLE_AND_INTERLACE_34786202_B970_4091_AC4A_4F4137124732_H__
#define __TEST_SUBSAMPLE_AND_INTERLACE_34786202_B970_4091_AC4A_4F4137124732_H__

#include <gtest/gtest.h>
#include <wtypes.h>
#include <cmath>
#include <sstream>
#include <ostream>
#include <iomanip>
#include "xy_malloc.h"

void subsample_and_interlace_2_line_c(BYTE* dst, const BYTE* u, const BYTE* v, int w, int pitch);
void subsample_and_interlace_2_line_sse2(BYTE* dst, const BYTE* u, const BYTE* v, int w, int pitch);

struct UV_TestData
{
public:
    static const int BUF_SIZE = 2048;
    BYTE *u;
    BYTE *v;
    int w;

    UV_TestData()
    {
        memset(this, 0, sizeof(*this));
        u = reinterpret_cast<BYTE*>(xy_malloc(BUF_SIZE*5));            
        v = u + BUF_SIZE;
    }
    ~UV_TestData()
    {
        xy_free(u);
    }

    void FillZero()
    {
        memset(u, 0, 5*BUF_SIZE);
    }

    bool operator==(const UV_TestData& rhs)
    {
        return ( w == rhs.w ) &&
            !memcmp(u, rhs.u, w*sizeof(*u)) &&
            !memcmp(v, rhs.v, w*sizeof(*v));
    }
    const UV_TestData& operator=(const UV_TestData& rhs)
    {
        w = rhs.w;
        memcpy(u, rhs.u, w*sizeof(*u));
        memcpy(v, rhs.v, w*sizeof(*v));
        return *this;
    }

    friend std::ostream& operator<<(std::ostream& os, const UV_TestData& test_data);   
    std::string GetUString()
    {
        std::stringstream os;
        os<<std::endl<<"u:";
        GetBufString(os, u);
        return os.str();
    }
    std::string GetVString()
    {
        std::stringstream os;
        os<<std::endl<<"v:";
        GetBufString(os, v);
        return os.str();
    }

    template<typename T>
    std::stringstream& GetBufString(std::stringstream& os, T* buf)
    {        
        os<<std::hex;
        for (int i=0;i<w;i++)
        {
            os<<std::setw(4)<<static_cast<int>(buf[i])<<" ";
        }
        os<<std::endl;
        os<<std::dec;
        return os;
    }
};

std::ostream& operator<<(std::ostream& os, const UV_TestData& test_data)
{
    os<<std::endl
        <<"\tw:"<<test_data.w<<std::endl;
    os<<std::hex;
    os<<"\tu:";
    for (int i=0;i<test_data.w;i++)
    {
        os<<std::setw(2)<<static_cast<int>(test_data.u[i])<<" ";
    }
    os<<std::endl;
    os<<"\tv:";
    for (int i=0;i<test_data.w;i++)
    {
        os<<std::setw(2)<<static_cast<int>(test_data.v[i])<<" ";
    }
    os<<std::endl;
    os<<std::dec;
    return os;
}

class SubSampleAndInterlaceTest : public ::testing::Test 
{
protected:
    virtual void SetUp() 
    {  
    }

    const UV_TestData& GetZeroData(int w)
    {
        data1.w = w;
        data1.FillZero();
        return data1;
    }
    const UV_TestData& GetRandomData(int w)
    {
        data1.w = w;
        FillRandByte( data1.u, w );
        FillRandByte( data1.v, w );
        return data1;        
    }
    void FillRandByte(BYTE* buf, int size)
    {
        for (int i=0;i<size;i++)
        {
            buf[i] = rand() & 0xff;
        }
    }

    // virtual void TearDown() {}
    UV_TestData data1;
    //A_Y_UV_TestData data2[20];
    //A_Y_UV_TestData data3[20];
};



TEST_F(SubSampleAndInterlaceTest, CheckSSE2)
{
    UV_TestData data0,data1,data2;

    for (int i=0;i<10000;i++)
    {        
        for (int w=16;w<33*16;w+=16)
        {            
            //zero
            data0 = GetZeroData(w*2);
            data1 = data0;
            data2 = data1;

            subsample_and_interlace_2_line_c( data1.u, data1.u, data1.v, w, w );
            subsample_and_interlace_2_line_sse2( data2.u, data2.u, data2.v, w, w );

            ASSERT_EQ(true, data1==data0)
                <<"data0"<<data0
                <<"data1"<<data1
                <<"data2"<<data2;
            ASSERT_EQ(true, data2==data0)
                <<"data0"<<data0
                <<"data1"<<data1
                <<"data2"<<data2;

            //random
            data0 = GetRandomData(w*2);
            data1 = data0;
            data2 = data1;

            subsample_and_interlace_2_line_c( data1.u, data1.u, data1.v, w, w );
            subsample_and_interlace_2_line_sse2( data2.u, data2.u, data2.v, w, w );

            ASSERT_EQ(true, data1==data2)
                <<"data0"<<data0
                <<"data1"<<data1
                <<"data2"<<data2;
        }
        //large input test
        int w = 960;
        data0 = GetRandomData(w*2);
        data1 = data0;
        data2 = data1;

        subsample_and_interlace_2_line_c( data1.u, data1.u, data1.v, w, w );
        subsample_and_interlace_2_line_sse2( data2.u, data2.u, data2.v, w, w );

        ASSERT_EQ(true, data1==data2)
            <<"data0"<<data0
            <<"data1"<<data1
            <<"data2"<<data2;
    }    
}


#endif // __TEST_SUBSAMPLE_AND_INTERLACE_34786202_B970_4091_AC4A_4F4137124732_H__
