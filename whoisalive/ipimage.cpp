#include "ipimage.h"
#include "ipmap.h"
#include "server.h"

using namespace std;

ipimage_t::ipimage_t(who::server &server, const xml::wptree *pt)
	: ipwidget_t(server, pt)
	, w_(0.0f)
	, h_(0.0f)
{
	try
	{
		if (pt)
		{
			wstring file = pt->get<wstring>(L"<xmlattr>.filename");

			image_.reset( new Gdiplus::Image(file.c_str()) );
			if (!image_.get())
				throw my::exception(L"Ошибка инициализации Gdi+");

			Gdiplus::Status status = image_->GetLastStatus();
			if (status != Gdiplus::Ok)
				throw my::exception(L"Не удалось загрузить изображение из файла")
					<< my::param(L"file", file);

			w_ = (float)image_->GetWidth();
			h_ = (float)image_->GetHeight();
			bitmap_.reset( image_optimize(image_.get()) );

			w_ *= scale_;
			h_ *= scale_;
			scale_ = 1.0f;

			w_ = pt->get<float>(L"<xmlattr>.width", w_);
			h_ = pt->get<float>(L"<xmlattr>.height", h_);
		}
	}
	catch (exception &e)
	{
		throw my::exception(L"Ошибка создания изображения")
			<< my::param( L"what", my::str::to_wstring(e.what()) )
			<< my::param( L"xml", xmlattr_to_str(*pt) );
	}
}

/******************************************************************************
*/
Gdiplus::RectF ipimage_t::own_rect( void)
{
	return Gdiplus::RectF( -w_/2.0f, -h_/2.0f, w_, h_);
}

/******************************************************************************
*/
void ipimage_t::paint_self( Gdiplus::Graphics *canvas)
{
	if (bitmap_.get()) {
		Gdiplus::RectF rect = own_rect();
		client_to_window(&rect);

		Gdiplus::ImageAttributes ia;

		const Gdiplus::ColorMatrix static_matrix = {
			1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
		
		Gdiplus::ColorMatrix matrix = static_matrix;
		matrix.m[3][3] = alpha();

		ia.SetColorMatrix(&matrix,
				Gdiplus::ColorMatrixFlagsDefault,
				Gdiplus::ColorAdjustTypeDefault);

#define CLIP
#ifdef CLIP
		{
			float dx = -rect.X;
			float dy = -rect.Y;
			float x = 0.0f;
			float y = 0.0f;
			float w = (float)bitmap_->GetWidth();
			float h = (float)bitmap_->GetHeight();

			if (dx > 0.0f) {
				x = dx / rect.Width * w;
				w -= x;
				rect.X = 0.0f;
				rect.Width -= dx;
			}
		
			if (dy > 0.0f) {
				y = dy / rect.Height * h;
				h -= y;
				rect.Y = 0.0f;
				rect.Height -= dy;
			}

			float ww = (float)window()->w();
			float wh = (float)window()->h();
			if (rect.Width > ww) {
				w -= (rect.Width - ww) / rect.Width * w;
				rect.Width = ww;
			}

			if (rect.Height > wh) {
				h -= (rect.Height - wh) / rect.Height * h;
				rect.Height = wh;
			}

			canvas->DrawImage( bitmap_.get(), rect, x, y, w, h,
					Gdiplus::UnitPixel, alpha_ == 1.0f ? NULL : &ia, NULL, NULL );
		}
#else
		{
			canvas->DrawImage( bitmap_.get(), rect,
					0.0f, 0.0f, (float)bitmap_->GetWidth(), (float)bitmap_->GetHeight(),
					Gdiplus::UnitPixel, alpha_ == 1.0f ? NULL : &ia, NULL, NULL );
		}
#endif // CLIP
#undef CLIP
	}
}

/******************************************************************************
*/
ipwidget_t* ipimage_t::hittest(float x, float y)
{
	ipwidget_t *widget = ipwidget_t::hittest(x, y);

	if (!widget) {
		Gdiplus::RectF rect = client_rect();

		int bmp_w = bitmap_->GetWidth();
		int bmp_h = bitmap_->GetHeight();
		int ix = (int)( (x - rect.X) * bmp_w / rect.Width + 0.5f );
		int iy = (int)( (y - rect.Y) * bmp_h / rect.Height + 0.5f );

		if (ix>=0 && ix<bmp_w && iy>=0 && iy<bmp_h) {

			Gdiplus::Color color;
			bitmap_->GetPixel(ix, iy, &color);

			if (color.GetA() >= 128) widget = this;
		}
	}

	return widget;
}

/******************************************************************************
*/
bool ipimage_t::animate_calc(void)
{
	return ipwidget_t::animate_calc();
}
