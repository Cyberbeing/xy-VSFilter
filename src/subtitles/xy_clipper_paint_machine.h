#ifndef __XY_CLIPPER_PAINT_MACHINE_98D7A2E7_B2FA_44BC_9678_8B27CE8EB9DB_H__
#define __XY_CLIPPER_PAINT_MACHINE_98D7A2E7_B2FA_44BC_9678_8B27CE8EB9DB_H__

#include <boost/shared_ptr.hpp>

#pragma warning(push)
#pragma warning (disable: 4231)
class CClipper;
extern template class ::boost::shared_ptr<CClipper>;
typedef ::boost::shared_ptr<CClipper> SharedPtrCClipper;

struct GrayImage2;
extern template class ::boost::shared_ptr<GrayImage2>;
typedef ::boost::shared_ptr<GrayImage2> SharedPtrGrayImage2;
#pragma warning(pop)

class ClipperAlphaMaskCacheKey;

class CClipperPaintMachine
{
public:
    CClipperPaintMachine(const SharedPtrCClipper& clipper)
        : m_clipper(clipper)
        , m_hash_key(clipper){ m_hash_key.UpdateHashValue(); }

    void Paint(SharedPtrGrayImage2* output);
    CRect CalcDirtyRect();
    const ClipperAlphaMaskCacheKey& GetHashKey();
private:
    SharedPtrCClipper m_clipper;
    ClipperAlphaMaskCacheKey m_hash_key;
};

#endif // __XY_CLIPPER_PAINT_MACHINE_98D7A2E7_B2FA_44BC_9678_8B27CE8EB9DB_H__
