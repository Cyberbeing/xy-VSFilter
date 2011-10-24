#pragma once

#include <boost/flyweight.hpp>
#include <boost/flyweight/set_factory.hpp>
#include <boost/flyweight/no_tracking.hpp>
#include <boost/flyweight/no_locking.hpp>
#include <afx.h>

typedef ::boost::flyweights::flyweight<CRect, ::boost::flyweights::no_locking> FwRect;

inline std::size_t hash_value(const CStringW& s)
{
    return CStringElementTraits<CStringW>::Hash(s);
}

inline std::size_t hash_value(const CStringA& s)
{
    return CStringElementTraits<CStringA>::Hash(s);
}

inline std::size_t hash_value(const CRect& s)
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
