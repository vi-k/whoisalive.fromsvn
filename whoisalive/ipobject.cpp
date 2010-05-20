#include "ipobject.h"
#include "server.h"
#include "obj_class.h"

#include <math.h>
#include <wchar.h>

#include <boost/foreach.hpp>
//#include <boost/spirit/include/classic_core.hpp> /* boost::spirit */
#include <boost/spirit/include/qi.hpp> /* boost::spirit:qi */

#ifdef _DEBUG
extern HWND g_parent_wnd; /* Для отладки */
#endif

#define PI 3.1415926535897932384626433832795f
#define EXCT 0.081819790992f

double atanh(double v)
{
	return 0.5 * log( (1.0 + v) / (1.0 - v) );
}

float atanh(float v)
{
	return 0.5f * log( (1.0f + v) / (1.0f - v) );
}

float lon2x(float lon)
{
	return 128.0f + lon / 180.0f * 128.0f;
}

float lat2y(float lat, int type = 2)
{
	float s = sin( lat * PI / 180.0f );

	switch (type) {
		case 1: return 128.0f * (1.0f - atanh(s) / PI);
		case 2: return 128.0f * (1.0f - (atanh(s)
			- EXCT * atanh(EXCT * s)) / PI);
	}

	return 0.0f;
}

float str2coord(const wchar_t *str)
{
	using namespace boost::spirit;
	float deg = 0;
	float min = 0;
	float sec = 0;

	qi::parse(str, str + wcslen(str),
		qi::float_ >> L' ' >>
		qi::float_ >> L' ' >>
		qi::float_,
		deg, min, sec);

	return deg + min / 60.0f + sec / 3600.0f;
}

const Gdiplus::ColorMatrix state_matrices[4] = {
	{	1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 1.0f   },

	{	0.0f, 0.2f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.2f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.2f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.4f, 0.0f, 0.0f, 1.0f   },

	{	0.2f, 0.2f, 0.0f, 0.0f, 0.0f,
		0.2f, 0.2f, 0.0f, 0.0f, 0.0f,
		0.2f, 0.2f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
		0.4f, 0.4f, 0.0f, 0.0f, 1.0f   },

	{	0.2f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.2f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.2f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
		0.4f, 0.0f, 0.0f, 0.0f, 1.0f   }
};

ipobject_t::ipobject_t(who::server &server, const xml::wptree *pt)
	: ipwidget_t(server, pt)
	, state_matrix_( state_matrices[state_.state()] )
	, new_state_matrix_( state_matrices[state_.state()] )
	, state_step_(0)
	, flash_step_(0)
	, flash_pause_(false)
	, flash_alpha_(1.0f)
	, flash_new_alpha_(1.0f)
	, link_()
	, offs_x_(0.0f)
	, offs_y_(0.0f)
	, link_type_(iplink_type::wire)
{
	try
	{
		if (pt)
		{
			xml::wptree attrs = pt->get_child(L"<xmlattr>");

			/* Определяем класс объекта */
			class_ = server.obj_class(attrs.get<wstring>(L"class"));

			if (!class_)
				throw my::exception(L"Не определён класс объекта");

			pair<xml::wptree::const_assoc_iterator,
				xml::wptree::const_assoc_iterator> p;
			
			p = attrs.equal_range(L"host");
			while (p.first != p.second)
			{
				ipaddr_t addr( (*p.first).second.data().c_str() );
				ipaddrs_.push_back(addr);
				server_.register_ipaddr(addr);
				p.first++;
			}

			p = pt->equal_range(L"host");
			while (p.first != p.second)
			{
				ipaddr_t addr( (*p.first).second.data().c_str() );
				ipaddrs_.push_back(addr);
				server_.register_ipaddr(addr);
				p.first++;
			}

			link_ = ipaddr_t( attrs.get<wstring>(L"link", L"").c_str() );

			wstring str = attrs.get<wstring>(L"link_type", L"");
			if (str == L"wire")
				link_type_ = iplink_type::wire;
			else if (str == L"optics")
				link_type_ = iplink_type::optics;
			else if (str == L"air")
				link_type_ = iplink_type::air;

			name_ = attrs.get<wstring>(L"name", L"");
			/* Если имя не задано, определяем его по первому попавшемуся ip-адресу */
			if (name_.empty() && ipaddrs_.size() > 0)
				name_ = (*ipaddrs_.begin()).str<wchar_t>();

			str = pt->get<wstring>(L"<xmlattr>.lon", L"");
			if (!str.empty()) {
				float lon = str2coord( str.c_str() );
				x_ = lon2x(lon);
			}
				
			str = pt->get<wstring>(L"<xmlattr>.lat", L"");
			if (!str.empty()) {
				float lat = str2coord( str.c_str() );
				y_ = lat2y(lat);
			}

			offs_x_ = pt->get<float>(L"<xmlattr>.offs_x", offs_x_);
			offs_y_ = pt->get<float>(L"<xmlattr>.offs_y", offs_y_);
		}
	}
	catch (my::exception &e)
	{
		throw my::exception(L"Ошибка создания объекта")
			<< my::param( L"xml", xmlattr_to_str(*pt) )
			<< e;
	}
	catch (exception &e)
	{
		throw my::exception(L"Ошибка создания объекта")
			<< my::param( L"xml", xmlattr_to_str(*pt) )
			<< e;
	}
}

