#include "stdafx.h"
#include "color_conv_table.h"
#include <string.h>

/************************************
Formula:
 Ey => [0,1]
 Eu => [-0.5,0.5]
 Ev => [-0.5,0.5]
 Er => [0,1]
 Eg => [0,1]
 Eb => [0,1]

 1  = Kr + Kg + Kb
 Ey = Kr * Er + Kg * Eg + Kb * Eb;
 Eu = (Eb - Ey) / (1 - Kb) / 2;
 Ev = (Er - Ey) / (1 - Kr) / 2;

Quantization:
 ANY = ANY * RANGE_SIZE + BASE

Range:
 TV Level
   Y => [16, 235]
   U => [16, 240]
   V => [16, 240]

   R => [16, 235]
   G => [16, 235]
   B => [16, 235]
 PC Level
   Y => [0,255]
   U => [0,255]
   V => [0,255]

   R => [0,255]
   G => [0,255]
   B => [0,255]
************************************/

//RGB to YUV
#define DEFINE_YUV_MATRIX(Kr,Kg,Kb) {                        \
    {   Kr            ,  Kg           ,   Kb            , 0},\
    {  -Kr /((1-Kb)*2), -Kg/((1-Kb)*2),(1-Kb)/((1-Kb)*2), 0},\
    {(1-Kr)/((1-Kr)*2), -Kg/((1-Kr)*2),  -Kb /((1-Kr)*2), 0} \
}

//YUV to RGB: INV stand for inverse
#define DEFINE_YUV_MATRIX_INV(Kr,Kg,Kb) {       \
    {   1,  0             ,  2*(1-Kr)      , 0},\
    {   1, -2*(1-Kb)*Kb/Kg, -2*(1-Kr)*Kr/Kg, 0},\
    {   1,  2*(1-Kb)      ,  0             , 0} \
}

const float MATRIX_BT_601    [3][4] = DEFINE_YUV_MATRIX    (0.299f , 0.587f , 0.114f );
const float MATRIX_BT_601_INV[3][4] = DEFINE_YUV_MATRIX_INV(0.299f , 0.587f , 0.114f );
const float MATRIX_BT_709    [3][4] = DEFINE_YUV_MATRIX    (0.2126f, 0.7152f, 0.0722f);
const float MATRIX_BT_709_INV[3][4] = DEFINE_YUV_MATRIX_INV(0.2126f, 0.7152f, 0.0722f);
const float YUV_PC           [3][4] = {
    {255,   0,   0,   0},
    {  0, 255,   0, 128},
    {  0,   0, 255, 128}
};
const float YUV_PC_INV       [3][4] = {
    {1/255.0f,       0 ,       0 ,          0 },
    {      0 , 1/255.0f,       0 , -128/255.0f},
    {      0 ,       0 , 1/255.0f, -128/255.0f}
};
const float YUV_TV           [3][4] = {
    {219,   0,   0,  16},
    {  0, 224,   0, 128},
    {  0,   0, 224, 128}
};
const float YUV_TV_INV       [3][4] = {
    {1/219.0f,       0 ,       0 , - 16/219.0f},
    {      0 , 1/224.0f,       0 , -128/224.0f},
    {      0 ,       0 , 1/224.0f, -128/224.0f}
};
const float RGB_PC           [3][4] = {
    {255,   0,   0,   0},
    {  0, 255,   0,   0},
    {  0,   0, 255,   0}
};
const float RGB_PC_INV       [3][4] = {
    {1/255.0f,       0 ,       0 ,   0},
    {      0 , 1/255.0f,       0 ,   0},
    {      0 ,       0 , 1/255.0f,   0}
};
const float RGB_TV           [3][4] = {
    {219,   0,   0,  16},
    {  0, 219,   0,  16},
    {  0,   0, 219,  16}
};
const float RGB_TV_INV       [3][4] = {
    {1/219.0f,       0 ,       0 , -16/219.0f},
    {      0 , 1/219.0f,       0 , -16/219.0f},
    {      0 ,       0 , 1/219.0f, -16/219.0f}
};
const float IDENTITY         [3][4] = {
    {  1,   0,   0,   0},
    {  0,   1,   0,   0},
    {  0,   0,   1,   0}
};

inline int clip(int value, int upper_bound)
{
    value &= ~(value>>31);//value = value > 0 ? value : 0
    return value^((value^upper_bound)&((upper_bound-value)>>31));//value = value < upper_bound ? value : upper_bound
}

#define E(M,i,j) M[i*4+j]

