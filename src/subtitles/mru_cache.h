/************************************************************************/
/* author: xy                                                           */
/* date: 20110918                                                       */
/************************************************************************/
#ifndef __MRU_CACHE_H_256FCF72_8663_41DC_B98A_B822F6007912__
#define __MRU_CACHE_H_256FCF72_8663_41DC_B98A_B822F6007912__

#include <atlcoll.h>
#include <utility>

////////////////////////////////////////////////////////////////////////////////////////////////////

// XyAtlMap: a modified CAtlMap

class XyAtlMapHelper
{
protected:
    typedef void* PNode;

    XyAtlMapHelper(
        _In_ UINT nBins = 17,
        _In_ float fOptimalLoad = 0.75f,
        _In_ float fLoThreshold = 0.25f,
        _In_ float fHiThreshold = 2.25f,
        _In_ UINT nBlockSize = 10) throw();
public:

    size_t GetCount() const throw();
    bool IsEmpty() const throw();
    
    UINT GetHashTableSize() const throw();

    POSITION GetStartPosition() const throw();

    bool InitHashTable(
        _In_ UINT nBins,
        _In_ bool bAllocNow = true);
    void EnableAutoRehash() throw();
    void DisableAutoRehash() throw();

#ifdef _DEBUG
    void AssertValid() const;
#endif  // _DEBUG

    // Implementation
protected:
    PNode* m_ppBins;
    size_t m_nElements;
    UINT m_nBins;
    float m_fOptimalLoad;
    float m_fLoThreshold;
    float m_fHiThreshold;
    size_t m_nHiRehashThreshold;
    size_t m_nLoRehashThreshold;
    ULONG m_nLockCount;
    UINT m_nBlockSize;
    CAtlPlex* m_pBlocks;
    PNode m_pFree;

protected:
    bool IsLocked() const throw();
    UINT PickSize(_In_ size_t nElements) const throw();

    void FreePlexes() throw();

    void UpdateRehashThresholds() throw();

public:
    ~XyAtlMapHelper() throw(){}

private:
    // Private to prevent use
    XyAtlMapHelper(_In_ const XyAtlMapHelper&) throw();
    XyAtlMapHelper& operator=(_In_ const XyAtlMapHelper&) throw();
};


template< typename K, typename V, class KTraits = CElementTraits< K >, class VTraits = CElementTraits< V > >
class XyAtlMap:public XyAtlMapHelper
{
public:
    typedef typename KTraits::INARGTYPE KINARGTYPE;
    typedef typename KTraits::OUTARGTYPE KOUTARGTYPE;
    typedef typename VTraits::INARGTYPE VINARGTYPE;
    typedef typename VTraits::OUTARGTYPE VOUTARGTYPE;

    class CPair :
        public __POSITION
    {
    protected:
        CPair(/* _In_ */ KINARGTYPE key) :
             m_key( key )
             {
             }

    public:
        const K m_key;
        V m_value;
    };

private:
    class CNode :
        public CPair
    {
    public:
        CNode(
            /* _In_ */ KINARGTYPE key,
            _In_ UINT nHash) :
        CPair( key ),
            m_nHash( nHash )
        {
        }

    public:
        UINT GetHash() const throw()
        {
            return( m_nHash );
        }

    public:
        CNode* m_pNext;
        UINT m_nHash;
    };

public:
    XyAtlMap(
        _In_ UINT nBins = 17,
        _In_ float fOptimalLoad = 0.75f,
        _In_ float fLoThreshold = 0.25f,
        _In_ float fHiThreshold = 2.25f,
        _In_ UINT nBlockSize = 10) throw();


    bool Lookup(
        /* _In_ */ KINARGTYPE key,
        _Out_ VOUTARGTYPE value) const;
    const CPair* Lookup(/* _In_ */ KINARGTYPE key) const throw();
    CPair* Lookup(/* _In_ */ KINARGTYPE key) throw();
    V& operator[](/* _In_ */ KINARGTYPE key) throw(...);

    POSITION SetAtIfNotExists(
        /* _In_ */ KINARGTYPE key, 
        /* _In_ */ VINARGTYPE value,
        bool *new_item_added);

