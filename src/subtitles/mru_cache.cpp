/************************************************************************/
/* author: xy                                                           */
/* date: 20130503                                                       */
/************************************************************************/

#include "stdafx.h"
#include "mru_cache.h"

XyAtlMapHelper::XyAtlMapHelper(
    _In_ UINT nBins,
    _In_ float fOptimalLoad,
    _In_ float fLoThreshold,
    _In_ float fHiThreshold,
    _In_ UINT nBlockSize) throw()
    : m_ppBins( NULL ),
    m_nBins( nBins ),
    m_nElements( 0 ),
    m_nLockCount( 0 ),  // Start unlocked
    m_fOptimalLoad( fOptimalLoad ),
    m_fLoThreshold( fLoThreshold ),
    m_fHiThreshold( fHiThreshold ),
    m_nHiRehashThreshold( UINT_MAX ),
    m_nLoRehashThreshold( 0 ),
    m_pBlocks( NULL ),
    m_pFree( NULL ),
    m_nBlockSize( nBlockSize )
{

}

POSITION XyAtlMapHelper::GetStartPosition() const throw()
{
    if( IsEmpty() )
    {
        return( NULL );
    }

    for( UINT iBin = 0; iBin < m_nBins; iBin++ )
    {
        if( m_ppBins[iBin] != NULL )
        {
            return( POSITION( m_ppBins[iBin] ) );
        }
    }
    ATLASSERT( false );

    return( NULL );
}

UINT XyAtlMapHelper::PickSize(_In_ size_t nElements) const throw()
{
    // List of primes such that s_anPrimes[i] is the smallest prime greater than 2^(5+i/3)
    static const UINT s_anPrimes[] =
    {
        17, 23, 29, 37, 41, 53, 67, 83, 103, 131, 163, 211, 257, 331, 409, 521, 647, 821,
        1031, 1291, 1627, 2053, 2591, 3251, 4099, 5167, 6521, 8209, 10331,
        13007, 16411, 20663, 26017, 32771, 41299, 52021, 65537, 82571, 104033,
        131101, 165161, 208067, 262147, 330287, 416147, 524309, 660563,
        832291, 1048583, 1321139, 1664543, 2097169, 2642257, 3329023, 4194319,
        5284493, 6658049, 8388617, 10568993, 13316089, UINT_MAX
    };

    size_t nBins = (size_t)(nElements/m_fOptimalLoad);
    UINT nBinsEstimate = UINT(  UINT_MAX < nBins ? UINT_MAX : nBins );

    // Find the smallest prime greater than our estimate
    int iPrime = 0;
    while( nBinsEstimate > s_anPrimes[iPrime] )
    {
        iPrime++;
    }

    if( s_anPrimes[iPrime] == UINT_MAX )
    {
        return( nBinsEstimate );
    }
    else
    {
        return( s_anPrimes[iPrime] );
    }
}

void XyAtlMapHelper::FreePlexes() throw()
{
    m_pFree = NULL;
    if( m_pBlocks != NULL )
    {
        m_pBlocks->FreeDataChain();
        m_pBlocks = NULL;
    }
}

void XyAtlMapHelper::UpdateRehashThresholds() throw()
{
    m_nHiRehashThreshold = size_t( m_fHiThreshold*m_nBins );
    m_nLoRehashThreshold = size_t( m_fLoThreshold*m_nBins );
    if( m_nLoRehashThreshold < 17 )
    {
        m_nLoRehashThreshold = 0;
    }
}

bool XyAtlMapHelper::InitHashTable(_In_ UINT nBins, _In_ bool bAllocNow)
{
    ATLASSUME( m_nElements == 0 );
    ATLASSERT( nBins > 0 );

    if( m_ppBins != NULL )
    {
        delete[] m_ppBins;
        m_ppBins = NULL;
    }

    if( bAllocNow )
    {
        ATLTRY( m_ppBins = new PNode[nBins] );
        if( m_ppBins == NULL )
        {
            return false;
        }

        ATLENSURE( UINT_MAX / sizeof( PNode ) >= nBins );
        memset( m_ppBins, 0, sizeof( PNode )*nBins );
    }
    m_nBins = nBins;

    UpdateRehashThresholds();

    return true;
}

#ifdef _DEBUG
void XyAtlMapHelper::AssertValid() const
{
    ATLASSUME( m_nBins > 0 );
    // non-empty map should have hash table
    ATLASSERT( IsEmpty() || (m_ppBins != NULL) );
}
#endif