void MultiplyMatrix(float *lhs_in_out, const float *rhs)
{
    float tmp1;
    float tmp2;
    float tmp3;

    tmp1 = E(lhs_in_out,0,0);
    tmp2 = E(lhs_in_out,0,1);
    tmp3 = E(lhs_in_out,0,2);

    E(lhs_in_out,0,0) = tmp1 * E(rhs,0,0) + tmp2 * E(rhs,1,0) + tmp3 * E(rhs,2,0);
    E(lhs_in_out,0,1) = tmp1 * E(rhs,0,1) + tmp2 * E(rhs,1,1) + tmp3 * E(rhs,2,1);
    E(lhs_in_out,0,2) = tmp1 * E(rhs,0,2) + tmp2 * E(rhs,1,2) + tmp3 * E(rhs,2,2);
    E(lhs_in_out,0,3) = tmp1 * E(rhs,0,3) + tmp2 * E(rhs,1,3) + tmp3 * E(rhs,2,3)+ E(lhs_in_out,0,3);

    tmp1 = E(lhs_in_out,1,0);
    tmp2 = E(lhs_in_out,1,1);
    tmp3 = E(lhs_in_out,1,2);

    E(lhs_in_out,1,0) = tmp1 * E(rhs,0,0) + tmp2 * E(rhs,1,0) + tmp3 * E(rhs,2,0);
    E(lhs_in_out,1,1) = tmp1 * E(rhs,0,1) + tmp2 * E(rhs,1,1) + tmp3 * E(rhs,2,1);
    E(lhs_in_out,1,2) = tmp1 * E(rhs,0,2) + tmp2 * E(rhs,1,2) + tmp3 * E(rhs,2,2);
    E(lhs_in_out,1,3) = tmp1 * E(rhs,0,3) + tmp2 * E(rhs,1,3) + tmp3 * E(rhs,2,3)+ E(lhs_in_out,1,3);

    tmp1 = E(lhs_in_out,2,0);
    tmp2 = E(lhs_in_out,2,1);
    tmp3 = E(lhs_in_out,2,2);

    E(lhs_in_out,2,0) = tmp1 * E(rhs,0,0) + tmp2 * E(rhs,1,0) + tmp3 * E(rhs,2,0);
    E(lhs_in_out,2,1) = tmp1 * E(rhs,0,1) + tmp2 * E(rhs,1,1) + tmp3 * E(rhs,2,1);
    E(lhs_in_out,2,2) = tmp1 * E(rhs,0,2) + tmp2 * E(rhs,1,2) + tmp3 * E(rhs,2,2);
    E(lhs_in_out,2,3) = tmp1 * E(rhs,0,3) + tmp2 * E(rhs,1,3) + tmp3 * E(rhs,2,3)+ E(lhs_in_out,2,3);
}

class ConvMatrix
{
public:
    enum LevelType
    {
        LEVEL_TV,
        LEVEL_PC,
        LEVEL_COUNT
    };
    enum ColorType
    {
        COLOR_YUV_601,
        COLOR_YUV_709,
        COLOR_RGB,
        COLOR_COUNT
    };
public:
    ConvMatrix();
    virtual ~ConvMatrix();

    bool Init();
    DWORD Convert(int x1, int x2, int x3, int in_level, int in_type, int out_level, int out_type);

    void InitMatrix(int in_level, int in_type, int out_level, int out_type);
private:
    const float * MATRIX_DE_QUAN  [LEVEL_COUNT][COLOR_COUNT];
    const float * MATRIX_INV_TRANS[COLOR_COUNT];
    const float * MATRIX_TRANS    [COLOR_COUNT];
    const float * MATRIX_QUAN     [LEVEL_COUNT][COLOR_COUNT];

    //m_matrix[in_level][in_type][out_level][out_type]
    int * m_matrix[LEVEL_COUNT][COLOR_COUNT][LEVEL_COUNT][COLOR_COUNT];
};

ConvMatrix::ConvMatrix()
{
    memset(m_matrix, 0, LEVEL_COUNT*COLOR_COUNT*LEVEL_COUNT*COLOR_COUNT*sizeof(float*));
}

ConvMatrix::~ConvMatrix()
{
    int * *p_matrix = (int **)m_matrix;
    for(int i=0;i<LEVEL_COUNT*COLOR_COUNT*LEVEL_COUNT*COLOR_COUNT;i++)
    {
        if (p_matrix[i])
        {
            delete [] p_matrix[i];
            p_matrix[i] = NULL;
        }
    }
}

bool ConvMatrix::Init()
{
    MATRIX_DE_QUAN[LEVEL_TV][COLOR_YUV_601] = &YUV_TV_INV[0][0];
    MATRIX_DE_QUAN[LEVEL_TV][COLOR_YUV_709] = &YUV_TV_INV[0][0];
    MATRIX_DE_QUAN[LEVEL_TV][COLOR_RGB    ] = &RGB_TV_INV[0][0];

    MATRIX_DE_QUAN[LEVEL_PC][COLOR_YUV_601] = &YUV_PC_INV[0][0];
    MATRIX_DE_QUAN[LEVEL_PC][COLOR_YUV_709] = &YUV_PC_INV[0][0];
    MATRIX_DE_QUAN[LEVEL_PC][COLOR_RGB    ] = &RGB_PC_INV[0][0];

    MATRIX_INV_TRANS[COLOR_YUV_601] = &MATRIX_BT_601_INV[0][0];
    MATRIX_INV_TRANS[COLOR_YUV_709] = &MATRIX_BT_709_INV[0][0];
    MATRIX_INV_TRANS[COLOR_RGB    ] = &IDENTITY[0][0];

    MATRIX_TRANS[COLOR_YUV_601] = &MATRIX_BT_601[0][0];
    MATRIX_TRANS[COLOR_YUV_709] = &MATRIX_BT_709[0][0];
    MATRIX_TRANS[COLOR_RGB    ] = &IDENTITY[0][0];

    MATRIX_QUAN[LEVEL_TV][COLOR_YUV_601] = &YUV_TV[0][0];
    MATRIX_QUAN[LEVEL_TV][COLOR_YUV_709] = &YUV_TV[0][0];
    MATRIX_QUAN[LEVEL_TV][COLOR_RGB    ] = &RGB_TV[0][0];

    MATRIX_QUAN[LEVEL_PC][COLOR_YUV_601] = &YUV_PC[0][0];
    MATRIX_QUAN[LEVEL_PC][COLOR_YUV_709] = &YUV_PC[0][0];
    MATRIX_QUAN[LEVEL_PC][COLOR_RGB    ] = &RGB_PC[0][0];

    //InitMatrix(LEVEL_PC, COLOR_RGB, LEVEL_TV, COLOR_YUV_601);
    //InitMatrix(LEVEL_PC, COLOR_RGB, LEVEL_TV, COLOR_YUV_709);
    //InitMatrix(LEVEL_TV, COLOR_YUV_601, LEVEL_PC, COLOR_RGB);
    //InitMatrix(LEVEL_TV, COLOR_YUV_709, LEVEL_PC, COLOR_RGB);

    //InitMatrix(LEVEL_TV, COLOR_RGB, LEVEL_TV, COLOR_YUV_601);
    //InitMatrix(LEVEL_TV, COLOR_RGB, LEVEL_TV, COLOR_YUV_709);
    //InitMatrix(LEVEL_TV, COLOR_YUV_601, LEVEL_TV, COLOR_RGB);
    //InitMatrix(LEVEL_TV, COLOR_YUV_709, LEVEL_TV, COLOR_RGB);
    return true;
};

