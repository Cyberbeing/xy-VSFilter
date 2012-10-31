#include "stdafx.h"
#include "SubRenderOptionsImpl.h"

int SubRenderOptionsImpl::GetIFiled( LPCSTR field, OptionType type )
{
    if (!_optionMap)
    {
        return -1;
    }
    const OptionMap * map = _optionMap;
    while( map->str_name && _stricmp(field, map->str_name) )
        map++;
    return (map->str_name && map->type==type) ? map->int_name : -1;
}

STDMETHODIMP SubRenderOptionsImpl::GetBool(LPCSTR field, bool *value)
{
    int ifield = GetIFiled(field, OPTION_TYPE_BOOL);
    CheckPointer(_impl, E_POINTER);
    return _impl->XyGetBool(ifield, value);
}

STDMETHODIMP SubRenderOptionsImpl::GetInt(LPCSTR field, int *value)
{
    int ifield = GetIFiled(field, OPTION_TYPE_INT);
    CheckPointer(_impl, E_POINTER);
    return _impl->XyGetInt(ifield, value);
}

STDMETHODIMP SubRenderOptionsImpl::GetSize(LPCSTR field, SIZE *value)
{
    int ifield = GetIFiled(field, OPTION_TYPE_SIZE);
    CheckPointer(_impl, E_POINTER);
    return _impl->XyGetSize(ifield, value);
}

STDMETHODIMP SubRenderOptionsImpl::GetRect(LPCSTR field, RECT *value)
{
    int ifield = GetIFiled(field, OPTION_TYPE_RECT);
    CheckPointer(_impl, E_POINTER);
    return _impl->XyGetRect(ifield, value);
}

STDMETHODIMP SubRenderOptionsImpl::GetUlonglong(LPCSTR field, ULONGLONG *value)
{
    int ifield = GetIFiled(field, OPTION_TYPE_ULONGLONG);
    CheckPointer(_impl, E_POINTER);
    return _impl->XyGetUlonglong(ifield, value);
}

STDMETHODIMP SubRenderOptionsImpl::GetDouble(LPCSTR field, double *value)
{
    int ifield = GetIFiled(field, OPTION_TYPE_DOUBLE);
    CheckPointer(_impl, E_POINTER);
    return _impl->XyGetDouble(ifield, value);
}

STDMETHODIMP SubRenderOptionsImpl::GetString(LPCSTR field, LPWSTR *value, int *chars)
{
    int ifield = GetIFiled(field, OPTION_TYPE_STRING);
    CheckPointer(_impl, E_POINTER);
    return _impl->XyGetString(ifield, value, chars);
}

STDMETHODIMP SubRenderOptionsImpl::GetBin(LPCSTR field, LPVOID *value, int *size)
{
    int ifield = GetIFiled(field, OPTION_TYPE_BIN);
    CheckPointer(_impl, E_POINTER);
    return _impl->XyGetBin(ifield, value, size);
}

STDMETHODIMP SubRenderOptionsImpl::SetBool(LPCSTR field, bool value)
{
    int ifield = GetIFiled(field, OPTION_TYPE_BOOL);
    CheckPointer(_impl, E_POINTER);
    return _impl->XySetBool(ifield, value);
}

STDMETHODIMP SubRenderOptionsImpl::SetInt(LPCSTR field, int value)
{
    int ifield = GetIFiled(field, OPTION_TYPE_INT);
    CheckPointer(_impl, E_POINTER);
    return _impl->XySetInt(ifield, value);
}

STDMETHODIMP SubRenderOptionsImpl::SetSize(LPCSTR field, SIZE value)
{
    int ifield = GetIFiled(field, OPTION_TYPE_SIZE);
    CheckPointer(_impl, E_POINTER);
    return _impl->XySetSize(ifield, value);
}

STDMETHODIMP SubRenderOptionsImpl::SetRect(LPCSTR field, RECT value)
{
    int ifield = GetIFiled(field, OPTION_TYPE_RECT);
    CheckPointer(_impl, E_POINTER);
    return _impl->XySetRect(ifield, value);
}

STDMETHODIMP SubRenderOptionsImpl::SetUlonglong(LPCSTR field, ULONGLONG value)
{
    int ifield = GetIFiled(field, OPTION_TYPE_ULONGLONG);
    CheckPointer(_impl, E_POINTER);
    return _impl->XySetUlonglong(ifield, value);
}

STDMETHODIMP SubRenderOptionsImpl::SetDouble(LPCSTR field, double value)
{
    int ifield = GetIFiled(field, OPTION_TYPE_DOUBLE);
    return _impl->XySetDouble(ifield, value);
}

STDMETHODIMP SubRenderOptionsImpl::SetString(LPCSTR field, LPWSTR value, int chars)
{
    int ifield = GetIFiled(field, OPTION_TYPE_STRING);
    CheckPointer(_impl, E_POINTER);
    return _impl->XySetString(ifield, value, chars);
}

STDMETHODIMP SubRenderOptionsImpl::SetBin(LPCSTR field, LPVOID value, int size)
{
    int ifield = GetIFiled(field, OPTION_TYPE_BIN);
    CheckPointer(_impl, E_POINTER);
    return _impl->XySetBin(ifield, value, size);
}
