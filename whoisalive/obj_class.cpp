#include "obj_class.h"
#include "server.h"
#include "ipaddr.h"

#include "../common/my_http.h"
#include "../common/my_str.h"
#include "../common/my_fs.h"

#include <boost/property_tree/xml_parser.hpp>

using namespace std;

namespace who {

obj_class::obj_class(who::server &server, const xml::wptree &config)
	: server_(server)
	, config_(config)
	, w_(0.0f)
	, h_(0.0f)
	, xc_(0.5f)
	, yc_(0.5f)
{
	try
	{
		name_ = config_.get<wstring>(L"<xmlattr>.name");

		pair<xml::wptree::assoc_iterator,
			xml::wptree::assoc_iterator> p
				= config_.equal_range(L"image");

		while (p.first != p.second)
		{
			xml::wptree &node = p.first->second;

			wstring file_remote = node.get_value<wstring>();
			wstring file = L"." + file_remote;
			if (!fs::exists(file))
				server_.load_file(file_remote, file);

			Gdiplus::Image image(file.c_str());
			
			Gdiplus::Status status = image.GetLastStatus();
			if (status != Gdiplus::Ok)
				throw my::exception(L"Не удалось открыть изображение")
					//<< my::param(L"gdiplus-status", status)
					<< my::param(L"file", file);

			who::bitmap_ptr bitmap( image_optimize(&image) );
			
			wstring state = node.get<wstring>(L"<xmlattr>.state", L"");

			if (state == L"ok")
				bitmap_[ipstate::ok] = bitmap;
			else if (state == L"warn")
				bitmap_[ipstate::warn] = bitmap;
			else if (state == L"fail")
				bitmap_[ipstate::fail] = bitmap;
			else if (state.empty())
			{
				bitmap_[ipstate::unknown] = bitmap;

				w_ = (float)bitmap->GetWidth();
				h_ = (float)bitmap->GetHeight();

				float scale = node.get<float>(L"<xmlattr>.scale", 1.0f);
				w_ *= scale;
				h_ *= scale;

				xc_ = node.get<float>(L"<xmlattr>.xc", xc_);
				yc_ = node.get<float>(L"<xmlattr>.yc", yc_);
			}
			else
				throw my::exception(L"Неизвестное состояние")
					<< my::param(L"state", state);

			p.first++;
		}
	}
	catch (my::exception &e)
	{
		throw my::exception(L"Ошибка создания класса")
			<< e;
	}
	catch (exception &e)
	{
		throw my::exception(L"Ошибка создания класса")
			<< e;
	}
}

/*-
void obj_class::paint( Gdiplus::Graphics *canvas, float offs_x, float offs_y,
		float scale, float x, float y, float w, float h)
{
	if (bitmap_.get()) {
		Gdiplus::RectF c_rect(
				offs_x + x * scale,
				offs_y + y * scale,
				w * scale,
				h * scale);

		Gdiplus::ImageAttributes ia;

		Gdiplus::ColorMatrix cm = {
			0.0f, 0.2f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.2f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.2f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.4f, 0.0f, 0.0f, 1.0f};

		ia.SetColorMatrix( &cm,
				Gdiplus::ColorMatrixFlagsDefault,
				Gdiplus::ColorAdjustTypeDefault);

		canvas->DrawImage( bitmap_.get(), c_rect,
				0.0f, 0.0f, (float)bitmap_->GetWidth(), (float)bitmap_->GetHeight(),
				Gdiplus::UnitPixel, &ia, NULL, NULL);
	}
}
-*/

}