    POSITION SetAt(
        /* _In_ */ KINARGTYPE key,
        /* _In_ */ VINARGTYPE value);
    void SetValueAt(
        _In_ POSITION pos,
        /* _In_ */ VINARGTYPE value);

    bool RemoveKey(/* _In_ */ KINARGTYPE key) throw();
    void RemoveAll();
    void RemoveAtPos(_In_ POSITION pos) throw();

    void GetNextAssoc(
        _Inout_ POSITION& pos,
        _Out_ KOUTARGTYPE key,
        _Out_ VOUTARGTYPE value) const;
    const CPair* GetNext(_Inout_ POSITION& pos) const throw();
    CPair* GetNext(_Inout_ POSITION& pos) throw();
    const K& GetNextKey(_Inout_ POSITION& pos) const;
    const V& GetNextValue(_Inout_ POSITION& pos) const;
    V& GetNextValue(_Inout_ POSITION& pos);
    void GetAt(
        _In_ POSITION pos,
        _Out_ KOUTARGTYPE key,
        _Out_ VOUTARGTYPE value) const;
    CPair* GetAt(_In_ POSITION pos) throw();
    const CPair* GetAt(_In_ POSITION pos) const throw();
    const K& GetKeyAt(_In_ POSITION pos) const;
    const V& GetValueAt(_In_ POSITION pos) const;
    V& GetValueAt(_In_ POSITION pos);

    void Rehash(_In_ UINT nBins = 0);
    void SetOptimalLoad(
        _In_ float fOptimalLoad,
        _In_ float fLoThreshold,
        _In_ float fHiThreshold,
        _In_ bool bRehashNow = false);

#ifdef _DEBUG
#endif  // _DEBUG

private:
    CNode* NewNode(
        /* _In_ */ KINARGTYPE key,
        _In_ UINT iBin,
        _In_ UINT nHash);
    void FreeNode(_Inout_ CNode* pNode);
    CNode* GetNode(
        /* _In_ */ KINARGTYPE key,
        _Out_ UINT& iBin,
        _Out_ UINT& nHash,
        _Deref_out_opt_ CNode*& pPrev) const throw();
    CNode* CreateNode(
        /* _In_ */ KINARGTYPE key,
        _In_ UINT iBin,
        _In_ UINT nHash) throw(...);
    void RemoveNode(
        _In_ CNode* pNode,
        _In_opt_ CNode* pPrev) throw();
    CNode* FindNextNode(_In_ CNode* pNode) const throw();

public:
    ~XyAtlMap() throw();

private:
    // Private to prevent use
    XyAtlMap(_In_ const XyAtlMap&) throw();
    XyAtlMap& operator=(_In_ const XyAtlMap&) throw();
};


inline size_t XyAtlMapHelper::GetCount() const throw()
{
    return( m_nElements );
}

inline bool XyAtlMapHelper::IsEmpty() const throw()
{
    return( m_nElements == 0 );
}

template< typename K, typename V, class KTraits, class VTraits >
inline V& XyAtlMap< K, V, KTraits, VTraits >::operator[](/* _In_ */ KINARGTYPE key) throw(...)
{
    CNode* pNode;
    UINT iBin;
    UINT nHash;
    CNode* pPrev;

    pNode = GetNode( key, iBin, nHash, pPrev );
    if( pNode == NULL )
    {
        pNode = CreateNode( key, iBin, nHash );
    }

    return( pNode->m_value );
}

inline UINT XyAtlMapHelper::GetHashTableSize() const throw()
{
    return( m_nBins );
}

template< typename K, typename V, class KTraits, class VTraits >
inline void XyAtlMap< K, V, KTraits, VTraits >::GetAt(
    _In_ POSITION pos,
    _Out_ KOUTARGTYPE key,
    _Out_ VOUTARGTYPE value) const
{
    ATLENSURE( pos != NULL );

    CNode* pNode = static_cast< CNode* >( pos );

    key = pNode->m_key;
    value = pNode->m_value;
}

template< typename K, typename V, class KTraits, class VTraits >
inline typename XyAtlMap< K, V, KTraits, VTraits >::CPair* XyAtlMap< K, V, KTraits, VTraits >::GetAt(
    _In_ POSITION pos) throw()
{
    ATLASSERT( pos != NULL );

    return( static_cast< CPair* >( pos ) );
}

