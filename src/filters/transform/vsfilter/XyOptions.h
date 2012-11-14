#pragma once

[uuid("85E5D6F9-BEFB-4E01-B047-758359CDF9AB")]
interface IXyOptions : public IUnknown 
{
    STDMETHOD(XyGetBool     )(unsigned field, bool      *value) = 0;
    STDMETHOD(XyGetInt      )(unsigned field, int       *value) = 0;
    STDMETHOD(XyGetSize     )(unsigned field, SIZE      *value) = 0;
    STDMETHOD(XyGetRect     )(unsigned field, RECT      *value) = 0;
    STDMETHOD(XyGetUlonglong)(unsigned field, ULONGLONG *value) = 0;
    STDMETHOD(XyGetDouble   )(unsigned field, double    *value) = 0;
    STDMETHOD(XyGetString   )(unsigned field, LPWSTR    *value, int *chars) = 0;
    STDMETHOD(XyGetBin      )(unsigned field, LPVOID    *value, int *size ) = 0;

    STDMETHOD(XySetBool     )(unsigned field, bool      value) = 0;
    STDMETHOD(XySetInt      )(unsigned field, int       value) = 0;
    STDMETHOD(XySetSize     )(unsigned field, SIZE      value) = 0;
    STDMETHOD(XySetRect     )(unsigned field, RECT      value) = 0;
    STDMETHOD(XySetUlonglong)(unsigned field, ULONGLONG value) = 0;
    STDMETHOD(XySetDouble   )(unsigned field, double    value) = 0;
    STDMETHOD(XySetString   )(unsigned field, LPWSTR    value, int chars) = 0;
    STDMETHOD(XySetBin      )(unsigned field, LPVOID    value, int size ) = 0;
};
