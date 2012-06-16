#ifndef __TEST_INTERLACED_UV_ALPHABLEND_E6211D22_124C_48C1_9202_F629C33143F9_H__
#define __TEST_INTERLACED_UV_ALPHABLEND_E6211D22_124C_48C1_9202_F629C33143F9_H__

#include <gtest/gtest.h>
#include <wtypes.h>
#include <cmath>
#include <sstream>
#include <ostream>
#include <iomanip>

void mix_line_a_y_uv(BYTE *dst_a, BYTE *dst_y, WORD *dst_uv, BYTE src_y, WORD src_uv, const BYTE *alpha, int w);

void AlphaBltUV(WORD* pUV,
    const byte* pAlphaMask, 
    const WORD UV, 
    int h, int w, int src_stride, int dst_stride);

void AlphaBlt(byte* pY,
    const byte* pAlphaMask, 
    const byte Y, 
    int h, int w, int src_stride, int dst_stride);

struct A_Y_UV_TestData
{
    static const int BUF_SIZE = 2048;
    BYTE *dst_a;
    BYTE *dst_y;
    WORD *dst_uv;
    BYTE src_y;
    WORD src_uv;
    BYTE *alpha;
    int w;

    A_Y_UV_TestData()
    {
        memset(this, 0, sizeof(*this));
        dst_a = new BYTE[BUF_SIZE*5];        
        dst_y = dst_a + BUF_SIZE;
        dst_uv = reinterpret_cast<WORD*>(dst_y + BUF_SIZE);
        alpha = reinterpret_cast<BYTE*>(dst_uv + BUF_SIZE);
    }
    ~A_Y_UV_TestData()
    {
        delete[] dst_a;
    }

    bool operator==(const A_Y_UV_TestData& rhs)
    {
        return ( w == rhs.w ) &&
            !memcmp(dst_a, rhs.dst_a, w*sizeof(*dst_a)) &&
            !memcmp(dst_y, rhs.dst_y, w*sizeof(*dst_y)) &&
            !memcmp(dst_uv, rhs.dst_uv, w*sizeof(*dst_uv)) &&
            !memcmp(alpha, rhs.alpha, w*sizeof(*alpha)) &&
            src_y == rhs.src_y &&
            src_uv == rhs.src_uv;
    }
    const A_Y_UV_TestData& operator=(const A_Y_UV_TestData& rhs)
    {
        w = rhs.w;
        memcpy(dst_a, rhs.dst_a, w*sizeof(*dst_a));
        memcpy(dst_y, rhs.dst_y, w*sizeof(*dst_y));
        memcpy(dst_uv, rhs.dst_uv, w*sizeof(*dst_uv));
        memcpy(alpha, rhs.alpha, w*sizeof(*alpha));
        src_y = rhs.src_y;
        src_uv = rhs.src_uv;
        return *this;
    }

