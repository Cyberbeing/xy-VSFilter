#pragma once

#include "XyOptions.h"

class XyOptionsImpl : public IXyOptions
{
public:
    static const int MAX_OPTION_ID = 63;

    enum OptionType
    {
        OPTION_TYPE_END_FLAG = 0,
        OPTION_TYPE_BOOL,
        OPTION_TYPE_INT,
        OPTION_TYPE_SIZE,
        OPTION_TYPE_RECT,
        OPTION_TYPE_ULONGLONG,
        OPTION_TYPE_DOUBLE,
        OPTION_TYPE_STRING,
        OPTION_TYPE_BIN,
        OPTION_TYPE_COUNT
    };

    struct Option
    {
        OptionType type;
        unsigned option_id;
    };
public:
    XyOptionsImpl(const Option *option_list);
    virtual ~XyOptionsImpl() {}

    STDMETHODIMP XyGetBool     (unsigned field, bool      *value);
    STDMETHODIMP XyGetInt      (unsigned field, int       *value);
    STDMETHODIMP XyGetSize     (unsigned field, SIZE      *value);
    STDMETHODIMP XyGetRect     (unsigned field, RECT      *value);
    STDMETHODIMP XyGetUlonglong(unsigned field, ULONGLONG *value);
    STDMETHODIMP XyGetDouble   (unsigned field, double    *value);
    STDMETHODIMP XyGetString   (unsigned field, LPWSTR    *value, int *chars);
    STDMETHODIMP XyGetBin      (unsigned field, LPVOID    *value, int *size );

    STDMETHODIMP XySetBool     (unsigned field, bool      value);
    STDMETHODIMP XySetInt      (unsigned field, int       value);
    STDMETHODIMP XySetSize     (unsigned field, SIZE      value);
    STDMETHODIMP XySetRect     (unsigned field, RECT      value);
    STDMETHODIMP XySetUlonglong(unsigned field, ULONGLONG value);
    STDMETHODIMP XySetDouble   (unsigned field, double    value);
    STDMETHODIMP XySetString   (unsigned field, LPWSTR    value, int chars);
    STDMETHODIMP XySetBin      (unsigned field, LPVOID    value, int size );
protected:
    OptionType m_option_type[MAX_OPTION_ID+1];
    
    int m_xy_int_opt[MAX_OPTION_ID+1];
    bool m_xy_bool_opt[MAX_OPTION_ID+1];
    double m_xy_double_opt[MAX_OPTION_ID+1];
    ULONGLONG m_xy_ulonglong_opt[MAX_OPTION_ID+1];
    CSize m_xy_size_opt[MAX_OPTION_ID+1];
    CRect m_xy_rect_opt[MAX_OPTION_ID+1];
    CStringW m_xy_str_opt[MAX_OPTION_ID+1];
};

#define DECLARE_IXYOPTIONS                                                                                                         \
    STDMETHODIMP XyGetBool     (unsigned field, bool      *value) { return XyOptionsImpl::XyGetBool(field, value); }                      \
    STDMETHODIMP XyGetInt      (unsigned field, int       *value) { return XyOptionsImpl::XyGetInt(field, value); }                       \
    STDMETHODIMP XyGetSize     (unsigned field, SIZE      *value) { return XyOptionsImpl::XyGetSize(field, value); }                      \
    STDMETHODIMP XyGetRect     (unsigned field, RECT      *value) { return XyOptionsImpl::XyGetRect(field, value); }                      \
    STDMETHODIMP XyGetUlonglong(unsigned field, ULONGLONG *value) { return XyOptionsImpl::XyGetUlonglong(field, value); }                 \
    STDMETHODIMP XyGetDouble   (unsigned field, double    *value) { return XyOptionsImpl::XyGetDouble(field, value); }                    \
    STDMETHODIMP XyGetString   (unsigned field, LPWSTR    *value, int *chars) { return XyOptionsImpl::XyGetString(field, value, chars); } \
    STDMETHODIMP XyGetBin      (unsigned field, LPVOID    *value, int *size ) { return XyOptionsImpl::XyGetBin(field, value, size); }     \
    \
    STDMETHODIMP XySetBool     (unsigned field, bool      value) { return XyOptionsImpl::XySetBool(field, value); }                       \
    STDMETHODIMP XySetInt      (unsigned field, int       value) { return XyOptionsImpl::XySetInt(field, value); }                        \
    STDMETHODIMP XySetSize     (unsigned field, SIZE      value) { return XyOptionsImpl::XySetSize(field, value); }                       \
    STDMETHODIMP XySetRect     (unsigned field, RECT      value) { return XyOptionsImpl::XySetRect(field, value); }                       \
    STDMETHODIMP XySetUlonglong(unsigned field, ULONGLONG value) { return XyOptionsImpl::XySetUlonglong(field, value); }                  \
    STDMETHODIMP XySetDouble   (unsigned field, double    value) { return XyOptionsImpl::XySetDouble(field, value); }                     \
    STDMETHODIMP XySetString   (unsigned field, LPWSTR    value, int chars) { return XyOptionsImpl::XySetString(field, value, chars); }   \
    STDMETHODIMP XySetBin      (unsigned field, LPVOID    value, int size ) { return XyOptionsImpl::XySetBin(field, value, size); }
