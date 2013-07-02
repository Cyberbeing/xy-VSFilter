#include "stdafx.h"
#include "XyOptionsImpl.h"


XyOptionsImpl::XyOptionsImpl( const Option *option_list )
{
    for (int i=0;i<=MAX_OPTION_ID;i++)
    {
        m_option_type[i] = OPTION_TYPE_END_FLAG;

        m_xy_int_opt[i] = 0;
        m_xy_bool_opt[i] = false;
        m_xy_double_opt[i] = 0;
    }
    for (const Option * op =option_list; op && op->type!=OPTION_TYPE_END_FLAG; op++)
    {
        ASSERT(op->option_id<MAX_OPTION_ID);
        m_option_type[op->option_id] = op->type | op->rw_mode;
    }
}

#define XY_GET_TEMPLATE(OPTION_TYPE_x, m_xy_opt_x)                                 \
    if (!TestOption(field, OPTION_TYPE_x, OPTION_MODE_READ)) return E_INVALIDARG;  \
    if (!value) return S_FALSE;                                                    \
    return DoGetField(field, value);

STDMETHODIMP XyOptionsImpl::XyGetBool(unsigned field, bool *value)
{
    XY_GET_TEMPLATE(OPTION_TYPE_BOOL, m_xy_bool_opt);
}

STDMETHODIMP XyOptionsImpl::XyGetInt(unsigned field, int *value)
{
    XY_GET_TEMPLATE(OPTION_TYPE_INT, m_xy_int_opt);
}

STDMETHODIMP XyOptionsImpl::XyGetSize(unsigned field, SIZE *value)
{
    XY_GET_TEMPLATE(OPTION_TYPE_SIZE, m_xy_size_opt);
}

STDMETHODIMP XyOptionsImpl::XyGetRect(unsigned field, RECT *value)
{
    XY_GET_TEMPLATE(OPTION_TYPE_RECT, m_xy_rect_opt);
}

STDMETHODIMP XyOptionsImpl::XyGetUlonglong(unsigned field, ULONGLONG *value)
{
    XY_GET_TEMPLATE(OPTION_TYPE_ULONGLONG, m_xy_ulonglong_opt);
}

STDMETHODIMP XyOptionsImpl::XyGetDouble(unsigned field, double *value)
{
    XY_GET_TEMPLATE(OPTION_TYPE_DOUBLE, m_xy_double_opt);
}

STDMETHODIMP XyOptionsImpl::XyGetString(unsigned field, LPWSTR *value, int *chars)
{
    if (!TestOption(field, OPTION_TYPE_STRING, OPTION_MODE_READ)) return E_INVALIDARG;
    if (!value && !chars) return S_FALSE;
    CStringW tmp;
    HRESULT hr = DoGetField(field, &tmp);
    if (SUCCEEDED(hr))
    {
        if (value)
        {
            *value = static_cast<LPWSTR>(LocalAlloc(LPTR, sizeof(WCHAR)*(tmp.GetLength()+1)));
            ASSERT(*value);
            memcpy(*value, tmp.GetString(), (tmp.GetLength()+1)*sizeof(WCHAR));
        }
        if (chars)
        {
            *chars = tmp.GetLength();
        }
    }
    return hr;
}

STDMETHODIMP XyOptionsImpl::XyGetBin(unsigned field, LPVOID *value, int *size)
{
    if (!TestOption(field, OPTION_TYPE_BIN, OPTION_MODE_READ)) return E_INVALIDARG;
    if (!value && !size) return S_FALSE;
    return E_NOTIMPL;
}

STDMETHODIMP XyOptionsImpl::XyGetBin2( unsigned field, void *value, int size )
{
    if (!TestOption(field, OPTION_TYPE_BIN, OPTION_MODE_READ)) return E_INVALIDARG;
    if (!value) return S_FALSE;
    return E_NOTIMPL;
}

