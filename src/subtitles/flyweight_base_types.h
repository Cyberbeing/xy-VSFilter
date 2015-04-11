#pragma once

#include <boost/flyweight.hpp>
#include <boost/flyweight/set_factory.hpp>
#include <boost/flyweight/no_tracking.hpp>
#include <boost/flyweight/no_locking.hpp>
#include <boost/smart_ptr.hpp>
#include <afx.h>

#include "mru_cache.h"

template<>
struct std::equal_to<CRect>
{	// functor for operator==
    bool operator()(const CRect& _Left, const CRect& _Right) const
    {	// apply operator== to operands
        return (_Left == _Right)==TRUE;
    }
};

typedef ::boost::flyweights::flyweight<CRect, ::boost::flyweights::no_locking> FwRect;

template<
    typename V,
    int DEFAULT_CACHE_SIZE = 1024,
    class VTraits=CElementTraits<V>
>
class XyFlyWeight
{
public:
    typedef std::size_t IdType;
    typedef ::boost::shared_ptr<const V> SharedConstV;

    class SharedVElementTraits:public CElementTraits<SharedConstV>
    {
    public:
        static inline ULONG Hash(const SharedConstV& v)
        {
            return VTraits::Hash(*v);
        }
        static inline bool CompareElements(
            const SharedConstV& element1,
            const SharedConstV& element2)
        {
            return VTraits::CompareElements(*element1, *element2);
        }
        static inline int CompareElementsOrdered(
            const SharedConstV& element1,
            const SharedConstV& element2)
        {
            return VTraits::CompareElementsOrdered(*element1, *element2);
        }
    };

    typedef EnhancedXyMru<SharedConstV, IdType, SharedVElementTraits> Cacher;    
public:
    static const int INVALID_ID = 0;
public:
    // v will be assigned to a shared pointer
    XyFlyWeight(const V *v):_v(v)
    {
        Cacher * cacher = GetCacher();
        ASSERT( cacher );
        bool new_item_added = false;
        POSITION pos = cacher->AddHeadIfNotExists(_v, INVALID_ID, &new_item_added);
        if ( !new_item_added )
        {
            cacher->UpdateCache(pos);
            _v = cacher->GetKeyAt(pos);
            _id = cacher->GetAt(pos);
        }
        else
        {
            _id = AllocId();
            cacher->GetAt(pos) = _id;
        }
    }
    inline const V& Get() const { return *_v; }
    inline IdType GetId() const { return _id; }

    static inline Cacher* GetCacher();
private:
    SharedConstV _v;
    IdType _id;

    static inline IdType AllocId();
};

template<typename V, int DEFAULT_CACHE_SIZE, class VTraits>
inline
typename XyFlyWeight<V, DEFAULT_CACHE_SIZE, VTraits>::Cacher* XyFlyWeight<V, DEFAULT_CACHE_SIZE, VTraits>::GetCacher()
{
    static Cacher cacher(DEFAULT_CACHE_SIZE);
    return &cacher;
}

template<typename V, int DEFAULT_CACHE_SIZE, class VTraits>
inline
typename XyFlyWeight<V, DEFAULT_CACHE_SIZE, VTraits>::IdType XyFlyWeight<V, DEFAULT_CACHE_SIZE, VTraits>::AllocId()
{
    static IdType cur_id=INVALID_ID;
    ++cur_id;
    if (cur_id==INVALID_ID)
    {
        ++cur_id;
    }
    return cur_id; 
}

typedef XyFlyWeight<CStringW, 16*1024, CStringElementTraits<CStringW>> XyFwStringW;

static inline std::size_t hash_value(const double& d)
{
    std::size_t hash = 515;
    const int32_t* tmp = reinterpret_cast<const int32_t*>(&d);
    hash += (hash<<5);
    hash += *(tmp++);
    hash += (hash<<5);
    hash += *(tmp++);
    return hash;
}

static inline std::size_t hash_value(const CStringW& s)
{
    return CStringElementTraits<CStringW>::Hash(s);
}

static inline std::size_t hash_value(const CRect& s)
{
    std::size_t hash = 515;
    hash += (hash<<5);
    hash += s.left;
    hash += (hash<<5);
    hash += s.right;
    hash += (hash<<5);
    hash += s.bottom;
    hash += (hash<<5);
    hash += s.top;
    return hash;
}
