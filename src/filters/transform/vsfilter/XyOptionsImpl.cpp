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
    HRESULT hr = OnOptionReading(field);                                           \
    if (SUCCEEDED(hr)) *value = m_xy_opt_x[field];                                 \
    return hr;

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
    HRESULT hr = OnOptionReading(field);
    if (SUCCEEDED(hr))
    {
        if (value)
        {
            *value = static_cast<LPWSTR>(LocalAlloc(LPTR, sizeof(WCHAR)*(m_xy_str_opt[field].GetLength()+1)));
            ASSERT(*value);
            memcpy(*value, m_xy_str_opt[field].GetString(), (m_xy_str_opt[field].GetLength()+1)*sizeof(WCHAR));
        }
        if (chars)
        {
            *chars = m_xy_str_opt[field].GetLength();
        }
    }
    return hr;
}

STDMETHODIMP XyOptionsImpl::XyGetBin(unsigned field, LPVOID *value, int *size)
{
    if (!TestOption(field, OPTION_TYPE_BIN, OPTION_MODE_READ)) return E_INVALIDARG;
    return E_NOTIMPL;
}

#define XY_SET_TEMPLATE(OPTION_TYPE_x, m_xy_opt_x)                                  \
    if (!TestOption(field, OPTION_TYPE_x, OPTION_MODE_WRITE)) return E_INVALIDARG;  \
    if (m_xy_opt_x[field] == value) return S_FALSE;                                 \
    m_xy_opt_x[field] = value;                                                      \
    return OnOptionChanged(field);

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
    if (m_xy_str_opt[field] == CStringW(value, chars)) return S_FALSE;
    if (chars<0)
    {
        m_xy_str_opt[field] = "";
    }
    m_xy_str_opt[field].SetString(value, chars);
    return OnOptionChanged(field);
}

STDMETHODIMP XyOptionsImpl::XySetBin(unsigned field, LPVOID value, int size)
{
    if (!TestOption(field, OPTION_TYPE_BIN, OPTION_MODE_WRITE)) return E_INVALIDARG;
    return E_NOTIMPL;
}

HRESULT XyOptionsImpl::OnOptionReading( unsigned field )
{
    return S_OK;
}

HRESULT XyOptionsImpl::OnOptionChanged( unsigned field )
{
    return S_OK;
}