void ConvMatrix::InitMatrix( int in_level, int in_type, int out_level, int out_type )
{
    int * &out_matrix = m_matrix[in_level][in_type][out_level][out_type];
    if (!out_matrix)
    {
        return;
    }
    out_matrix = DEBUG_NEW int[3*4];
    ASSERT(out_matrix);

    float matrix[3][4];
    float *p_matrix = &matrix[0][0];
    memcpy(p_matrix, MATRIX_QUAN[out_level][out_type], 3*4*sizeof(float));
    MultiplyMatrix(p_matrix, MATRIX_TRANS[out_type]);
    MultiplyMatrix(p_matrix, MATRIX_INV_TRANS[in_type]);
    MultiplyMatrix(p_matrix, MATRIX_DE_QUAN[in_level][in_type]);
    for (int i=0;i<3*4;i++)
    {
        out_matrix[i] = p_matrix[i] * (1<<16) + 0.5f;
        ASSERT(out_matrix[i] <¡¡(1<<24));
    }
}

DWORD ConvMatrix::Convert(int x1, int x2, int x3, int in_level, int in_type, int out_level, int out_type)
{
    int * &matrix_int = m_matrix[in_level][in_type][out_level][out_type];
    if (!matrix_int)
    {
        InitMatrix(in_level, in_type, out_level, out_type);
        ASSERT(matrix_int);
    }
    int tmp1 = (E(matrix_int,0,0) * x1 + E(matrix_int,0,1) * x2 + E(matrix_int,0,2) * x3 + E(matrix_int,0,3) + (1<<15)) >> 16;
    int tmp2 = (E(matrix_int,1,0) * x1 + E(matrix_int,1,1) * x2 + E(matrix_int,1,2) * x3 + E(matrix_int,1,3) + (1<<15)) >> 16;
    int tmp3 = (E(matrix_int,2,0) * x1 + E(matrix_int,2,1) * x2 + E(matrix_int,2,2) * x3 + E(matrix_int,2,3) + (1<<15)) >> 16;
    tmp1 = clip(tmp1, 255);
    tmp2 = clip(tmp2, 255);
    tmp3 = clip(tmp3, 255);
    return (tmp1<<16) | (tmp2<<8) | tmp3;
}

//////////////////////////////////////////////////////////////////////////

const int FRACTION_BITS  = 16;
const int FRACTION_SCALE = 1<<16;

struct RGBLevelInfo
{
    int low, size;
};
const RGBLevelInfo RGB_LVL_PC = {  0, 255 };
const RGBLevelInfo RGB_LVL_TV = { 16, 219 };

struct YUVLevelInfo
{
    int y_low, y_size;
    int u_mid, u_size;
};
const YUVLevelInfo YUV_LVL_PC = { 0, 255, 128, 255 };
const YUVLevelInfo YUV_LVL_TV = {16, 219, 128, 224 };

#define FLOAT_TO_FIXED(f, SCALE) int((f)*(SCALE)+0.5)

