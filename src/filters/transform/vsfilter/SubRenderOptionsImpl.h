#pragma once

#include "SubRenderIntf.h"

interface IXyOptions;

class SubRenderOptionsImpl : public ISubRenderOptions
{
public:
    enum OptionType
    {
        OPTION_TYPE_BOOL,
        OPTION_TYPE_INT,
        OPTION_TYPE_SIZE,
        OPTION_TYPE_RECT,
        OPTION_TYPE_ULONGLONG,
        OPTION_TYPE_DOUBLE,
        OPTION_TYPE_STRING,
        OPTION_TYPE_BIN
    };

    struct OptionMap
    {
        LPCSTR str_name;
        OptionType type;
        unsigned int_name;
    };

public:
    SubRenderOptionsImpl(const OptionMap *optionMap, IXyOptions *impl) : _optionMap(optionMap), _impl(impl){};
    virtual ~SubRenderOptionsImpl(void) {};

    // ISubRenderOptions
    STDMETHODIMP GetBool     (LPCSTR field, bool      *value);
    STDMETHODIMP GetInt      (LPCSTR field, int       *value);
    STDMETHODIMP GetSize     (LPCSTR field, SIZE      *value);
    STDMETHODIMP GetRect     (LPCSTR field, RECT      *value);
    STDMETHODIMP GetUlonglong(LPCSTR field, ULONGLONG *value);
    STDMETHODIMP GetDouble   (LPCSTR field, double    *value);
    STDMETHODIMP GetString   (LPCSTR field, LPWSTR    *value, int *chars);
    STDMETHODIMP GetBin      (LPCSTR field, LPVOID    *value, int *size );

    STDMETHODIMP SetBool     (LPCSTR field, bool      value);
    STDMETHODIMP SetInt      (LPCSTR field, int       value);
    STDMETHODIMP SetSize     (LPCSTR field, SIZE      value);
    STDMETHODIMP SetRect     (LPCSTR field, RECT      value);
    STDMETHODIMP SetUlonglong(LPCSTR field, ULONGLONG value);
    STDMETHODIMP SetDouble   (LPCSTR field, double    value);
    STDMETHODIMP SetString   (LPCSTR field, LPWSTR    value, int chars);
    STDMETHODIMP SetBin      (LPCSTR field, LPVOID    value, int size );
private:
    int GetIFiled(LPCSTR field, OptionType type);

    IXyOptions *_impl;
    const OptionMap *_optionMap;
};

#define DECLARE_ISUBRENDEROPTIONS                                                                                                         \
    STDMETHODIMP GetBool     (LPCSTR field, bool      *value) { return SubRenderOptionsImpl::GetBool(field, value); }                      \
    STDMETHODIMP GetInt      (LPCSTR field, int       *value) { return SubRenderOptionsImpl::GetInt(field, value); }                       \
    STDMETHODIMP GetSize     (LPCSTR field, SIZE      *value) { return SubRenderOptionsImpl::GetSize(field, value); }                      \
    STDMETHODIMP GetRect     (LPCSTR field, RECT      *value) { return SubRenderOptionsImpl::GetRect(field, value); }                      \
    STDMETHODIMP GetUlonglong(LPCSTR field, ULONGLONG *value) { return SubRenderOptionsImpl::GetUlonglong(field, value); }                 \
    STDMETHODIMP GetDouble   (LPCSTR field, double    *value) { return SubRenderOptionsImpl::GetDouble(field, value); }                    \
    STDMETHODIMP GetString   (LPCSTR field, LPWSTR    *value, int *chars) { return SubRenderOptionsImpl::GetString(field, value, chars); } \
    STDMETHODIMP GetBin      (LPCSTR field, LPVOID    *value, int *size ) { return SubRenderOptionsImpl::GetBin(field, value, size); }     \
    \
    STDMETHODIMP SetBool     (LPCSTR field, bool      value) { return SubRenderOptionsImpl::SetBool(field, value); }                       \
    STDMETHODIMP SetInt      (LPCSTR field, int       value) { return SubRenderOptionsImpl::SetInt(field, value); }                        \
    STDMETHODIMP SetSize     (LPCSTR field, SIZE      value) { return SubRenderOptionsImpl::SetSize(field, value); }                       \
    STDMETHODIMP SetRect     (LPCSTR field, RECT      value) { return SubRenderOptionsImpl::SetRect(field, value); }                       \
    STDMETHODIMP SetUlonglong(LPCSTR field, ULONGLONG value) { return SubRenderOptionsImpl::SetUlonglong(field, value); }                  \
    STDMETHODIMP SetDouble   (LPCSTR field, double    value) { return SubRenderOptionsImpl::SetDouble(field, value); }                     \
    STDMETHODIMP SetString   (LPCSTR field, LPWSTR    value, int chars) { return SubRenderOptionsImpl::SetString(field, value, chars); }   \
    STDMETHODIMP SetBin      (LPCSTR field, LPVOID    value, int size ) { return SubRenderOptionsImpl::SetBin(field, value, size); }