template< typename K, typename V, class KTraits, class VTraits >
inline const typename XyAtlMap< K, V, KTraits, VTraits >::CPair* XyAtlMap< K, V, KTraits, VTraits >::GetAt(
    _In_ POSITION pos) const throw()
{
    ATLASSERT( pos != NULL );

    return( static_cast< const CPair* >( pos ) );
}

template< typename K, typename V, class KTraits, class VTraits >
inline const K& XyAtlMap< K, V, KTraits, VTraits >::GetKeyAt(_In_ POSITION pos) const
{
    ATLENSURE( pos != NULL );

    CNode* pNode = static_cast<CNode*>(pos);

    return( pNode->m_key );
}

template< typename K, typename V, class KTraits, class VTraits >
inline const V& XyAtlMap< K, V, KTraits, VTraits >::GetValueAt(_In_ POSITION pos) const
{
    ATLENSURE( pos != NULL );

    CNode* pNode = static_cast<CNode*>(pos);

    return( pNode->m_value );
}

template< typename K, typename V, class KTraits, class VTraits >
inline V& XyAtlMap< K, V, KTraits, VTraits >::GetValueAt(_In_ POSITION pos)
{
    ATLENSURE( pos != NULL );

    CNode* pNode = static_cast<CNode*>(pos);

    return( pNode->m_value );
}

inline void XyAtlMapHelper::DisableAutoRehash() throw()
{
    m_nLockCount++;
}

inline void XyAtlMapHelper::EnableAutoRehash() throw()
{
    ATLASSUME( m_nLockCount > 0 );
    m_nLockCount--;
}

inline bool XyAtlMapHelper::IsLocked() const throw()
{
    return( m_nLockCount != 0 );
}

template< typename K, typename V, class KTraits, class VTraits >
typename XyAtlMap< K, V, KTraits, VTraits >::CNode* XyAtlMap< K, V, KTraits, VTraits >::CreateNode(
    /* _In_ */ KINARGTYPE key,
    _In_ UINT iBin,
    _In_ UINT nHash) throw(...)
{
    CNode* pNode;

    if( m_ppBins == NULL )
    {
        bool bSuccess;

        bSuccess = InitHashTable( m_nBins );
        if( !bSuccess )
        {
            AtlThrow( E_OUTOFMEMORY );
        }
    }

    pNode = NewNode( key, iBin, nHash );

    return( pNode );
}

template< typename K, typename V, class KTraits, class VTraits >
POSITION XyAtlMap<K, V, KTraits, VTraits>::SetAtIfNotExists( /* _In_ */ KINARGTYPE key, /* _In_ */ VINARGTYPE value,
    bool *new_item_added)
{
    CNode* pNode;
    UINT iBin;
    UINT nHash;
    CNode* pPrev;

    if (new_item_added)
    {
        *new_item_added = false;
    }

    pNode = GetNode( key, iBin, nHash, pPrev );
    if( pNode == NULL )
    {
        pNode = CreateNode( key, iBin, nHash );
        _ATLTRY
        {
            pNode->m_value = value;
        }
        _ATLCATCHALL()
        {
            RemoveAtPos( POSITION( pNode ) );
            _ATLRETHROW;
        }
        if (new_item_added)
        {
            *new_item_added = true;
        }
    }

    return( POSITION( pNode ) );
}

template< typename K, typename V, class KTraits, class VTraits >
POSITION XyAtlMap< K, V, KTraits, VTraits >::SetAt(
    /* _In_ */ KINARGTYPE key,
    /* _In_ */ VINARGTYPE value)
{
    CNode* pNode;
    UINT iBin;
    UINT nHash;
    CNode* pPrev;

    pNode = GetNode( key, iBin, nHash, pPrev );
    if( pNode == NULL )
    {
        pNode = CreateNode( key, iBin, nHash );
        _ATLTRY
        {
            pNode->m_value = value;
        }
        _ATLCATCHALL()
        {
            RemoveAtPos( POSITION( pNode ) );
            _ATLRETHROW;
        }
    }
    else
    {
        pNode->m_value = value;
    }

    return( POSITION( pNode ) );
}

