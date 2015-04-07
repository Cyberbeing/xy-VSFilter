#ifndef __XY_WIDEN_REGOIN_ECAEEA0A_9D51_4284_B0AE_081AF0E75438_H__
#define __XY_WIDEN_REGOIN_ECAEEA0A_9D51_4284_B0AE_081AF0E75438_H__

class WidenRegionCreaterImpl;
class WidenRegionCreater
{
public:
    typedef tSpanBuffer SpanBuffer;

public:
    static WidenRegionCreater* GetDefaultWidenRegionCreater();

    void xy_overlap_region(SpanBuffer* dst, const SpanBuffer& src, int rx, int ry);
private:
    WidenRegionCreater();
    ~WidenRegionCreater();

    WidenRegionCreaterImpl* m_impl;
};

#endif // __XY_WIDEN_REGOIN_ECAEEA0A_9D51_4284_B0AE_081AF0E75438_H__