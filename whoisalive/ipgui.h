#ifndef IPGUI_H
#define IPGUI_H

#include "../common/my_inet.h"
#include "../common/my_exception.h"

#include <minmax.h>
#include <windows.h>
#define GDIPVER 0x0110
#include <gdiplus.h> /* Gdi+ */

#include <string>

/* Вернуть rect, вмещающий в себя оба заданных rect'а */
Gdiplus::RectF maxrect(const Gdiplus::RectF &rect1,
		const Gdiplus::RectF &rect2);

/* Перевод "плавающего" rect'а в целочисленный.
	Новый rect включает (!) в себя старый */
Gdiplus::Rect rectF_to_rect(const Gdiplus::RectF &rect);

Gdiplus::Bitmap *image_optimize(Gdiplus::Image *image);

const wchar_t *GetGdiplusErr(Gdiplus::Status status);

std::wstring WinErrorText(DWORD error_id);

#endif