#define DEFINE_RGB2YUV_FUNC(func, RGB_LEVEL, YUV_LEVEL, Kr, Kg, Kb, YUV_POS)                 \
DWORD func(int r8, int g8, int b8)                                                           \
{                                                                                            \
    r8 -= RGB_LEVEL.low;                                                                     \
    g8 -= RGB_LEVEL.low;                                                                     \
    b8 -= RGB_LEVEL.low;                                                                     \
    const int INT_Kr = int(Kr*FRACTION_SCALE+0.5);                                           \
    const int INT_Kg = int(Kg*FRACTION_SCALE+0.5);                                           \
    const int INT_Kb = int(Kb*FRACTION_SCALE+0.5);                                           \
    const int Y_CU   = int(0.5/(1-Kb)*4096+0.5);                                             \
    const int Y_CV   = int(0.5/(1-Kr)*4096+0.5);                                             \
    const int Y_SCALE= int(1.0*YUV_LEVEL.y_size/RGB_LEVEL.size*4096+0.5);                    \
    const int U_SCALE= int(1.0*YUV_LEVEL.u_size/RGB_LEVEL.size*4096+0.5);                    \
                                                                                             \
    int y = INT_Kr*r8 + INT_Kg*g8 + INT_Kb*b8;                                               \
    int u = (((b8<<FRACTION_BITS) - y) >> 12) * Y_CU;                                        \
    int v = (((r8<<FRACTION_BITS) - y) >> 12) * Y_CV;                                        \
    y = Y_SCALE == 4096 ? y : (y>>12)*Y_SCALE;                                               \
    u = U_SCALE == 4096 ? u : (u>>12)*U_SCALE;                                               \
    v = U_SCALE == 4096 ? v : (v>>12)*U_SCALE;                                               \
    y = (y + (YUV_LEVEL.y_low*FRACTION_SCALE + FRACTION_SCALE/2))>>FRACTION_BITS;            \
    u = (u + (YUV_LEVEL.u_mid*FRACTION_SCALE + FRACTION_SCALE/2))>>FRACTION_BITS;            \
    v = (v + (YUV_LEVEL.u_mid*FRACTION_SCALE + FRACTION_SCALE/2))>>FRACTION_BITS;            \
    y = clip(y, 255);                                                                        \
    u = clip(u, 255);                                                                        \
    v = clip(v, 255);                                                                        \
    return (y<<YUV_POS.y) | (u<<YUV_POS.u) | (v<<YUV_POS.v);                                 \
}

#define DEFINE_YUV2RGB_FUNC(func, RGB_LEVEL, YUV_LEVEL, Kr, Kg, Kb)                       \
DWORD func(int y8, int u8, int v8)                                                        \
{                                                                                         \
    const int Y_SCALE= int(1.0*RGB_LEVEL.size/YUV_LEVEL.y_size* FRACTION_SCALE      +0.5);\
    const int U_SCALE= int(1.0*RGB_LEVEL.size/YUV_LEVEL.u_size*(FRACTION_SCALE/4096)+0.5);\
    y8 = (y8-YUV_LEVEL.y_low)*Y_SCALE;                                                    \
    u8 = (u8-YUV_LEVEL.u_mid)*U_SCALE;                                                    \
    v8 = (v8-YUV_LEVEL.u_mid)*U_SCALE;                                                    \
    const int INT_RV = int( 2*(1-Kr)      *4096+0.5);                                     \
    const int INT_GU = int(-2*(1-Kb)*Kb/Kg*4096+0.5);                                     \
    const int INT_GV = int(-2*(1-Kr)*Kr/Kg*4096+0.5);                                     \
    const int INT_BU = int( 2*(1-Kb)      *4096+0.5);                                     \
                                                                                          \
    int r = (y8 +         0 + INT_RV*v8 + FRACTION_SCALE/2)>>FRACTION_BITS;               \
    int g = (y8 + INT_GU*u8 + INT_GV*v8 + FRACTION_SCALE/2)>>FRACTION_BITS;               \
    int b = (y8 + INT_BU*u8 + 0         + FRACTION_SCALE/2)>>FRACTION_BITS;               \
    r = clip(r, RGB_LEVEL.size);                                                          \
    g = clip(g, RGB_LEVEL.size);                                                          \
    b = clip(b, RGB_LEVEL.size);                                                          \
    r += RGB_LEVEL.low;                                                                   \
    g += RGB_LEVEL.low;                                                                   \
    b += RGB_LEVEL.low;                                                                   \
    return (r<<16) | (g<<8) | b;                                                          \
}

#define DEFINE_PREMUL_ARGB2AYUV_FUNC(func, RGB_LEVEL, YUV_LEVEL, Kr, Kg, Kb, YUV_POS)     \
DWORD func(int a8, int r8, int g8, int b8)                                                \
{                                                                                         \
    r8 -= RGB_LEVEL.low;                                                                  \
    g8 -= RGB_LEVEL.low;                                                                  \
    b8 -= RGB_LEVEL.low;                                                                  \
    const int INT_Kr = int(Kr*FRACTION_SCALE+0.5);                                        \
    const int INT_Kg = int(Kg*FRACTION_SCALE+0.5);                                        \
    const int INT_Kb = int(Kb*FRACTION_SCALE+0.5);                                        \
    const int Y_CU   = int(0.5/(1-Kb)*4096+0.5);                                          \
    const int Y_CV   = int(0.5/(1-Kr)*4096+0.5);                                          \
    const int Y_SCALE= int(1.0*YUV_LEVEL.y_size/RGB_LEVEL.size*4096+0.5);                 \
    const int U_SCALE= int(1.0*YUV_LEVEL.u_size/RGB_LEVEL.size*4096+0.5);                 \
                                                                                          \
    int a = (256-a8)<<(FRACTION_BITS-8);                                                  \
    int y = INT_Kr*r8 + INT_Kg*g8 + INT_Kb*b8;                                            \
    int u = (((b8<<FRACTION_BITS) - y) >> 12) * Y_CU;                                     \
    int v = (((r8<<FRACTION_BITS) - y) >> 12) * Y_CV;                                     \
    y = Y_SCALE == 4096 ? y : (y>>12)*Y_SCALE;                                            \
    u = U_SCALE == 4096 ? u : (u>>12)*U_SCALE;                                            \
    v = U_SCALE == 4096 ? v : (v>>12)*U_SCALE;                                            \
    y = (y + YUV_LEVEL.y_low*a  + FRACTION_SCALE/2)>>FRACTION_BITS;                       \
    u = (u + YUV_LEVEL.u_mid*a  + FRACTION_SCALE/2)>>FRACTION_BITS;                       \
    v = (v + YUV_LEVEL.u_mid*a  + FRACTION_SCALE/2)>>FRACTION_BITS;                       \
    y = clip(y, 255);                                                                     \
    u = clip(u, 255);                                                                     \
    v = clip(v, 255);                                                                     \
    return (y<<YUV_POS.y) | (u<<YUV_POS.u) | (v<<YUV_POS.v);                              \
}