    friend std::ostream& operator<<(std::ostream& os, const A_Y_UV_TestData& test_data);   
    std::string GetAlphaString()
    {
        std::stringstream os;
        os<<std::endl<<"alpha:"<<std::endl;
        GetBufString(os, alpha);
        return os.str();
    }
    std::string GetDstYString()
    {
        std::stringstream os;
        os<<std::endl<<"Dst Y:"<<std::endl;
        GetBufString(os, dst_y);
        return os.str();
    }
    std::string GetDstUVString()
    {
        std::stringstream os;
        os<<std::endl<<"Dst UV:"<<std::endl;
        GetBufString(os, dst_uv);
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

std::ostream& operator<<(std::ostream& os, const A_Y_UV_TestData& test_data)
{
    os<<std::endl          
        <<"w:"<<test_data.w<<" src_y:"<<test_data.src_y<<" src_uv:"<<test_data.src_uv
        <<std::endl;
    os<<std::hex<<std::setw(4);
    os<<"alpha:"<<std::endl;
    for (int i=0;i<test_data.w;i++)
    {
        os<<static_cast<int>(test_data.alpha[i])<<" ";
    }
    os<<std::endl;
    os<<"dst_a:"<<std::endl;
    for (int i=0;i<test_data.w;i++)
    {
        os<<static_cast<int>(test_data.dst_a[i])<<" ";
    }
    os<<std::endl;
    os<<"dst_y:"<<std::endl;
    for (int i=0;i<test_data.w;i++)
    {
        os<<static_cast<int>(test_data.dst_y[i])<<" ";
    }
    os<<std::endl;
    os<<"dst_uv:"<<std::endl;
    for (int i=0;i<test_data.w;i++)
    {
        os<<static_cast<int>(test_data.dst_uv[i])<<" ";
    }
    os<<std::endl;
    os<<std::dec;
    return os;
}

class InterlacedUvAlphaBlendTest : public ::testing::Test 
{
protected:
    virtual void SetUp() 
    {  
    }

    const A_Y_UV_TestData& GetZeroData()
    {
        memset(data1.dst_a, 0, A_Y_UV_TestData::BUF_SIZE*5);
        return data1;
    }
    const A_Y_UV_TestData& GetRandomData(int w)
    {
        data1.w = w;
        FillRandByte( data1.dst_a, w );
        FillRandByte( data1.dst_y, w );
        FillRandByte( reinterpret_cast<BYTE*>(data1.dst_uv), w*sizeof(WORD) );
        FillRandByte( data1.alpha, w );
        data1.src_y = rand() & 0xff;
        data1.src_uv = rand() & 0xffff;
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
    A_Y_UV_TestData data1;
    //A_Y_UV_TestData data2[20];
    //A_Y_UV_TestData data3[20];
};



TEST_F(InterlacedUvAlphaBlendTest, WhatEver)
{
    A_Y_UV_TestData data0,data1,data2;

    data0 = GetZeroData();
    data1 = data0;
    data2 = data1;

    mix_line_a_y_uv( data1.dst_a, data1.dst_y, data1.dst_uv, data1.src_y, data1.src_uv, data1.alpha, data1.w );

    AlphaBlt( data2.dst_a, data2.alpha, 0, 1, data2.w, data2.w, data2.w );
    AlphaBlt( data2.dst_y, data2.alpha, data2.src_y, 1, data2.w, data2.w, data2.w );
    AlphaBltUV( data2.dst_uv, data2.alpha, data2.src_uv, 1, data2.w, data2.w, data2.w );       
    ASSERT_EQ(true, data1==data0);
    ASSERT_EQ(true, data2==data0);

    for (int i=0;i<10000;i++)
    {
        for (int w=1;w<33;w++)
        {            
            data0 = GetRandomData(w);
            data1 = data0;
            data2 = data1;

            mix_line_a_y_uv( data1.dst_a, data1.dst_y, data1.dst_uv, data1.src_y, data1.src_uv, data1.alpha, data1.w );

            AlphaBlt( data2.dst_a, data2.alpha, 0, 1, data2.w, data2.w, data2.w );
            AlphaBlt( data2.dst_y, data2.alpha, data2.src_y, 1, data2.w, data2.w, data2.w );
            AlphaBltUV( data2.dst_uv, data2.alpha, data2.src_uv, 1, data2.w, data2.w, data2.w );
            //ASSERT_EQ(true, data1==data2);
            ASSERT_EQ(0, memcmp(data2.dst_a, data1.dst_a, data1.w) )
                <<"data0"<<data0
                <<"data1"<<data1
                <<"data2"<<data2;
            ASSERT_EQ(0, memcmp(data2.dst_y, data1.dst_y, data1.w) )
                <<"data0 src_y:"<<data0.src_y<<" w:"<<data0.w
                <<data0.GetAlphaString().c_str()
                <<data0.GetDstYString().c_str()
                <<std::endl<<"data1 src_y:"<<data1.src_y<<" w:"<<data1.w
                <<data1.GetDstYString().c_str()
                <<std::endl<<"data2 src_y:"<<data2.src_y<<" w:"<<data2.w
                <<data2.GetDstYString().c_str();
            ASSERT_EQ(0, memcmp(data2.dst_uv, data1.dst_uv, data1.w*sizeof(*data1.dst_uv)) )
                <<"data0 src_uv:"<<data0.src_uv
                <<data0.GetAlphaString().c_str()
                <<data0.GetDstUVString().c_str()
                <<std::endl<<"data1 src_uv:"<<data1.src_uv
                <<data1.GetDstUVString().c_str()
                <<std::endl<<"data2 src_uv:"<<data2.src_uv
                <<data2.GetDstUVString().c_str();
        }
        data0 = GetRandomData(1920);
        data1 = data0;
        data2 = data1;

        mix_line_a_y_uv( data1.dst_a, data1.dst_y, data1.dst_uv, data1.src_y, data1.src_uv, data1.alpha, data1.w );

        AlphaBlt( data2.dst_a, data2.alpha, 0, 1, data2.w, data2.w, data2.w );
        AlphaBlt( data2.dst_y, data2.alpha, data2.src_y, 1, data2.w, data2.w, data2.w );
        AlphaBltUV( data2.dst_uv, data2.alpha, data2.src_uv, 1, data2.w, data2.w, data2.w );
        //ASSERT_EQ(true, data1==data2);
        ASSERT_EQ(0, memcmp(data2.dst_a, data1.dst_a, data1.w) )
            <<"data0"<<data0
            <<"data1"<<data1
            <<"data2"<<data2;
        ASSERT_EQ(0, memcmp(data2.dst_y, data1.dst_y, data1.w) )
            <<"data0"<<data0
            <<"data1"<<data1
            <<"data2"<<data2;
        ASSERT_EQ(0, memcmp(data2.dst_uv, data1.dst_uv, data1.w*sizeof(*data1.dst_uv)) )
            <<"data0 src_uv:"<<data0.src_uv
            <<data0.GetAlphaString().c_str()
            <<data0.GetDstUVString().c_str()
            <<std::endl<<"data1 src_uv:"<<data1.src_uv
            <<data1.GetDstUVString().c_str()
            <<std::endl<<"data2 src_uv:"<<data2.src_uv
            <<data2.GetDstUVString().c_str();
    }    
}

#endif // __TEST_INTERLACED_UV_ALPHABLEND_E6211D22_124C_48C1_9202_F629C33143F9_H__
