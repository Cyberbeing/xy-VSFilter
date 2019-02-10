/*
 *   Copyright(C) 2016-2017 Blitzker
 *
 *   This program is free software : you can redistribute it and / or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <ass.h>

class SubFrame final
    : public CUnknown
    , public ISubRenderFrame
{
public:

    SubFrame(RECT rect, ULONGLONG id, ASS_Image* image);

    DECLARE_IUNKNOWN;

    // CUnknown
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv) override;

    // ISubRenderFrame
    STDMETHODIMP GetOutputRect(RECT* outputRect) override;
    STDMETHODIMP GetClipRect(RECT* clipRect) override;
    STDMETHODIMP GetBitmapCount(int* count) override;
    STDMETHODIMP GetBitmap(int index, ULONGLONG* id, POINT* position, SIZE* size, LPCVOID* pixels, int* pitch) override;

private:

    void Flatten(ASS_Image* image);

    const RECT m_rect;
    const ULONGLONG m_id;

    std::unique_ptr<uint32_t[]> m_pixels;
    RECT m_pixelsRect;
};
