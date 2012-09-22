#ifndef __XY_VSFILTER_530DDFFF_41AD_450B_A92B_0EA375778509_H__
#define __XY_VSFILTER_530DDFFF_41AD_450B_A92B_0EA375778509_H__

class CDirectVobSubFilter;

[uuid("285ef3a6-c9f7-4be5-837a-20e5c8e75f91")]
class XyVSFilter
{
public:
    void AddSubStream(ISubStream* pSubStream);
    void RemoveSubStream(ISubStream* pSubStream);
    void InvalidateSubtitle(REFERENCE_TIME rtInvalidate = -1, DWORD_PTR nSubtitleId = -1);
private:
    XyVSFilter(CDirectVobSubFilter* pFilter, CCritSec* pLock, CCritSec* pSubLock, HRESULT* phr);

    CDirectVobSubFilter* m_pDVS;
};


#endif // __XY_VSFILTER_530DDFFF_41AD_450B_A92B_0EA375778509_H__
