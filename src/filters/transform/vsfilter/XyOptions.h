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
    STDMETHOD(XyGetBin2     )(unsigned field, void      *value, int  size ) = 0;

    STDMETHOD(XySetBool     )(unsigned field, bool      value) = 0;
    STDMETHOD(XySetInt      )(unsigned field, int       value) = 0;
    STDMETHOD(XySetSize     )(unsigned field, SIZE      value) = 0;
    STDMETHOD(XySetRect     )(unsigned field, RECT      value) = 0;
    STDMETHOD(XySetUlonglong)(unsigned field, ULONGLONG value) = 0;
    STDMETHOD(XySetDouble   )(unsigned field, double    value) = 0;
    STDMETHOD(XySetString   )(unsigned field, LPWSTR    value, int chars) = 0;
    STDMETHOD(XySetBin      )(unsigned field, LPVOID    value, int size ) = 0;

    //methods for array
    STDMETHOD(XyGetBools     )(unsigned field, int id, bool      *value) = 0;
    STDMETHOD(XyGetInts      )(unsigned field, int id, int       *value) = 0;
    STDMETHOD(XyGetSizes     )(unsigned field, int id, SIZE      *value) = 0;
    STDMETHOD(XyGetRects     )(unsigned field, int id, RECT      *value) = 0;
    STDMETHOD(XyGetUlonglongs)(unsigned field, int id, ULONGLONG *value) = 0;
    STDMETHOD(XyGetDoubles   )(unsigned field, int id, double    *value) = 0;
    STDMETHOD(XyGetStrings   )(unsigned field, int id, LPWSTR    *value, int *chars) = 0;
    STDMETHOD(XyGetBins      )(unsigned field, int id, LPVOID    *value, int *size ) = 0;
    STDMETHOD(XyGetBins2     )(unsigned field, int id, void      *value, int  size ) = 0;

    STDMETHOD(XySetBools     )(unsigned field, int id, bool      value) = 0;
    STDMETHOD(XySetInts      )(unsigned field, int id, int       value) = 0;
    STDMETHOD(XySetSizes     )(unsigned field, int id, SIZE      value) = 0;
    STDMETHOD(XySetRects     )(unsigned field, int id, RECT      value) = 0;
    STDMETHOD(XySetUlonglongs)(unsigned field, int id, ULONGLONG value) = 0;
    STDMETHOD(XySetDoubles   )(unsigned field, int id, double    value) = 0;
    STDMETHOD(XySetStrings   )(unsigned field, int id, LPWSTR    value, int chars) = 0;
    STDMETHOD(XySetBins      )(unsigned field, int id, LPVOID    value, int size ) = 0;
};
