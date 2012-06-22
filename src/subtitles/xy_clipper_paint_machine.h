#ifndef __XY_CLIPPER_PAINT_MACHINE_98D7A2E7_B2FA_44BC_9678_8B27CE8EB9DB_H__
#define __XY_CLIPPER_PAINT_MACHINE_98D7A2E7_B2FA_44BC_9678_8B27CE8EB9DB_H__

#include <boost/shared_ptr.hpp>

class CClipper;
typedef ::boost::shared_ptr<CClipper> SharedPtrCClipper;

struct GrayImage2;
typedef ::boost::shared_ptr<GrayImage2> SharedPtrGrayImage2;

class CClipperPaintMachine
{
public:
    CClipperPaintMachine(const SharedPtrCClipper& clipper)
        : m_clipper(clipper){}
  
    void Paint(SharedPtrGrayImage2* output);
private:
    SharedPtrCClipper m_clipper;
};

#endif // __XY_CLIPPER_PAINT_MACHINE_98D7A2E7_B2FA_44BC_9678_8B27CE8EB9DB_H__