template< typename K, typename V, class KTraits, class VTraits >
void XyAtlMap< K, V, KTraits, VTraits >::SetValueAt(
    _In_ POSITION pos,
    /* _In_ */ VINARGTYPE value)
{
    ATLASSUME( pos != NULL );

    CNode* pNode = static_cast< CNode* >( pos );

    pNode->m_value = value;
}

template< typename K, typename V, class KTraits, class VTraits >
XyAtlMap< K, V, KTraits, VTraits >::XyAtlMap(
    _In_ UINT nBins,
    _In_ float fOptimalLoad,
    _In_ float fLoThreshold,
    _In_ float fHiThreshold,
    _In_ UINT nBlockSize) throw() 
    : XyAtlMapHelper(nBins, fOptimalLoad, fLoThreshold, fHiThreshold, nBlockSize)
{
    ATLASSERT( nBins > 0 );
    ATLASSERT( nBlockSize > 0 );

    SetOptimalLoad( fOptimalLoad, fLoThreshold, fHiThreshold, false );
}

template< typename K, typename V, class KTraits, class VTraits >
void XyAtlMap< K, V, KTraits, VTraits >::SetOptimalLoad(
    _In_ float fOptimalLoad,
    _In_ float fLoThreshold,
    _In_ float fHiThreshold,
    _In_ bool bRehashNow)
{
    ATLASSERT( fOptimalLoad > 0 );
    ATLASSERT( (fLoThreshold >= 0) && (fLoThreshold < fOptimalLoad) );
    ATLASSERT( fHiThreshold > fOptimalLoad );

    m_fOptimalLoad = fOptimalLoad;
    m_fLoThreshold = fLoThreshold;
    m_fHiThreshold = fHiThreshold;

    UpdateRehashThresholds();

    if( bRehashNow && ((m_nElements > m_nHiRehashThreshold) ||
        (m_nElements < m_nLoRehashThreshold)) )
    {
        Rehash( PickSize( m_nElements ) );
    }
}

template< typename K, typename V, class KTraits, class VTraits >
void XyAtlMap< K, V, KTraits, VTraits >::RemoveAll()
{
    DisableAutoRehash();
    if( m_ppBins != NULL )
    {
        for( UINT iBin = 0; iBin < m_nBins; iBin++ )
        {
            CNode* pNext;

            pNext = static_cast<CNode*>(m_ppBins[iBin]);
            while( pNext != NULL )
            {
                CNode* pKill;

                pKill = pNext;
                pNext = pNext->m_pNext;
                FreeNode( pKill );
            }
        }
    }

    delete[] m_ppBins;
    m_ppBins = NULL;
    m_nElements = 0;

    if( !IsLocked() )
    {
        InitHashTable( PickSize( m_nElements ), false );
    }

    FreePlexes();
    EnableAutoRehash();
}

template< typename K, typename V, class KTraits, class VTraits >
XyAtlMap< K, V, KTraits, VTraits >::~XyAtlMap() throw()
{
    _ATLTRY
    {
        RemoveAll();
    }
    _ATLCATCHALL()
    {
        ATLASSERT(false);
    }
}

#pragma push_macro("new")
#undef new

template< typename K, typename V, class KTraits, class VTraits >
typename XyAtlMap< K, V, KTraits, VTraits >::CNode* XyAtlMap< K, V, KTraits, VTraits >::NewNode(
    /* _In_ */ KINARGTYPE key,
    _In_ UINT iBin,
    _In_ UINT nHash)
{
    CNode* pNewNode;

    if( m_pFree == NULL )
    {
        CAtlPlex* pPlex;
        CNode* pNode;

        pPlex = CAtlPlex::Create( m_pBlocks, m_nBlockSize, sizeof( CNode ) );
        if( pPlex == NULL )
        {
            AtlThrow( E_OUTOFMEMORY );
        }
        pNode = static_cast<CNode*>(pPlex->data());
        pNode += m_nBlockSize-1;
        for( int iBlock = m_nBlockSize-1; iBlock >= 0; iBlock-- )
        {
            pNode->m_pNext = static_cast<CNode*>(m_pFree);
            m_pFree = pNode;
            pNode--;
        }
    }
    ATLENSURE(m_pFree != NULL );
    pNewNode = static_cast<CNode*>(m_pFree);
    m_pFree = pNewNode->m_pNext;

    _ATLTRY
    {
        ::new( pNewNode ) CNode( key, nHash );
    }
    _ATLCATCHALL()
    {
        pNewNode->m_pNext = static_cast<CNode*>(m_pFree);
        m_pFree = pNewNode;

        _ATLRETHROW;
    }
    m_nElements++;

    pNewNode->m_pNext = static_cast<CNode*>(m_ppBins[iBin]);
    m_ppBins[iBin] = pNewNode;

    if( (m_nElements > m_nHiRehashThreshold) && !IsLocked() )
    {
        Rehash( PickSize( m_nElements ) );
    }

    return( pNewNode );
}

