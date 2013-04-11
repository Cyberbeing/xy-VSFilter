#pragma once

#include "XyOptions.h"

class XyOptionsImpl : public IXyOptions
{
public:
    static const int MAX_OPTION_ID = 127;

    typedef int OptionType;
    static const int OPTION_TYPE_END_FLAG = 0;
    static const int OPTION_TYPE_BOOL     = 0x1;
    static const int OPTION_TYPE_INT      = 0x2;
    static const int OPTION_TYPE_SIZE     = 0x4;
    static const int OPTION_TYPE_RECT     = 0x8;
    static const int OPTION_TYPE_ULONGLONG= 0x10;
    static const int OPTION_TYPE_DOUBLE   = 0x20;
    static const int OPTION_TYPE_STRING   = 0x40;
    static const int OPTION_TYPE_BIN      = 0x800;

    typedef int OptionMode;
    static const int OPTION_MODE_READ   = 0x100;
    static const int OPTION_MODE_WRITE  = 0x200;
    static const int OPTION_MODE_RW     = 0x300;

    struct Option
    {
        OptionType type;
        OptionMode rw_mode;//read/write
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

    inline bool TestOption(unsigned field)
    {
        return (field < MAX_OPTION_ID && m_option_type[field] != OPTION_TYPE_END_FLAG);
    }
    inline bool TestOption(unsigned field, OptionType type, OptionMode rw_mode=OPTION_MODE_RW)
    {
        type |= rw_mode;
        return (field < MAX_OPTION_ID && (m_option_type[field] & type)== type);
    }
protected:
    virtual HRESULT OnOptionReading(unsigned field);
    virtual HRESULT OnOptionChanged(unsigned field);

protected:
    int m_option_type[MAX_OPTION_ID+1];

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
