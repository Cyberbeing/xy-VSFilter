#pragma once

#include <atltypes.h>
#include <cmath>

#define  SIGN_OF(x) ((x)>0?1:(x)=0?0:-1)
#define  DISTANCE(x1,y1,x2,y2)	(abs(x1-x2)+abs(y1-y2))

class CRect2: public CRect
{
public:
	inline static int Compare(LPCRECT lpRect1, LPCRECT lpRect2)
	{
		int dLeft = lpRect1->left - lpRect2->left;
		if(dLeft!=0)
			return dLeft;
		else
		{
			int dTop = lpRect1->top - lpRect2->top;
			return dTop;
		}
	}
	inline static bool IsIntersect(LPCRECT lpRect1, LPCRECT lpRect2)
	{
		CRect r;
		return ::IntersectRect(r, lpRect1, lpRect2);
	}
	inline static int Area(LPCRECT lpRect)
	{
		return (lpRect->right-lpRect->left)*(lpRect->bottom-lpRect->top);
	}
};

class CPoint2: public CPoint
{
public:
	inline static int Distance(POINT p1, POINT p2)
	{
		return abs(p1.x - p2.x) + abs(p1.y - p2.y);
	}
	inline static int Distance(int x1, int y1, int x2, int y2)
	{
		return abs(x1-x2) + abs(y1-y2);
	}
};