#pragma pop_macro("new")

template< typename K, typename V, class KTraits, class VTraits >
void XyAtlMap< K, V, KTraits, VTraits >::FreeNode(_Inout_ CNode* pNode)
{
    ATLENSURE( pNode != NULL );

    pNode->~CNode();
    pNode->m_pNext = static_cast<CNode*>(m_pFree);
    m_pFree = pNode;

    ATLASSUME( m_nElements > 0 );
    m_nElements--;

    if( (m_nElements < m_nLoRehashThreshold) && !IsLocked() )
    {
        Rehash( PickSize( m_nElements ) );
    }

    if( m_nElements == 0 )
    {
        FreePlexes();
    }
}

template< typename K, typename V, class KTraits, class VTraits >
typename XyAtlMap< K, V, KTraits, VTraits >::CNode* XyAtlMap< K, V, KTraits, VTraits >::GetNode(
    /* _In_ */ KINARGTYPE key,
    _Out_ UINT& iBin,
    _Out_ UINT& nHash,
    _Deref_out_opt_ CNode*& pPrev) const throw()
{
    CNode* pFollow;

    nHash = KTraits::Hash( key );
    iBin = nHash%m_nBins;

    if( m_ppBins == NULL )
    {
        return( NULL );
    }

    pFollow = NULL;
    pPrev = NULL;
    for( CNode* pNode = static_cast<CNode*>(m_ppBins[iBin]); pNode != NULL; pNode = pNode->m_pNext )
    {
        if( (pNode->GetHash() == nHash) && KTraits::CompareElements( pNode->m_key, key ) )
        {
            pPrev = pFollow;
            return( pNode );
        }
        pFollow = pNode;
    }

    return( NULL );
}

template< typename K, typename V, class KTraits, class VTraits >
bool XyAtlMap< K, V, KTraits, VTraits >::Lookup(
    /* _In_ */ KINARGTYPE key,
    _Out_ VOUTARGTYPE value) const
{
    UINT iBin;
    UINT nHash;
    CNode* pNode;
    CNode* pPrev;

    pNode = GetNode( key, iBin, nHash, pPrev );
    if( pNode == NULL )
    {
        return( false );
    }

    value = pNode->m_value;

    return( true );
}

template< typename K, typename V, class KTraits, class VTraits >
const typename XyAtlMap< K, V, KTraits, VTraits >::CPair* XyAtlMap< K, V, KTraits, VTraits >::Lookup(
    /* _In_ */ KINARGTYPE key) const throw()
{
    UINT iBin;
    UINT nHash;
    CNode* pNode;
    CNode* pPrev;

    pNode = GetNode( key, iBin, nHash, pPrev );

    return( pNode );
}

template< typename K, typename V, class KTraits, class VTraits >
typename XyAtlMap< K, V, KTraits, VTraits >::CPair* XyAtlMap< K, V, KTraits, VTraits >::Lookup(
    /* _In_ */ KINARGTYPE key) throw()
{
    UINT iBin;
    UINT nHash;
    CNode* pNode;
    CNode* pPrev;

    pNode = GetNode( key, iBin, nHash, pPrev );

    return( pNode );
}

template< typename K, typename V, class KTraits, class VTraits >
bool XyAtlMap< K, V, KTraits, VTraits >::RemoveKey(/* _In_ */ KINARGTYPE key) throw()
{
    CNode* pNode;
    UINT iBin;
    UINT nHash;
    CNode* pPrev;

    pPrev = NULL;
    pNode = GetNode( key, iBin, nHash, pPrev );
    if( pNode == NULL )
    {
        return( false );
    }

    RemoveNode( pNode, pPrev );

    return( true );
}

