#include "stdafx.h"
#include "xy_vsfilter.h"
#include "DirectVobSubFilter.h"

XyVSFilter::XyVSFilter(CDirectVobSubFilter* pFilter)
    : m_pDVS(pFilter)
{
}

void XyVSFilter::AddSubStream(ISubStream* pSubStream)
{
    m_pDVS->AddSubStream(pSubStream);
}

void XyVSFilter::RemoveSubStream(ISubStream* pSubStream)
{
    m_pDVS->RemoveSubStream(pSubStream);
}

void XyVSFilter::InvalidateSubtitle(REFERENCE_TIME rtStart, ISubStream* pSubStream)
{
    m_pDVS->InvalidateSubtitle(rtStart, (DWORD_PTR)(ISubStream*)pSubStream);
}