#define XY_SET_TEMPLATE(OPTION_TYPE_x, m_xy_opt_x)                                  \
    if (!TestOption(field, OPTION_TYPE_x, OPTION_MODE_WRITE)) return E_INVALIDARG;  \
    return DoSetField(field, &value);

STDMETHODIMP XyOptionsImpl::XySetBool(unsigned field, bool value)
{
    XY_SET_TEMPLATE(OPTION_TYPE_BOOL, m_xy_bool_opt);
}

STDMETHODIMP XyOptionsImpl::XySetInt(unsigned field, int value)
{
    XY_SET_TEMPLATE(OPTION_TYPE_INT, m_xy_int_opt);
}

STDMETHODIMP XyOptionsImpl::XySetSize(unsigned field, SIZE value)
{
    XY_SET_TEMPLATE(OPTION_TYPE_SIZE, m_xy_size_opt);
}

STDMETHODIMP XyOptionsImpl::XySetRect(unsigned field, RECT value)
{
    XY_SET_TEMPLATE(OPTION_TYPE_RECT, m_xy_rect_opt);
}

STDMETHODIMP XyOptionsImpl::XySetUlonglong(unsigned field, ULONGLONG value)
{
    XY_SET_TEMPLATE(OPTION_TYPE_ULONGLONG, m_xy_ulonglong_opt);
}

STDMETHODIMP XyOptionsImpl::XySetDouble(unsigned field, double value)
{
    XY_SET_TEMPLATE(OPTION_TYPE_DOUBLE, m_xy_double_opt);
}

STDMETHODIMP XyOptionsImpl::XySetString(unsigned field, LPWSTR value, int chars)
{
    if (!TestOption(field, OPTION_TYPE_STRING, OPTION_MODE_WRITE)) return E_INVALIDARG;
    CStringW tmp(value, chars);
    return DoSetField(field, &tmp);
}

STDMETHODIMP XyOptionsImpl::XySetBin(unsigned field, LPVOID value, int size)
{
    if (!TestOption(field, OPTION_TYPE_BIN, OPTION_MODE_WRITE)) return E_INVALIDARG;
    return E_NOTIMPL;
}

// methods for array not implement

STDMETHODIMP XyOptionsImpl::XyGetBools(unsigned field, int id, bool *value)
{
    return E_NOTIMPL;
}

STDMETHODIMP XyOptionsImpl::XyGetInts(unsigned field, int id, int *value)
{
    return E_NOTIMPL;
}

STDMETHODIMP XyOptionsImpl::XyGetSizes(unsigned field, int id, SIZE *value)
{
    return E_NOTIMPL;
}

STDMETHODIMP XyOptionsImpl::XyGetRects(unsigned field, int id, RECT *value)
{
    return E_NOTIMPL;
}

STDMETHODIMP XyOptionsImpl::XyGetUlonglongs(unsigned field, int id, ULONGLONG *value)
{
    return E_NOTIMPL;
}

STDMETHODIMP XyOptionsImpl::XyGetDoubles(unsigned field, int id, double *value)
{
    return E_NOTIMPL;
}

STDMETHODIMP XyOptionsImpl::XyGetStrings(unsigned field, int id, LPWSTR *value, int *chars)
{
    return E_NOTIMPL;
}

STDMETHODIMP XyOptionsImpl::XyGetBins(unsigned field, int id, LPVOID *value, int *size)
{
    return E_NOTIMPL;
}

STDMETHODIMP XyOptionsImpl::XyGetBins2( unsigned field, int id, void *value, int size )
{
    return E_NOTIMPL;
}

STDMETHODIMP XyOptionsImpl::XySetBools(unsigned field, int id, bool value)
{
    return E_NOTIMPL;
}

STDMETHODIMP XyOptionsImpl::XySetInts(unsigned field, int id, int value)
{
    return E_NOTIMPL;
}

STDMETHODIMP XyOptionsImpl::XySetSizes(unsigned field, int id, SIZE value)
{
    return E_NOTIMPL;
}

STDMETHODIMP XyOptionsImpl::XySetRects(unsigned field, int id, RECT value)
{
    return E_NOTIMPL;
}