template< typename K, typename V, class KTraits, class VTraits >
void XyAtlMap< K, V, KTraits, VTraits >::RemoveNode(
    _In_ CNode* pNode,
    _In_opt_ CNode* pPrev)
{
    ATLENSURE( pNode != NULL );

    UINT iBin = pNode->GetHash() % m_nBins;

    if( pPrev == NULL )
    {
        ATLASSUME( m_ppBins[iBin] == pNode );
        m_ppBins[iBin] = pNode->m_pNext;
    }
    else
    {
        ATLASSERT( pPrev->m_pNext == pNode );
        pPrev->m_pNext = pNode->m_pNext;
    }
    FreeNode( pNode );
}

template< typename K, typename V, class KTraits, class VTraits >
void XyAtlMap< K, V, KTraits, VTraits >::RemoveAtPos(_In_ POSITION pos)
{
    ATLENSURE( pos != NULL );

    CNode* pNode = static_cast< CNode* >( pos );
    CNode* pPrev = NULL;
    UINT iBin = pNode->GetHash() % m_nBins;

    ATLASSUME( m_ppBins[iBin] != NULL );
    if( pNode == static_cast<CNode*>(m_ppBins[iBin]) )
    {
        pPrev = NULL;
    }
    else
    {
        pPrev = static_cast<CNode*>(m_ppBins[iBin]);
        while( pPrev->m_pNext != pNode )
        {
            pPrev = pPrev->m_pNext;
            ATLASSERT( pPrev != NULL );
        }
    }
    RemoveNode( pNode, pPrev );
}

template< typename K, typename V, class KTraits, class VTraits >
void XyAtlMap< K, V, KTraits, VTraits >::Rehash(_In_ UINT nBins)
{
    CNode** ppBins = NULL;

    if( nBins == 0 )
    {
        nBins = PickSize( m_nElements );
    }

    if( nBins == m_nBins )
    {
        return;
    }

    ATLTRACE(atlTraceMap, 2, _T("Rehash: %u bins\n"), nBins );

    if( m_ppBins == NULL )
    {
        // Just set the new number of bins
        InitHashTable( nBins, false );
        return;
    }

    ATLTRY(ppBins = new CNode*[nBins]);
    if (ppBins == NULL)
    {
        AtlThrow( E_OUTOFMEMORY );
    }

    ATLENSURE( UINT_MAX / sizeof( CNode* ) >= nBins );
    memset( ppBins, 0, nBins*sizeof( CNode* ) );

    // Nothing gets copied.  We just rewire the old nodes
    // into the new bins.
    for( UINT iSrcBin = 0; iSrcBin < m_nBins; iSrcBin++ )
    {
        CNode* pNode;

        pNode = static_cast<CNode*>(m_ppBins[iSrcBin]);
        while( pNode != NULL )
        {
            CNode* pNext;
            UINT iDestBin;

            pNext = pNode->m_pNext;  // Save so we don't trash it
            iDestBin = pNode->GetHash()%nBins;
            pNode->m_pNext = ppBins[iDestBin];
            ppBins[iDestBin] = pNode;

            pNode = pNext;
        }
    }

    delete[] m_ppBins;
    m_ppBins = (PNode*)ppBins;
    m_nBins = nBins;

    UpdateRehashThresholds();
}

template< typename K, typename V, class KTraits, class VTraits >
void XyAtlMap< K, V, KTraits, VTraits >::GetNextAssoc(
    _Inout_ POSITION& pos,
    _Out_ KOUTARGTYPE key,
    _Out_ VOUTARGTYPE value) const
{
    CNode* pNode;
    CNode* pNext;

    ATLASSUME( m_ppBins != NULL );
    ATLENSURE( pos != NULL );

    pNode = static_cast<CNode*>(pos);
    pNext = FindNextNode( pNode );

    pos = POSITION( pNext );
    key = pNode->m_key;
    value = pNode->m_value;
}

template< typename K, typename V, class KTraits, class VTraits >
const typename XyAtlMap< K, V, KTraits, VTraits >::CPair* XyAtlMap< K, V, KTraits, VTraits >::GetNext(
    _Inout_ POSITION& pos) const throw()
{
    CNode* pNode;
    CNode* pNext;

    ATLASSUME( m_ppBins != NULL );
    ATLASSERT( pos != NULL );

    pNode = static_cast<CNode*>(pos);
    pNext = FindNextNode( pNode );

    pos = POSITION( pNext );

    return( pNode );
}