ipobject_t::~ipobject_t()
{
	BOOST_FOREACH(const ipaddr_t &addr, ipaddrs_)
		server_.unregister_ipaddr(addr);
}

/******************************************************************************
*/
Gdiplus::RectF ipobject_t::own_rect(void)
{
	return Gdiplus::RectF(0.0f, 0.0f, 0.0f, 0.0f);

/*-
	return Gdiplus::RectF(
			-class_->w() * class_->xc(),
			-class_->h() * class_->yc(),
			class_->w(), class_->h());
-*/
}

/******************************************************************************
*/
Gdiplus::RectF ipobject_t::rect_norm( void)
{
	Gdiplus::RectF r;

	r.Width = class_->w();
	r.Height = class_->h();
	r.X = x_ - r.Width * class_->xc();
	r.Y = y_ - r.Height * class_->yc();

	return r;
}

/******************************************************************************
* Плюс к обычной анимации анимируем состояние (меняем цвета)
*/
bool ipobject_t::animate_calc( void)
{
	if (state_step_) {
		/* Плавно меняем состояние */
		for (int i = 0; i < 5; i++) {
			for (int j = 0; j < 5; j++) {
				state_matrix_.m[i][j] += (new_state_matrix_.m[i][j]
						- state_matrix_.m[i][j]) / state_step_;
			}
		}

		state_step_--;
	}

	/* Мигание для "мигающих" объектов */
	if (flash_step_) {
		flash_alpha_ += (flash_new_alpha_ - flash_alpha_) / flash_step_;

		if (--flash_step_ == 0) {
			flash_pause_ = !flash_pause_;		
			flash_step_ = server_.def_anim_steps(); //flash_pause_ ? server_.def_anim_steps() : 1;

			/* Выход из паузы */
			if (!flash_pause_) {
				/* Меняем "проявление" с "исчезанием" */
				if (flash_new_alpha_ == 0.0f) {
					flash_new_alpha_ = 1.0f;
				}
				else if (!acknowledged()) {
					flash_new_alpha_ = 0.0f;
				}
				/* Если поступил сигнал квитирования, прекращаем мигать */
				else flash_step_ = 0;
			}
		}
	}

	return ipwidget_t::animate_calc() || state_step_ || flash_step_;
}

