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
        m_option_type[op->option_id] = op->type;
    }
}

STDMETHODIMP XyOptionsImpl::XyGetBool(unsigned field, bool *value)
{
    if (field >= MAX_OPTION_ID || m_option_type[field] != OPTION_TYPE_BOOL)
        return E_INVALIDARG;
    if (!value) return S_FALSE;
    *value = m_xy_bool_opt[field];
    return S_OK;
}

STDMETHODIMP XyOptionsImpl::XyGetInt(unsigned field, int *value)
{
    if (field >= MAX_OPTION_ID || m_option_type[field] != OPTION_TYPE_INT)
        return E_INVALIDARG;
    if (!value) return S_FALSE;
    *value = m_xy_int_opt[field];
    return S_OK;
}

STDMETHODIMP XyOptionsImpl::XyGetSize(unsigned field, SIZE *value)
{
    if (field >= MAX_OPTION_ID || m_option_type[field] != OPTION_TYPE_SIZE)
        return E_INVALIDARG;
    if (!value) return S_FALSE;
    *value = m_xy_size_opt[field];
    return S_OK;
}

STDMETHODIMP XyOptionsImpl::XyGetRect(unsigned field, RECT *value)
{
    if (field >= MAX_OPTION_ID || m_option_type[field] != OPTION_TYPE_RECT)
        return E_INVALIDARG;
    if (!value) return S_FALSE;
    *value = m_xy_rect_opt[field];
    return S_OK;
}

STDMETHODIMP XyOptionsImpl::XyGetUlonglong(unsigned field, ULONGLONG *value)
{
    if (field >= MAX_OPTION_ID || m_option_type[field] != OPTION_TYPE_ULONGLONG)
        return E_INVALIDARG;
    if (!value) return S_FALSE;
    *value = m_xy_ulonglong_opt[field];
    return S_OK;
}

STDMETHODIMP XyOptionsImpl::XyGetDouble(unsigned field, double *value)
{
    if (field >= MAX_OPTION_ID || m_option_type[field] != OPTION_TYPE_DOUBLE)
        return E_INVALIDARG;
    if (!value) return S_FALSE;
    *value = m_xy_double_opt[field];
    return S_OK;
}

STDMETHODIMP XyOptionsImpl::XyGetString(unsigned field, LPWSTR *value, int *chars)
{
    if (field >= MAX_OPTION_ID || m_option_type[field] != OPTION_TYPE_STRING)
        return E_INVALIDARG;
    if (!value && !chars) return S_FALSE;
    if (value)
    {
        *value = new WCHAR[m_xy_str_opt[field].GetLength()];
        ASSERT(*value);
        memcpy(*value, m_xy_str_opt[field].GetString(), m_xy_str_opt[field].GetLength()*sizeof(WCHAR));
    }
    if (chars)
    {
        *chars = m_xy_str_opt[field].GetLength();
    }
    return S_OK;
}

STDMETHODIMP XyOptionsImpl::XyGetBin(unsigned field, LPVOID *value, int *size)
{
    if (field >= MAX_OPTION_ID || m_option_type[field] != OPTION_TYPE_BIN)
        return E_INVALIDARG;
    return E_NOTIMPL;
}

STDMETHODIMP XyOptionsImpl::XySetBool(unsigned field, bool value)
{
    if (field >= MAX_OPTION_ID || m_option_type[field] != OPTION_TYPE_BOOL)
        return E_INVALIDARG;
    if (m_xy_bool_opt[field] == value)
        return S_FALSE;
    m_xy_bool_opt[field] = value;
    return S_OK;
}

STDMETHODIMP XyOptionsImpl::XySetInt(unsigned field, int value)
{
    if (field >= MAX_OPTION_ID || m_option_type[field] != OPTION_TYPE_INT)
        return E_INVALIDARG;
    if (m_xy_int_opt[field] == value)
        return S_FALSE;
    m_xy_int_opt[field] = value;
    return S_OK;
}

STDMETHODIMP XyOptionsImpl::XySetSize(unsigned field, SIZE value)
{
    if (field >= MAX_OPTION_ID || m_option_type[field] != OPTION_TYPE_SIZE)
        return E_INVALIDARG;
    if (m_xy_size_opt[field] == value)
        return S_FALSE;
    m_xy_size_opt[field] = value;
    return S_OK;
}

STDMETHODIMP XyOptionsImpl::XySetRect(unsigned field, RECT value)
{
    if (field >= MAX_OPTION_ID || m_option_type[field] != OPTION_TYPE_RECT)
        return E_INVALIDARG;
    if (m_xy_rect_opt[field] == value)
        return S_FALSE;
    m_xy_rect_opt[field] = value;
    return S_OK;
}

STDMETHODIMP XyOptionsImpl::XySetUlonglong(unsigned field, ULONGLONG value)
{
    if (field >= MAX_OPTION_ID || m_option_type[field] != OPTION_TYPE_ULONGLONG)
        return E_INVALIDARG;
    if (m_xy_ulonglong_opt[field] == value)
        return S_FALSE;
    m_xy_ulonglong_opt[field] = value;
    return S_OK;
}

STDMETHODIMP XyOptionsImpl::XySetDouble(unsigned field, double value)
{
    if (field >= MAX_OPTION_ID || m_option_type[field] != OPTION_TYPE_DOUBLE)
        return E_INVALIDARG;
    if (m_xy_double_opt[field] == value)
        return S_FALSE;
    m_xy_double_opt[field] = value;
    return S_OK;
}

STDMETHODIMP XyOptionsImpl::XySetString(unsigned field, LPWSTR value, int chars)
{
    if (field >= MAX_OPTION_ID || m_option_type[field] != OPTION_TYPE_STRING)
        return E_INVALIDARG;
    if (m_xy_str_opt[field] == CStringW(value, chars))
        return S_FALSE;
    if (chars<0)
    {
        m_xy_str_opt[field] = "";
    }
    m_xy_str_opt[field].SetString(value, chars);
    return S_OK;
}

STDMETHODIMP XyOptionsImpl::XySetBin(unsigned field, LPVOID value, int size)
{
    if (field >= MAX_OPTION_ID || m_option_type[field] != OPTION_TYPE_BIN)
        return E_INVALIDARG;
    return E_NOTIMPL;
}