STDMETHODIMP XyOptionsImpl::XySetUlonglongs(unsigned field, int id, ULONGLONG value)
{
    return E_NOTIMPL;
}

STDMETHODIMP XyOptionsImpl::XySetDoubles(unsigned field, int id, double value)
{
    return E_NOTIMPL;
}

STDMETHODIMP XyOptionsImpl::XySetStrings(unsigned field, int id, LPWSTR value, int chars)
{
    return E_NOTIMPL;
}

STDMETHODIMP XyOptionsImpl::XySetBins(unsigned field, int id, LPVOID value, int size)
{
    return E_NOTIMPL;
}

HRESULT XyOptionsImpl::DoGetField( unsigned field, void *value )
{
    switch(m_option_type[field] & ~OPTION_MODE_RW)
    {
    case OPTION_TYPE_BOOL:
        *(bool*)value = m_xy_bool_opt[field];
        break;
    case OPTION_TYPE_INT:
        *(int*)value = m_xy_int_opt[field];
        break;
    case OPTION_TYPE_SIZE:
        *(SIZE*)value = m_xy_size_opt[field];
        break;
    case OPTION_TYPE_RECT:
        *(RECT*)value = m_xy_rect_opt[field];
        break;
    case OPTION_TYPE_ULONGLONG:
        *(ULONGLONG*)value = m_xy_ulonglong_opt[field];
        break;
    case OPTION_TYPE_DOUBLE:
        *(double*)value = m_xy_double_opt[field];
        break;
    case OPTION_TYPE_STRING:
        *(CStringW*)value = m_xy_str_opt[field];
        break;
    case OPTION_TYPE_BIN:
    case OPTION_TYPE_BIN2:
        return E_NOTIMPL;
    }
    return S_OK;
}

HRESULT XyOptionsImpl::DoSetField( unsigned field, void const *value )
{
    HRESULT hr = S_FALSE;
    switch(m_option_type[field] & ~OPTION_MODE_RW)
    {
    case OPTION_TYPE_BOOL:
        if (m_xy_bool_opt[field] != *(bool*)value)
        {
            m_xy_bool_opt[field] = *(bool*)value;
            hr = OnOptionChanged(field);
        }
        break;
    case OPTION_TYPE_INT:
        if (m_xy_int_opt[field] != *(int*)value)
        {
            m_xy_int_opt[field] = *(int*)value;
            hr = OnOptionChanged(field);
        }
        break;
    case OPTION_TYPE_SIZE:
        if (m_xy_size_opt[field] != *(SIZE*)value)
        {
            m_xy_size_opt[field] = *(SIZE*)value;
            hr = OnOptionChanged(field);
        }
        break;
    case OPTION_TYPE_RECT:
        if (m_xy_rect_opt[field] != *(RECT*)value)
        {
            m_xy_rect_opt[field] = *(RECT*)value;
            hr = OnOptionChanged(field);
        }
        break;
    case OPTION_TYPE_ULONGLONG:
        if (m_xy_ulonglong_opt[field] = *(ULONGLONG*)value)
        {
            m_xy_ulonglong_opt[field] = *(ULONGLONG*)value;
            hr = OnOptionChanged(field);
        }
        break;
    case OPTION_TYPE_DOUBLE:
        if (m_xy_double_opt[field] != *(double*)value)
        {
            m_xy_double_opt[field] = *(double*)value;
            hr = OnOptionChanged(field);
        }
        break;
    case OPTION_TYPE_STRING:
        if (m_xy_str_opt[field] != *(CStringW*)value)
        {
            m_xy_str_opt[field] = *(CStringW*)value;
            hr = OnOptionChanged(field);
        }
        break;
    case OPTION_TYPE_BIN:
    case OPTION_TYPE_BIN2:
        hr = E_NOTIMPL;
        break;
    }
    return S_OK;
}

HRESULT XyOptionsImpl::OnOptionChanged( unsigned field )
{
    return S_OK;
}