#define DEFINE_RGB2Y_FUNC(func, RGB_LEVEL, YUV_LEVEL, Kr, Kg, Kb)                            \
DWORD func(int r8, int g8, int b8)                                                           \
{                                                                                            \
    const int INT_Kr = int(Kr*FRACTION_SCALE+0.5);                                           \
    const int INT_Kg = int(Kg*FRACTION_SCALE+0.5);                                           \
    const int INT_Kb = int(Kb*FRACTION_SCALE+0.5);                                           \
    const int Y_CU   = int(0.5/(1-Kb)*4096+0.5);                                             \
    const int Y_CV   = int(0.5/(1-Kr)*4096+0.5);                                             \
    const int Y_SCALE= int(1.0*YUV_LEVEL.y_size/RGB_LEVEL.size*4096+0.5);                    \
    const int U_SCALE= int(1.0*YUV_LEVEL.u_size/RGB_LEVEL.size*4096+0.5);                    \
                                                                                             \
    int y = INT_Kr*r8 + INT_Kg*g8 + INT_Kb*b8 - RGB_LEVEL.low;                               \
    y = Y_SCALE == 4096 ? y : (y>>12)*Y_SCALE;                                               \
    y = (y + (YUV_LEVEL.y_low*FRACTION_SCALE + FRACTION_SCALE/2))>>FRACTION_BITS;            \
    y = clip(y, 255);                                                                        \
    return y;                                                                                \
}

DWORD RGBToYUV_TV_BT601(int r8, int g8, int b8);
DWORD RGBToYUV_PC_BT601(int r8, int g8, int b8);
DWORD RGBToYUV_TV_BT709(int r8, int g8, int b8);
DWORD RGBToYUV_PC_BT709(int r8, int g8, int b8);

DWORD RGBToUYV_TV_BT601(int r8, int g8, int b8);
DWORD RGBToUYV_PC_BT601(int r8, int g8, int b8);
DWORD RGBToUYV_TV_BT709(int r8, int g8, int b8);
DWORD RGBToUYV_PC_BT709(int r8, int g8, int b8);

DWORD PREMUL_ARGB2AYUV_TV_BT601(int a8, int r8, int g8, int b8);
DWORD PREMUL_ARGB2AYUV_PC_BT601(int a8, int r8, int g8, int b8);
DWORD PREMUL_ARGB2AYUV_TV_BT709(int a8, int r8, int g8, int b8);
DWORD PREMUL_ARGB2AYUV_PC_BT709(int a8, int r8, int g8, int b8);

DWORD RGBToY_TV_BT601(int r8, int g8, int b8);
DWORD RGBToY_PC_BT601(int r8, int g8, int b8);
DWORD RGBToY_TV_BT709(int r8, int g8, int b8);
DWORD RGBToY_PC_BT709(int r8, int g8, int b8);

DWORD YUVToRGB_TV_BT601(int y, int u, int v);
DWORD YUVToRGB_PC_BT601(int y, int u, int v);
DWORD YUVToRGB_TV_BT709(int y, int u, int v);
DWORD YUVToRGB_PC_BT709(int y, int u, int v);

typedef ColorConvTable::YuvMatrixType YuvMatrixType;
typedef ColorConvTable::YuvRangeType YuvRangeType;

class ConvFunc
{
public:
    ConvFunc(YuvMatrixType yuv_type, YuvRangeType range);
    bool InitConvFunc(YuvMatrixType yuv_type, YuvRangeType range);

    typedef DWORD (*R8G8B8ToYuvFunc)(int r8, int g8, int b8);
    typedef DWORD (*PreMulArgbToAyuvFunc)(int a8, int r8, int g8, int b8);
    typedef R8G8B8ToYuvFunc R8G8B8ToY;
    typedef R8G8B8ToYuvFunc Y8U8V8ToRGBFunc;

    R8G8B8ToYuvFunc      r8g8b8_to_yuv_func;
    R8G8B8ToYuvFunc      r8g8b8_to_uyv_func;
    PreMulArgbToAyuvFunc pre_mul_argb_to_ayuv_func;
    R8G8B8ToY            r8g8b8_to_y_func;
    Y8U8V8ToRGBFunc      y8u8v8_to_rgb_func;

    YuvMatrixType _yuv_type;
    YuvRangeType  _range_type;

    ConvMatrix    _conv_matrix;//for YUV to YUV or other complicated conversions
};

ConvFunc s_default_conv_set(ColorConvTable::BT601, ColorConvTable::RANGE_TV);