/******************************************************************************
*/
void ipobject_t::paint_self(Gdiplus::Graphics *canvas)
{
	who::bitmap_ptr bitmap = class_->bitmap(state_.state());

	if (bitmap)
	{
		Gdiplus::ImageAttributes ia;

		Gdiplus::ColorMatrix matrix = state_matrix_;

		matrix.m[3][3] = alpha() * flash_alpha_;

		ia.SetColorMatrix(&matrix,
				Gdiplus::ColorMatrixFlagsDefault,
				Gdiplus::ColorAdjustTypeDefault);

		float x = x_;
		float y = y_;
		parent_->client_to_window(&x, &y);

		x += offs_x_;
		y += offs_y_;

		Gdiplus::RectF rect(
			x - class_->w() * class_->xc(),
			y - class_->h() * class_->yc(),
			class_->w(), class_->h() );

		canvas->DrawImage( bitmap.get(), rect, 0.0f, 0.0f,
			(float)bitmap->GetWidth(), (float)bitmap->GetHeight(),
			Gdiplus::UnitPixel, &ia, NULL, NULL );
		
		/* Состояние объектов */
		if (ipaddrs_.size() > 1)
		{
			Gdiplus::Rect rs = rectF_to_rect(rect);
			int count = ipaddrs_.size();
			int count_in_row = (count < 10 ? count : 10);
			const int hh = 5;
			
			rs.Height = hh;
			rs.Y = rs.Y - ((count - 1) / 10 + 1) * hh - 1;

			int w = rs.Width - 1; /* Ширина объекта */
			int ww = w / count_in_row;
			if (ww < 4) ww = 4;
			rs.X -= (ww * count_in_row - w) / 2;

			const Gdiplus::Color colors[4] =
			{
				Gdiplus::Color(0, 0, 0), Gdiplus::Color(0, 255, 0),
				Gdiplus::Color(255, 255, 0), Gdiplus::Color(255, 0, 0)
			};

			int index = 0;
			BOOST_FOREACH(const ipaddr_t &addr, ipaddrs_)
			{
				ipaddr_state_t state = server_.ipaddr_state(addr);

				BYTE _alpha = state.acknowledged() ? 255
					: (BYTE)(255 * alpha() * flash_alpha_);

				int x = rs.X + (index % 10) * ww;
				int y = rs.Y + (index / 10) * hh;

				Gdiplus::Pen pen( Gdiplus::Color(_alpha, 0, 0, 0), 1);
				canvas->DrawRectangle(&pen, x, y, ww, hh);

				Gdiplus::Color color( colors[state.state()] );
				color.SetValue( color.GetValue() & 0xFFFFFF
					| (Gdiplus::ARGB)(state.acknowledged() ? 255 : _alpha) << 24);

				Gdiplus::SolidBrush brush(color);
				canvas->FillRectangle(&brush, x + 1, y + 1, ww - 1, hh - 1);

				index++;
			}
		}

		/* Подписи */
		if (show_name_)
		{
			BYTE _alpha = (BYTE)(255 * alpha() * flash_alpha_);

			Gdiplus::Font font(L"Tahoma", 10, 0, Gdiplus::UnitPixel);
			Gdiplus::StringFormat sf;
			Gdiplus::SolidBrush text_brush( Gdiplus::Color(_alpha, 0, 0, 0) );

			sf.SetAlignment(Gdiplus::StringAlignmentCenter);
			sf.SetLineAlignment(Gdiplus::StringAlignmentNear);

			Gdiplus::PointF pt(rect.X + rect.Width * class_->xc(),
				rect.Y + rect.Height + 1);

			Gdiplus::RectF bounds;
			canvas->MeasureString(name_.c_str(), name_.size(),
				&font, pt, &sf, &bounds);

			Gdiplus::Rect rs = rectF_to_rect(bounds);

			Gdiplus::Pen pen( Gdiplus::Color(_alpha, 0, 0, 0), 1);
			canvas->DrawRectangle(&pen, rs.X - 1, rs.Y,
				rs.Width + 1, rs.Height - 1);

			Gdiplus::SolidBrush fill_brush(
				Gdiplus::Color( (BYTE)(_alpha * 0.7f), 255, 255, 0) );
			canvas->FillRectangle(&fill_brush, rs.X, rs.Y + 1,
				rs.Width, rs.Height - 2);

			canvas->DrawString(name_.c_str(), name_.size(),
				&font, pt, &sf, &text_brush);
		}
	}
}

/* Оповещение о необходимости проверить состояние объекта */
void ipobject_t::do_check_state(void)
{
	ipgroup_state_t new_state;

	BOOST_FOREACH(ipaddr_t &addr, ipaddrs_)
		new_state << server_.ipaddr_state(addr);

	if (new_state != state_) {
		lock();

		state_ = new_state;
		state_step_ = 2 * server_.def_anim_steps();
		new_state_matrix_ = state_matrices[ state_.state() ];

		/* Делаем объект мигающим */
		if (!state_.acknowledged()) {
			flash_step_ = 1;
			flash_pause_ = true;
		}

		unlock();

		animate();
	}

	ipwidget_t::do_check_state();
}

/* Квитирование */
void ipobject_t::acknowledge(void)
{
	lock();

	BOOST_FOREACH(const ipaddr_t &addr, ipaddrs_)
		server_.acknowledge(addr);
	
	unlock();

	animate();
}

/* Отмена квитирования */
void ipobject_t::unacknowledge(void)
{
	lock();
	
	BOOST_FOREACH(const ipaddr_t &addr, ipaddrs_)
		server_.unacknowledge(addr);

	unlock();

	animate();
}

/******************************************************************************
* Находится ли объект в координатах
*/
ipwidget_t* ipobject_t::hittest(float x, float y)
{
	ipwidget_t *widget = ipwidget_t::hittest(x, y);
	if (widget || alpha() == 0.0f)
		return widget;

	who::bitmap_ptr bitmap = class_->bitmap(state_.state());
	if (!bitmap)
		return NULL;

	/* Координаты объекта и координаты для проверки приводим
		к одной системе координат */
	float wx = x_;
	float wy = y_;
	parent_->client_to_window(&wx, &wy);
	client_to_window(&x, &y);

	wx += offs_x_;
	wy += offs_y_;

	int bmp_w = bitmap->GetWidth();
	int bmp_h = bitmap->GetHeight();
	int ix = (int)(x - wx + class_->xc() * bmp_w + 0.5f);
	int iy = (int)(y - wy + class_->yc() * bmp_h + 0.5f);

	if (ix>=0 && ix<bmp_w && iy>=0 && iy<bmp_h)
	{
		Gdiplus::Color color;
		bitmap->GetPixel(ix, iy, &color);
		
		if (color.GetA() >= 128)
			widget = this;
	}

	return widget;
}
