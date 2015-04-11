/* 
 *	Copyright (C) 2003-2006 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "stdafx.h"

#include <atlbase.h>
#include <afxinet.h>

#include "TextFile.h"
#include "Utf8.h"

CTextFile::CTextFile(enc e)
: m_encoding(e)
, m_defaultencoding(e)
, m_offset(0)
, m_bufferPos(0)
, m_bufferCount(0)
{
}

bool CTextFile::Open(LPCTSTR lpszFileName)
{
	if (!__super::Open(lpszFileName, modeRead | typeBinary | shareDenyNone)) {
		return false;
	}

	m_encoding = m_defaultencoding;
	m_offset = 0;
	m_bufferCount = false;

	if(__super::GetLength() >= 2)
	{
		WORD w;
		if(sizeof(w) != Read(&w, sizeof(w)))
			return Close(), false;

		if(w == 0xfeff)
		{
			m_encoding = LE16;
			m_offset = 2;
		}
		else if(w == 0xfffe)
		{
			m_encoding = BE16;
			m_offset = 2;
		}
		else if(w == 0xbbef && __super::GetLength() >= 3)
		{
			BYTE b;
			if(sizeof(b) != Read(&b, sizeof(b)))
				return Close(), false;

			if(b == 0xbf)
			{
				m_encoding = UTF8;
				m_offset = 3;
			}
		}
	}

	if (m_offset == 0) { // No BOM detected, ensure the file is read from the beginning
		Seek(0, begin);
	}

	return true;
}

bool CTextFile::Save(LPCTSTR lpszFileName, enc e)
{
	if(!__super::Open(lpszFileName, modeCreate|modeWrite|shareDenyWrite|(e==DEFAULT_ENCODING?typeText:typeBinary)))
		return(false);

	if(e == UTF8)
	{
		BYTE b[3] = {0xef,0xbb,0xbf};
		Write(b, sizeof(b));
	}
	else if(e == LE16)
	{
		BYTE b[2] = {0xff,0xfe};
		Write(b, sizeof(b));
	}
	else if(e == BE16)
	{
		BYTE b[2] = {0xfe,0xff};
		Write(b, sizeof(b));
	}

	m_encoding = e;

	return true;
}

void CTextFile::SetEncoding(enc e)
{
	m_encoding = e;
}

CTextFile::enc CTextFile::GetEncoding()
{
	return m_encoding;
}

bool CTextFile::IsUnicode()
{
	return m_encoding != DEFAULT_ENCODING;
}

// CFile

CString CTextFile::GetFilePath() const
{
	// to avoid a CException coming from CTime
	return m_strFileName; // __super::GetFilePath();
}

// CStdioFile

ULONGLONG CTextFile::GetPosition() const
{
	return (__super::GetPosition() - m_offset - (m_bufferCount ? m_bufferCount - m_bufferPos : 0));
}

ULONGLONG CTextFile::GetLength() const
{
	return (__super::GetLength() - m_offset);
}

ULONGLONG CTextFile::Seek(LONGLONG lOff, UINT nFrom)
{
	ULONGLONG pos = GetPosition();
	ULONGLONG len = GetLength();

	switch(nFrom)
	{
	default:
	case begin: lOff = lOff; break;
	case current: lOff = pos + lOff; break;
	case end: lOff = len - lOff; break;
	}

	lOff = max((LONGLONG)min((ULONGLONG)lOff, len), 0ll) + m_offset;

	pos = __super::Seek(lOff, begin) - m_offset;
	m_bufferCount = 0;

	return pos;
}

void CTextFile::WriteString(LPCWSTR lpsz/*CStringW str*/)
{
	CStringW str(lpsz);

	if(m_encoding == DEFAULT_ENCODING)
	{
		__super::WriteString(str);
	}
	else if(m_encoding == UTF8)
	{
		str.Replace(L"\n", L"\r\n");
		CW2AEX<1024> utf8(str, CP_UTF8);
		Write(utf8, ::strlen(utf8));
	}
	else if(m_encoding == LE16)
	{
		str.Replace(L"\n", L"\r\n");
		Write((LPCWSTR)str, str.GetLength()*2);
	}
	else if(m_encoding == BE16)
	{
		str.Replace(L"\n", L"\r\n");
		for (unsigned int i = 0, l = str.GetLength(); i < l; i++) {
			str.SetAt(i, ((str[i] >> 8) & 0x00ff) | ((str[i] << 8) & 0xff00));
		}
		Write((LPCWSTR)str, str.GetLength() * 2);
	}
}