bool ConvFunc::InitConvFunc(YuvMatrixType yuv_type, YuvRangeType range)
{
    bool result = true;

    if ( yuv_type==ColorConvTable::BT601 && range==ColorConvTable::RANGE_TV )
    {
        r8g8b8_to_yuv_func        = RGBToYUV_TV_BT601;
        r8g8b8_to_uyv_func        = RGBToUYV_TV_BT601;
        pre_mul_argb_to_ayuv_func = PREMUL_ARGB2AYUV_TV_BT601;
        r8g8b8_to_y_func          = RGBToY_TV_BT601;
        y8u8v8_to_rgb_func        = YUVToRGB_TV_BT601;

        _yuv_type   = yuv_type;
        _range_type = range;
    }
    else if ( yuv_type==ColorConvTable::BT709 && range==ColorConvTable::RANGE_TV )
    {
        r8g8b8_to_yuv_func        = RGBToYUV_TV_BT709;
        r8g8b8_to_uyv_func        = RGBToUYV_TV_BT709;
        pre_mul_argb_to_ayuv_func = PREMUL_ARGB2AYUV_TV_BT709;
        r8g8b8_to_y_func          = RGBToY_TV_BT709;
        y8u8v8_to_rgb_func        = YUVToRGB_TV_BT709;

        _yuv_type   = yuv_type;
        _range_type = range;
    }
    else if ( yuv_type==ColorConvTable::BT601 && range==ColorConvTable::RANGE_PC )
    {
        r8g8b8_to_yuv_func        = RGBToYUV_PC_BT601;
        r8g8b8_to_uyv_func        = RGBToUYV_PC_BT601;
        pre_mul_argb_to_ayuv_func = PREMUL_ARGB2AYUV_PC_BT601;
        r8g8b8_to_y_func          = RGBToY_PC_BT601;
        y8u8v8_to_rgb_func        = YUVToRGB_PC_BT601;

        _yuv_type   = yuv_type;
        _range_type = range;
    }
    else if ( yuv_type==ColorConvTable::BT709 && range==ColorConvTable::RANGE_PC )
    {
        r8g8b8_to_yuv_func        = RGBToYUV_PC_BT709;
        r8g8b8_to_uyv_func        = RGBToUYV_PC_BT709;
        pre_mul_argb_to_ayuv_func = PREMUL_ARGB2AYUV_PC_BT709;
        r8g8b8_to_y_func          = RGBToY_PC_BT709;
        y8u8v8_to_rgb_func        = YUVToRGB_PC_BT709;

        _yuv_type   = yuv_type;
        _range_type = range;
    }
    else
    {
        r8g8b8_to_yuv_func        = RGBToYUV_TV_BT601;
        r8g8b8_to_uyv_func        = RGBToUYV_TV_BT601;
        pre_mul_argb_to_ayuv_func = PREMUL_ARGB2AYUV_TV_BT601;
        r8g8b8_to_y_func          = RGBToY_TV_BT601;
        y8u8v8_to_rgb_func        = YUVToRGB_TV_BT601;

        _yuv_type   = ColorConvTable::BT601;
        _range_type = ColorConvTable::RANGE_TV;
    }

    return result;
}

ConvFunc::ConvFunc( YuvMatrixType yuv_type, YuvRangeType range )
{
    _conv_matrix.Init();
    _conv_matrix.InitMatrix(
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_601,
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_709);
    _conv_matrix.InitMatrix(
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_601,
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_709);
    _conv_matrix.InitMatrix(
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_601,
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_709);
    _conv_matrix.InitMatrix(
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_601,
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_709);

    _conv_matrix.InitMatrix(
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_709,
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_601);
    _conv_matrix.InitMatrix(
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_709,
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_601);
    _conv_matrix.InitMatrix(
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_709,
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_601);
    _conv_matrix.InitMatrix(
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_709,
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_601);
    InitConvFunc(yuv_type, range);
}

//
// ColorConvTable
//
ColorConvTable::YuvMatrixType ColorConvTable::GetDefaultYUVType()
{
    return s_default_conv_set._yuv_type;
}

ColorConvTable::YuvRangeType ColorConvTable::GetDefaultRangeType()
{
    return s_default_conv_set._range_type;
}

void ColorConvTable::SetDefaultConvType( YuvMatrixType yuv_type, YuvRangeType range )
{
    if( s_default_conv_set._yuv_type != yuv_type || s_default_conv_set._range_type != range )
    {
        s_default_conv_set.InitConvFunc(yuv_type, range);
    }
}

DWORD ColorConvTable::Argb2Auyv( DWORD argb )
{
    int r = (argb & 0x00ff0000) >> 16;
    int g = (argb & 0x0000ff00) >> 8;
    int b = (argb & 0x000000ff);
    return (argb & 0xff000000) | s_default_conv_set.r8g8b8_to_uyv_func(r, g, b);
}

DWORD ColorConvTable::Argb2Ayuv( DWORD argb )
{
    int r = (argb & 0x00ff0000) >> 16;
    int g = (argb & 0x0000ff00) >> 8;
    int b = (argb & 0x000000ff);
    return (argb & 0xff000000) | s_default_conv_set.r8g8b8_to_yuv_func(r, g, b);
}

DWORD ColorConvTable::Argb2Ayuv_TV_BT601( DWORD argb )
{
    int r = (argb & 0x00ff0000) >> 16;
    int g = (argb & 0x0000ff00) >> 8;
    int b = (argb & 0x000000ff);
    return (argb & 0xff000000) | RGBToYUV_TV_BT601(r, g, b);
}

DWORD ColorConvTable::Ayuv2Auyv( DWORD ayuv )
{
    int y = (ayuv & 0x00ff0000) >> 8;
    int u = (ayuv & 0x0000ff00) << 8;
    return (ayuv & 0xff0000ff)| u | y;
}

DWORD ColorConvTable::PreMulArgb2Ayuv( int a8, int r8, int g8, int b8 )
{
    return s_default_conv_set.pre_mul_argb_to_ayuv_func(a8, r8, g8, b8);
}