template< typename K, typename V, class KTraits, class VTraits >
typename XyAtlMap< K, V, KTraits, VTraits >::CPair* XyAtlMap< K, V, KTraits, VTraits >::GetNext(
    _Inout_ POSITION& pos) throw()
{
    ATLASSUME( m_ppBins != NULL );
    ATLASSERT( pos != NULL );

    CNode* pNode = static_cast< CNode* >( pos );
    CNode* pNext = FindNextNode( pNode );

    pos = POSITION( pNext );

    return( pNode );
}

template< typename K, typename V, class KTraits, class VTraits >
const K& XyAtlMap< K, V, KTraits, VTraits >::GetNextKey(
    _Inout_ POSITION& pos) const
{
    CNode* pNode;
    CNode* pNext;

    ATLASSUME( m_ppBins != NULL );
    ATLENSURE( pos != NULL );

    pNode = static_cast<CNode*>(pos);
    pNext = FindNextNode( pNode );

    pos = POSITION( pNext );

    return( pNode->m_key );
}

template< typename K, typename V, class KTraits, class VTraits >
const V& XyAtlMap< K, V, KTraits, VTraits >::GetNextValue(
    _Inout_ POSITION& pos) const
{
    CNode* pNode;
    CNode* pNext;

    ATLASSUME( m_ppBins != NULL );
    ATLENSURE( pos != NULL );

    pNode = static_cast<CNode*>(pos);
    pNext = FindNextNode( pNode );

    pos = POSITION( pNext );

    return( pNode->m_value );
}

template< typename K, typename V, class KTraits, class VTraits >
V& XyAtlMap< K, V, KTraits, VTraits >::GetNextValue(
    _Inout_ POSITION& pos)
{
    CNode* pNode;
    CNode* pNext;

    ATLASSUME( m_ppBins != NULL );
    ATLENSURE( pos != NULL );

    pNode = static_cast<CNode*>(pos);
    pNext = FindNextNode( pNode );

    pos = POSITION( pNext );

    return( pNode->m_value );
}

template< typename K, typename V, class KTraits, class VTraits >
typename XyAtlMap< K, V, KTraits, VTraits >::CNode* XyAtlMap< K, V, KTraits, VTraits >::FindNextNode(
    _In_ CNode* pNode) const throw()
{
    CNode* pNext;

    if(pNode == NULL)
    {
        ATLASSERT(FALSE);
        return NULL;
    }

    if( pNode->m_pNext != NULL )
    {
        pNext = pNode->m_pNext;
    }
    else
    {
        UINT iBin;

        pNext = NULL;
        iBin = (pNode->GetHash()%m_nBins)+1;
        while( (pNext == NULL) && (iBin < m_nBins) )
        {
            if( m_ppBins[iBin] != NULL )
            {
                pNext = static_cast<CNode*>(m_ppBins[iBin]);
            }

            iBin++;
        }
    }

    return( pNext );
}


////////////////////////////////////////////////////////////////////////////////////////////////////

template<
    typename K,
    typename V,
    class KTraits = CElementTraits< K >
>
class XyMru
{
public:
    XyMru(std::size_t max_item_num):_max_item_num(max_item_num){}