bool CTextFile::ReadLine() {
	m_convertedBuffer.clear();
	ULONGLONG startPos = GetPosition();

	while (true) {
		if (m_bufferPos >= m_bufferCount) {
			m_bufferCount = Read(m_readbuffer, sizeof(m_readbuffer));
			if (!m_bufferCount) return true;
			m_bufferPos = 0;
		}

		int c = NextChar();
		if (c == '\r') continue;
		if (c == '\n') return false;
		if (c >= 0 && c < 0x10000) {
			m_convertedBuffer.push_back((wchar_t)c);
			continue;
		}
		if (c > 0 && c <= 0x10FFFF) {
			c -= 0x10000;
			m_convertedBuffer.push_back(0xD800 | (c>>10));
			m_convertedBuffer.push_back(0xDC00 | (c&0x3FF));
			continue;
		}

		if (c == CHARERR_NEED_MORE) {
			size_t count = m_bufferCount - m_bufferPos;
			memmove(m_readbuffer, m_readbuffer + m_bufferPos, count);
			m_bufferCount = count + Read(m_readbuffer + count, sizeof(m_readbuffer) - count);
			m_bufferPos = 0;
			if (m_bufferCount == count) return true;
			continue;
		}

		if (c == CHARERR_REOPEN) {
			if (m_offset) {
				// File had a BOM, so don't try to reopen as DEFAULT_ENCODING
				c = '?';
			}
			else {
				// Switch to DEFAULT_ENCODING and read the line again
				m_encoding = DEFAULT_ENCODING;
				m_convertedBuffer.clear();
				Seek(startPos, begin);
				continue;
			}
		}

		XY_LOG_ERROR("This character code '" << c << "'is out of UTF-16 accessible range");
		m_convertedBuffer.push_back(L'?');
	}
}

int CTextFile::NextChar() {
	if (m_encoding == UTF8) {
		unsigned char c = m_readbuffer[m_bufferPos++];
		if (Utf8::isSingleByte(c)) return c;
		if (!Utf8::isFirstOfMultibyte(c)) return CHARERR_REOPEN;

		int nContinuationBytes = Utf8::continuationBytes(c);
		if (m_bufferCount - m_bufferPos < nContinuationBytes) {
			--m_bufferPos;
			return CHARERR_NEED_MORE;
		}

		unsigned char *rdbuf = m_readbuffer + m_bufferPos - 1;
		m_bufferPos += nContinuationBytes;

		switch (nContinuationBytes) {
		case 1: // 110xxxxx 10xxxxxx
			return (rdbuf[0] & 0x1f) << 6 | (rdbuf[1] & 0x3f);
		case 2: // 1110xxxx 10xxxxxx 10xxxxxx
			return (rdbuf[0] & 0x0f) << 12 | (rdbuf[1] & 0x3f) << 6 | (rdbuf[2] & 0x3f);
		case 3: // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
			return (rdbuf[0] & 0x07) << 18 | (rdbuf[1] & 0x3f) << 12 | (rdbuf[2] & 0x3f) << 6 | (rdbuf[3] & 0x3f);
		}
	}

	if (m_encoding == DEFAULT_ENCODING) {
		return m_readbuffer[m_bufferPos++];
	}

	if (m_encoding == LE16) {
		if (m_bufferCount - m_bufferPos < 2) return CHARERR_NEED_MORE;
		unsigned char c1 = m_readbuffer[m_bufferPos++];
		unsigned char c2 = m_readbuffer[m_bufferPos++];
		return (c2 << 8) + c1;
	}

	if (m_encoding == BE16) {
		if (m_bufferCount - m_bufferPos < 2) return CHARERR_NEED_MORE;
		unsigned char c1 = m_readbuffer[m_bufferPos++];
		unsigned char c2 = m_readbuffer[m_bufferPos++];
		return (c1 << 8) + c2;
	}

	return CHARERR_REOPEN;
}

BOOL CTextFile::ReadString(CStringW& str) {
	bool fEOF = ReadLine();

	str.Empty();
	if (!m_convertedBuffer.empty()) {
		str.Append(&m_convertedBuffer[0], m_convertedBuffer.size());
	}
	return !fEOF;
}

//
// CWebTextFile
//
CWebTextFile::CWebTextFile(CTextFile::enc e, LONGLONG llMaxSize)
: CTextFile(e)
, m_llMaxSize(llMaxSize)
{
}

bool CWebTextFile::Open(LPCTSTR lpszFileName)
{
	CString fn(lpszFileName);

	if(fn.Find(_T("http://")) != 0)
		return __super::Open(lpszFileName);

	try
	{
		CInternetSession is;

		CAutoPtr<CStdioFile> f(is.OpenURL(fn, 1, INTERNET_FLAG_TRANSFER_BINARY|INTERNET_FLAG_EXISTING_CONNECT));
		if(!f) return(false);

		TCHAR path[MAX_PATH];
		GetTempPath(MAX_PATH, path);

		fn = path + fn.Mid(fn.ReverseFind('/')+1);
		int i = fn.Find(_T("?"));
		if(i > 0) fn = fn.Left(i);
		CFile temp;
		if(!temp.Open(fn, modeCreate|modeWrite|typeBinary|shareDenyWrite))
		{
			f->Close();
			return(false);
		}

		BYTE buff[1024];
		int len, total = 0;
		while((len = f->Read(buff, 1024)) == 1024 && (m_llMaxSize < 0 || (total+=1024) < m_llMaxSize))
			temp.Write(buff, len);
		if(len > 0) temp.Write(buff, len);

		m_tempfn = fn;

		f->Close(); // must close it because the desctructor doesn't seem to do it and we will get an exception when "is" is destroying
	}
	catch(CInternetException* ie)
	{
		ie->Delete();
		return(false);
	}

	return __super::Open(m_tempfn);
}

bool CWebTextFile::Save(LPCTSTR lpszFileName, enc e)
{
	// CWebTextFile is read-only...
	ASSERT(0);
	return(false);
}

void CWebTextFile::Close()
{
	__super::Close();

	if(!m_tempfn.IsEmpty())
	{
		_tremove(m_tempfn);
		m_tempfn.Empty();
	}
}