DWORD ColorConvTable::Rgb2Y( int r8, int g8, int b8 )
{
    return s_default_conv_set.r8g8b8_to_y_func(r8, g8, b8);
}

DWORD ColorConvTable::Ayuv2Argb_TV_BT601( DWORD ayuv )
{
    int y = (ayuv & 0x00ff0000) >> 16;
    int u = (ayuv & 0x0000ff00) >> 8;
    int v = (ayuv & 0x000000ff);
    return (ayuv & 0xff000000) | YUVToRGB_TV_BT601(y, u, v);
}

DWORD ColorConvTable::Ayuv2Argb_TV_BT709( DWORD ayuv )
{
    int y = (ayuv & 0x00ff0000) >> 16;
    int u = (ayuv & 0x0000ff00) >> 8;
    int v = (ayuv & 0x000000ff);
    return (ayuv & 0xff000000) | YUVToRGB_TV_BT709(y, u, v);
}

DWORD ColorConvTable::Ayuv2Argb( DWORD ayuv )
{
    int y = (ayuv & 0x00ff0000) >> 16;
    int u = (ayuv & 0x0000ff00) >> 8;
    int v = (ayuv & 0x000000ff);
    return (ayuv & 0xff000000) | s_default_conv_set.y8u8v8_to_rgb_func(y, u, v);
}

DWORD ColorConvTable::A8Y8U8V8_To_ARGB_TV_BT601( int a8, int y8, int u8, int v8 )
{
    return (a8<<24) | YUVToRGB_TV_BT601(y8, u8, v8);
}

DWORD ColorConvTable::A8Y8U8V8_To_ARGB_PC_BT601( int a8, int y8, int u8, int v8 )
{
    return (a8<<24) | YUVToRGB_PC_BT601(y8, u8, v8);
}

DWORD ColorConvTable::A8Y8U8V8_To_ARGB_TV_BT709( int a8, int y8, int u8, int v8 )
{
    return (a8<<24) | YUVToRGB_TV_BT709(y8, u8, v8);
}

DWORD ColorConvTable::A8Y8U8V8_To_ARGB_PC_BT709( int a8, int y8, int u8, int v8 )
{
    return (a8<<24) | YUVToRGB_PC_BT709(y8, u8, v8);
}

DWORD ColorConvTable::A8Y8U8V8_PC_To_TV( int a8, int y8, int u8, int v8 )
{
    const int FRACTION_SCALE = 1<<16;
    const int YUV_MIN = 16;
    const int cy = int(219.0/255*FRACTION_SCALE+0.5);
    const int cuv = int(224.0/255*FRACTION_SCALE+0.5);
    y8 = ((y8*cy)>>16) + YUV_MIN;
    u8 = ((u8*cuv)>>16) + YUV_MIN;
    v8 = ((v8*cuv)>>16) + YUV_MIN;
    return (a8<<24) | (y8<<16) | (u8<<8) | v8;
}

DWORD ColorConvTable::A8Y8U8V8_TV_To_PC( int a8, int y8, int u8, int v8 )
{
    const int FRACTION_SCALE = 1<<16;
    const int YUV_MIN = 16;
    const int cy = int(255/219.0*FRACTION_SCALE+0.5);
    const int cuv = int(255/224.0*FRACTION_SCALE+0.5);
    y8 = ((y8-YUV_MIN)*cy)>>16;
    u8 = ((u8-YUV_MIN)*cuv)>>16;
    v8 = ((v8-YUV_MIN)*cuv)>>16;
    return (a8<<24) | (y8<<16) | (u8<<8) | v8;
}

DWORD ColorConvTable::RGB_PC_TO_TV( DWORD argb )
{
    const int MIN = 16;
    const int SCALE = int(219.0/255*FRACTION_SCALE+0.5);
    DWORD r = (argb & 0x00ff0000)>>16;
    DWORD g = (argb & 0x0000ff00)>>8;
    DWORD b = (argb & 0x000000ff);
    r = ((r*SCALE)>>16) + MIN;
    g = ((g*SCALE)>>16) + MIN;
    b = ((b*SCALE)>>16) + MIN;
    return (argb & 0xff000000)|(r<<16)|(g<<8)|b;
}

DWORD ColorConvTable::A8Y8U8V8_TO_AYUV( int a8, int y8, int u8, int v8,
    YuvRangeType in_range, YuvMatrixType in_type, YuvRangeType out_range, YuvMatrixType out_type )
{
    const int level_map[3] = {
        ConvMatrix::LEVEL_TV,
        ConvMatrix::LEVEL_TV,
        ConvMatrix::LEVEL_PC
    };
    const int type_map[3] = {
        ConvMatrix::COLOR_YUV_601,
        ConvMatrix::COLOR_YUV_601,
        ConvMatrix::COLOR_YUV_709

    };
    //level_map[ColorConvTable::RANGE_NONE] = ConvMatrix::LEVEL_TV;
    //level_map[ColorConvTable::RANGE_TV]   = ConvMatrix::LEVEL_TV;
    //level_map[ColorConvTable::RANGE_PC]   = ConvMatrix::LEVEL_PC;
    //type_map[ColorConvTable::NONE]        = ConvMatrix::COLOR_YUV_601;
    //type_map[ColorConvTable::BT601]       = ConvMatrix::COLOR_YUV_601;
    //type_map[ColorConvTable::BT709]       = ConvMatrix::COLOR_YUV_709;
    if (in_type==out_type)
    {
        if (in_range==RANGE_PC && out_range==RANGE_TV)
        {
            return A8Y8U8V8_PC_To_TV(a8,y8,u8,v8);
        }
        else if (in_range==RANGE_TV && out_range==RANGE_PC)
        {
            return A8Y8U8V8_TV_To_PC(a8,y8,u8,v8);
        }
        else
        {
            return (a8<<24) | (y8<<16) | (u8<<8) | v8;
        }
    }
    return (a8<<24) | s_default_conv_set._conv_matrix.Convert(y8, u8, v8, 
        level_map[in_range], type_map[in_type], level_map[out_range], type_map[out_type]);
}