    inline POSITION UpdateCache(POSITION pos)
    {
        _list.MoveToHead(pos);
        return pos;
    }
    inline POSITION UpdateCache(POSITION pos, const V& value)
    {
        _list.GetAt(pos).second = value;
        _list.MoveToHead(pos);
        return pos;
    }
    inline POSITION UpdateCache(const K& key, const V& value)
    {
        POSITION pos;
        POSITION pos_hash_value = NULL;
        bool new_item_added = false;
        pos = _hash.SetAtIfNotExists(key, (POSITION)NULL, &new_item_added);
        if (new_item_added)
        {
            pos_hash_value = _list.AddHead( ListItem(pos, value) );
            _hash.SetValueAt(pos, pos_hash_value);
        }
        else
        {
            pos_hash_value = _hash.GetValueAt(pos);
            _list.GetAt(pos_hash_value).second = value;
            _list.MoveToHead(pos_hash_value);
        }
        if(_list.GetCount()>_max_item_num)
        {
            _hash.RemoveAtPos(_list.GetTail().first);
            _list.RemoveTail();
        }
        return pos_hash_value;
    }
    inline POSITION AddHeadIfNotExists(const K& key, const V& value, bool *new_item_added)
    {
        POSITION pos;
        POSITION pos_hash_value = NULL;
        bool new_hash_item_added = false;
        pos = _hash.SetAtIfNotExists(key, (POSITION)NULL, &new_hash_item_added);
        if (new_hash_item_added)
        {
            pos_hash_value = _list.AddHead( ListItem(pos, value) );
            _hash.SetValueAt(pos, pos_hash_value);
            if (new_item_added)
            {
                *new_item_added = true;
            }
        }
        else
        {
            pos_hash_value = _hash.GetValueAt(pos);
            if (new_item_added)
            {
                *new_item_added = false;
            }
        }
        if(_list.GetCount()>_max_item_num)
        {
            _hash.RemoveAtPos(_list.GetTail().first);
            _list.RemoveTail();
        }
        return pos_hash_value;
    }
    inline void RemoveAll() 
    { 
        _hash.RemoveAll();
        _list.RemoveAll();
    }
    
    inline POSITION Lookup(const K& key) const
    {
        POSITION pos;
        if( _hash.Lookup(key,pos) )
        {
            return pos;
        }
        else
        {
            return NULL;
        }
    }
    inline V& GetAt(POSITION pos)
    {
        return _list.GetAt(pos).second;
    }
    inline const V& GetAt(POSITION pos) const
    {
        return _list.GetAt(pos).second;
    }
    inline const K& GetKeyAt(POSITION pos) const
    {
        return _hash.GetKeyAt(_list.GetAt(pos).first);
    }
    inline std::size_t SetMaxItemNum( std::size_t max_item_num )
    {
        _max_item_num = max_item_num;
        while(_list.GetCount()>_max_item_num)
        {
            _hash.RemoveAtPos(_list.GetTail().first);
            _list.RemoveTail();
        }
        return _max_item_num;
    }
    inline std::size_t GetMaxItemNum() const { return _max_item_num; }
    inline std::size_t GetCurItemNum() const { return _list.GetCount(); }
protected:
    typedef std::pair<POSITION,V> ListItem;
    CAtlList<ListItem> _list;
    XyAtlMap<K,POSITION,KTraits> _hash;

    std::size_t _max_item_num;
};

template<
    typename K,
    typename V,
class KTraits = CElementTraits< K >
>
class EnhancedXyMru:public XyMru<K,V,KTraits>
{
public:
    EnhancedXyMru(std::size_t max_item_num):XyMru(max_item_num),_cache_hit(0),_query_count(0){}

    std::size_t SetMaxItemNum( std::size_t max_item_num, bool clear_statistic_info=false )
    {
        if(clear_statistic_info)
        {
            _cache_hit = 0;
            _query_count = 0;
        }
        return __super::SetMaxItemNum(max_item_num);
    }
    void RemoveAll(bool clear_statistic_info=false) 
    { 
        if(clear_statistic_info) 
        { 
            _cache_hit=0; 
            _query_count=0; 
        } 
        __super::RemoveAll();
    }

    inline POSITION Lookup(const K& key)
    {
        _query_count++;
        POSITION pos = __super::Lookup(key);
        _cache_hit += (pos!=NULL);
        return pos;
    }
    inline POSITION AddHeadIfNotExists(const K& key, const V& value, bool *new_item_added)
    {
        _query_count++;
        bool tmp = false;
        POSITION pos = __super::AddHeadIfNotExists(key, value, &tmp);
        _cache_hit += (tmp==false);
        if(new_item_added)
            *new_item_added = tmp;
        return pos;
    }

    inline std::size_t GetCacheHitCount() const { return _cache_hit; }
    inline std::size_t GetQueryCount() const { return _query_count; }
protected:
    std::size_t _cache_hit;
    std::size_t _query_count;
};

#endif // end of __MRU_CACHE_H_256FCF72_8663_41DC_B98A_B822F6007912__
