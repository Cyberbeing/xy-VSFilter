#pragma once

#include <boost/flyweight.hpp>
#include <boost/flyweight/set_factory.hpp>
#include <boost/flyweight/no_tracking.hpp>
#include <boost/flyweight/no_locking.hpp>
#include <afx.h>

typedef ::boost::flyweights::flyweight<CStringW, ::boost::flyweights::no_locking> FwStringW;
typedef ::boost::flyweights::flyweight<CStringA, ::boost::flyweights::no_locking> FwStringA;
#ifdef _UNICODE
typedef FwStringW FwString;
#else
typedef FwStringA FwString;
#endif // _UNICODE
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
    return s.left ^ s.right ^ s.bottom ^ s.top;
}