DWORD ColorConvTable::A8Y8U8V8_TO_CUR_AYUV( int a8, int y8, int u8, int v8, YuvRangeType in_range, YuvMatrixType in_type )
{
    return A8Y8U8V8_TO_AYUV(a8,y8,u8,v8,in_range,in_type,
        s_default_conv_set._range_type,s_default_conv_set._yuv_type);
}

struct YuvPos
{
    int y;
    int u;
    int v;
};
const YuvPos POS_YUV = {16, 8, 0};
const YuvPos POS_UYV = {8, 16, 0};


DEFINE_RGB2YUV_FUNC(RGBToYUV_TV_BT601, RGB_LVL_PC, YUV_LVL_TV, 0.299 , 0.587 , 0.114 , POS_YUV)
DEFINE_RGB2YUV_FUNC(RGBToYUV_PC_BT601, RGB_LVL_PC, YUV_LVL_PC, 0.299 , 0.587 , 0.114 , POS_YUV)
DEFINE_RGB2YUV_FUNC(RGBToYUV_TV_BT709, RGB_LVL_PC, YUV_LVL_TV, 0.2126, 0.7152, 0.0722, POS_YUV)
DEFINE_RGB2YUV_FUNC(RGBToYUV_PC_BT709, RGB_LVL_PC, YUV_LVL_PC, 0.2126, 0.7152, 0.0722, POS_YUV)

DEFINE_RGB2YUV_FUNC(RGBToUYV_TV_BT601, RGB_LVL_PC, YUV_LVL_TV, 0.299 , 0.587 , 0.114 , POS_UYV)
DEFINE_RGB2YUV_FUNC(RGBToUYV_PC_BT601, RGB_LVL_PC, YUV_LVL_PC, 0.299 , 0.587 , 0.114 , POS_UYV)
DEFINE_RGB2YUV_FUNC(RGBToUYV_TV_BT709, RGB_LVL_PC, YUV_LVL_TV, 0.2126, 0.7152, 0.0722, POS_UYV)
DEFINE_RGB2YUV_FUNC(RGBToUYV_PC_BT709, RGB_LVL_PC, YUV_LVL_PC, 0.2126, 0.7152, 0.0722, POS_UYV)

DEFINE_YUV2RGB_FUNC(YUVToRGB_TV_BT601, RGB_LVL_PC, YUV_LVL_TV, 0.299 , 0.587 , 0.114 )
DEFINE_YUV2RGB_FUNC(YUVToRGB_PC_BT601, RGB_LVL_PC, YUV_LVL_PC, 0.299 , 0.587 , 0.114 )
DEFINE_YUV2RGB_FUNC(YUVToRGB_TV_BT709, RGB_LVL_PC, YUV_LVL_TV, 0.2126, 0.7152, 0.0722)
DEFINE_YUV2RGB_FUNC(YUVToRGB_PC_BT709, RGB_LVL_PC, YUV_LVL_PC, 0.2126, 0.7152, 0.0722)

DEFINE_PREMUL_ARGB2AYUV_FUNC(PREMUL_ARGB2AYUV_TV_BT601, RGB_LVL_PC, YUV_LVL_TV, 0.299 , 0.587 , 0.114 , POS_YUV)
DEFINE_PREMUL_ARGB2AYUV_FUNC(PREMUL_ARGB2AYUV_PC_BT601, RGB_LVL_PC, YUV_LVL_PC, 0.299 , 0.587 , 0.114 , POS_YUV)
DEFINE_PREMUL_ARGB2AYUV_FUNC(PREMUL_ARGB2AYUV_TV_BT709, RGB_LVL_PC, YUV_LVL_TV, 0.2126, 0.7152, 0.0722, POS_YUV)
DEFINE_PREMUL_ARGB2AYUV_FUNC(PREMUL_ARGB2AYUV_PC_BT709, RGB_LVL_PC, YUV_LVL_PC, 0.2126, 0.7152, 0.0722, POS_YUV)

DEFINE_RGB2Y_FUNC(RGBToY_TV_BT601, RGB_LVL_PC, YUV_LVL_TV, 0.299 , 0.587 , 0.114 )
DEFINE_RGB2Y_FUNC(RGBToY_PC_BT601, RGB_LVL_PC, YUV_LVL_PC, 0.299 , 0.587 , 0.114 )
DEFINE_RGB2Y_FUNC(RGBToY_TV_BT709, RGB_LVL_PC, YUV_LVL_TV, 0.2126, 0.7152, 0.0722)
DEFINE_RGB2Y_FUNC(RGBToY_PC_BT709, RGB_LVL_PC, YUV_LVL_PC, 0.2126, 0.7152, 0.0722)
