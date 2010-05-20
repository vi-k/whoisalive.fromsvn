#include "ipgui.h"

using namespace std;

/* Оптимизация картинки для вывода на экран */
Gdiplus::Bitmap *image_optimize( Gdiplus::Image *image)
{
	Gdiplus::Bitmap *bitmap = NULL;

	if (image) {
		int w = image->GetWidth();
		int h = image->GetHeight();
		if (w && h) {
			Gdiplus::Graphics g( (HWND)NULL, FALSE );

			bitmap = new Gdiplus::Bitmap(w, h, &g);

			Gdiplus::Graphics g2(bitmap);
			g2.DrawImage(image, 0, 0, w, h);
		}
	}

	return bitmap;
}

/* Текст ошибки Gdiplus */
const wchar_t *GetGdiplusErr( Gdiplus::Status status)
{
	const wchar_t *str_status[] = {
		L"Ok",
		L"GenericError",
		L"InvalidParameter",
		L"OutOfMemory",
		L"ObjectBusy",
		L"InsufficientBuffer",
		L"NotImplemented",
		L"Win32Error",
		L"WrongState",
		L"Aborted",
		L"FileNotFound",
		L"ValueOverflow",
		L"AccessDenied",
		L"UnknownImageFormat",
		L"FontFamilyNotFound",
		L"FontStyleNotFound",
		L"NotTrueTypeFont",
		L"UnsupportedGdiplusVersion",
		L"GdiplusNotInitialized",
		L"PropertyNotFound",
		L"PropertyNotSupported",
		L"ProfileNotFound"
	};

	if ((size_t)status > sizeof(str_status)) return L"Unknown error";

	return str_status[ status ];
}

/* */
Gdiplus::Rect rectF_to_rect(const Gdiplus::RectF &rect)
{
	/* Левая и верхняя граница равны либо меньше */
	int x = (int)rect.X;
	int y = (int)rect.Y;
	int r = (int)ceil(rect.X + rect.Width);
	int b = (int)ceil(rect.Y + rect.Height);
	return Gdiplus::Rect(x, y, r - x, b - y);
}

/* Текст Windows-ошибки */
wstring WinErrorText( DWORD error_id)
{
	wchar_t *tmp = NULL;
	wstring str;

	FormatMessageW( FORMAT_MESSAGE_FROM_SYSTEM
			| FORMAT_MESSAGE_ALLOCATE_BUFFER
			| FORMAT_MESSAGE_MAX_WIDTH_MASK,
			NULL, error_id,
			MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT),
			(wchar_t*)&tmp, 0, NULL);

	if (tmp) {
		str = tmp;
		LocalFree( tmp);
	}

	return str;
}

/* */
Gdiplus::RectF maxrect( const Gdiplus::RectF &rect1,
		const Gdiplus::RectF &rect2)
{
	Gdiplus::RectF rect;

	/* Правый нижний угол */
	float right = max(
			rect1.X + rect1.Width,
			rect2.X + rect2.Width);
	float bottom = max(
			rect1.Y + rect1.Height,
			rect2.Y + rect2.Height);

	/* Левый верхний угол */
	rect.X = min( rect1.X, rect2.X);
	rect.Y = min( rect1.Y, rect2.Y);

	/* Ширина и высота */
	rect.Width = right - rect.X;
	rect.Height = bottom - rect.Y;

	return rect;
}
