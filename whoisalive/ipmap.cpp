#include "ipmap.h"

#include <map>
#include <string>
#include <sstream>
#include <fstream>
#include <utility> /* std::pair */

#include <boost/foreach.hpp>

#include "window.h"
#include "server.h"
#include "obj_class.h"

ipmap_t::ipmap_t(who::server &server, const xml::wptree *pt)
	: ipwidget_t(server, pt)
	, window_(NULL)
	, name_()
	, first_activation_(true)
	, min_scale_(0.0f)
	, max_scale_(0.0f)
	, show_names_(true)
{
	try
	{
		if (pt)
		{
			name_ = pt->get<wstring>(L"<xmlattr>.name");

			min_scale_ = pt->get<float>(L"<xmlattr>.min_scale", min_scale_);
			max_scale_ = pt->get<float>(L"<xmlattr>.max_scale", max_scale_);
		
			show_names_ = pt->get<bool>(L"<xmlattr>.show_names", true);

			wstring type = pt->get<wstring>(L"<xmlattr>.type", L"map");
			if (type == L"scheme")
				type_ = map_type::scheme;
			else if (type == L"map")
				type_ = map_type::map;
			else
				throw my::exception(L"Неизвестный тип схемы")
					<< my::param(L"type", type);

			pair<xml::wptree::const_assoc_iterator,
				xml::wptree::const_assoc_iterator> p;

			p = pt->equal_range(L"image");
			while (p.first != p.second)
			{
				ipimage_t *image = new ipimage_t(server_, &(*p.first).second);
				image->set_parent(this);
				childs_.push_back(image);
				p.first++;
			}

			p = pt->equal_range(L"object");
			while (p.first != p.second)
			{
				who::object *object = new who::object(server_, &(*p.first).second);
				object->set_parent(this);
				object->set_show_name(show_names_);
				childs_.push_back(object);
				childs_.sort();
				p.first++;
			}
		}
	}
	catch (my::exception &e)
	{
		throw my::exception(L"Ошибка создания схемы")
			<< my::param( L"xml", xmlattr_to_str(*pt) )
			<< e;
	}
	catch (exception &e)
	{
		throw my::exception(L"Ошибка создания схемы")
			<< my::param( L"xml", xmlattr_to_str(*pt) )
			<< e;
	}
}

/******************************************************************************
*/
void ipmap_t::animate(void)
{
	if (window_) window_->animate();
}

/******************************************************************************
* Изменение масштаба карты
*/
void ipmap_t::scale__(float new_scale, float fix_x, float fix_y, int steps)
{
	lock();

		if (steps <= 0) steps = 1;
		else steps *= server_.def_anim_steps();

		if (max_scale_ != 0.0f) new_scale = min(new_scale, max_scale_);
		new_scale = max(new_scale, min_scale_);

		/* Выравниваем относительно главных величин: 1,2,4,8,16... */
		{
			int z_scale = 1 << ( z(new_scale) - 1 );
			float k = new_scale / (float)z_scale;
			if (k >= 1.0f && k < 1.1f)
				new_scale = (float)z_scale;
			else if (k > 1.9f && k <= 2.0f)
				new_scale = (float)(z_scale << 1);
		}

		new_scale_ = new_scale;
		scale_step_ = steps;

		/* Фиксируем карту в координатах fix_x, fix_y */
		client_to_parent(&fix_x, &fix_y);

		float ds = new_scale_ / scale_;

		new_x_ = fix_x - (fix_x - x_) * ds;
		new_y_ = fix_y - (fix_y - y_) * ds;
		xy_step_ = steps;

	unlock();

	animate();
}

/******************************************************************************
* Координаты и размеры карты
*/
Gdiplus::RectF ipmap_t::own_rect(void)
{
	/* У карты нет своего rect'а */
	return Gdiplus::RectF(0.0f, 0.0f, 0.0f, 0.0f);
}

/******************************************************************************
* Координаты и размеры карты
*/
Gdiplus::RectF ipmap_t::client_rect(void)
{
	Gdiplus::RectF rect = own_rect();
	bool rect_is_empty = true;

	BOOST_FOREACH(ipwidget_t &child, childs_) {
		Gdiplus::RectF child_rect = child.client_rect();
		child.client_to_parent(&child_rect);
		if (!rect_is_empty) rect = maxrect(rect, child_rect);
		else {
			rect = child_rect;
			rect_is_empty = false;
		}
	}

	float dx = rect.Width * 0.1f;
	float dy = rect.Height * 0.1f;
	rect.X -= dx;
	rect.Y -= dy;
	rect.Width += 2.0f * dx;
	rect.Height += 2.0f * dy;

	return rect;
}

/******************************************************************************
* Карту в центр экрана
*/
void ipmap_t::align(float scr_w, float scr_h, int steps)
{
	if (scr_w && scr_h) {

		lock();
			if (steps <= 0) steps = 1;
			else steps *= server_.def_anim_steps();

			Gdiplus::RectF r = objects_rect();

			if (r.Width > 0.0f) {
				float new_scale = 0.9f * min( scr_w/r.Width, scr_h/r.Height );
				int _z = z(new_scale) - 1;
				if (_z) new_scale = (float)(1 << _z);
				new_scale_ = new_scale;

				scale_step_ = steps;

				//min_scale_ = 0.01f; //new_scale_ * 0.1f;
				//max_scale_ = 100.0f; //min_scale_ * 20.0f;

				new_x_ = (scr_w - r.Width * new_scale_) / 2.0f - r.X * new_scale_;
				new_y_ = (scr_h - r.Height * new_scale_) / 2.0f - r.Y * new_scale_;
				xy_step_ = steps;

				if (first_activation_) {
					first_activation_ = false;
					scale_ = new_scale_ * 1.2f;
					x_ = new_x_;
					y_ = new_y_;
				}
			}

		unlock();

		animate();
	}
}

/******************************************************************************
*/
bool ipmap_t::animate_calc(void)
{
	lock();

	bool anim = ipwidget_t::animate_calc();

	x_ = floor( x_+0.5f );
	y_ = floor( y_+0.5f );

	unlock();
	return anim;

/*-
// Попытка сделать анимироанный откат при уходе за пределы min_scale_,
	max_scale_ - сам scale работает замечательно, но как вернуть изменение
	координат?

	bool anim = false;

	if (xy_step_) {
		x_ += (new_x_ - x_) / xy_step_;
		y_ += (new_y_ - y_) / xy_step_;
		xy_step_--;
	}

	if (scale_step_) {
		scale_ += (new_scale_ - scale_) / scale_step_;
		scale_step_--;

		if( max_scale_ != 0.0f && new_scale_ > max_scale_) {
			new_scale_ = max_scale_;
			scale_step_ = max(scale_step_, 1);
		}
		if( new_scale_ < min_scale_) {
			new_scale_ = min_scale_;
			scale_step_ = max(scale_step_, 1);
		}
	}

	BOOST_FOREACH(ipwidget_t *widget, childs_) {
		if (widget->animate_calc()) anim = true;
	}

	return anim || xy_step_ || scale_step_;
-*/
}

/******************************************************************************
*/
ipwidget_t* ipmap_t::hittest(float x, float y)
{
	ipwidget_t *widget = ipwidget_t::hittest(x, y);

	if (!widget) widget = this;

	return widget;
}

void ipmap_t::paint_self(Gdiplus::Graphics *canvas)
{
	/* Прорисовка карты */
	//if () {
		
		/* Суть - сначала закидываем тайлы в буфер в масштабе 1:1,
			а затем буфер проецируем в окно уже с нужным масштабом.
			Если делать это сразу, то из-за нецелочисленных размеров
			между тайлами образуются светлые линии */

		/* Размеры окна */
		Gdiplus::Rect bounds;
		canvas->GetVisibleClipBounds(&bounds);

		/* Расчитываем zoom-Фактор для текущего масштаба:
			1 (для 1x-2x), 2 (2x-4x), 3 (4x-8x), 4 (8x-16x) ... */
		int z = ipmap_t::z(scale_);

		/* Размер карты в тайлах. И по совместительству - масштаб
			для рассчитанного zoom-фактора. Из-за округлений в расчётах z
			он будет меньше либо равен текущему масштабу */
		int sz = 1 << (z - 1);

		/* Коэффициент увеличения тайлов */
		float k = scale_ / (float)sz;

		/* Реальные размеры (и ширина, и высота) тайла при выводе в окно */
		float w = 256.0f * k;

		/* Первый тайл, попавший в зону окна.
			x_, y_ - смещение карты относительно верхнего левого угла окна,
			если карта уходит за край окна, эти значения отрицательные */
		int begin_x = - (int)ceil(x_ / w);
		int begin_y = - (int)ceil(y_ / w);
		
		// /* По x - карты бесконечна, по y - ограничена */
		if (begin_x < 0) begin_x = 0;
		if (begin_y < 0) begin_y = 0;

		/* Кол-во тайлов, попавших в окно */
		int count_x;
		int count_y;
		{
			/* Размеры окна за вычетом первого тайла */
			float W = bounds.Width - ( x_ + w * (begin_x + 1) );
			float H = bounds.Height - ( y_ + w * (begin_y + 1) );

			/* Округляем в большую сторону, т.е. включаем в окно тайлы,
				частично попавшие в окно. Не забываем и про первый тайл,
				который исключили */
			count_x = (int)ceil(W / w) + 1;
			count_y = (int)ceil(H / w) + 1;

			/* По x - карта бесконечна, по y - ограничена */
			if (begin_x + count_x > sz) count_x = sz - begin_x;
			if (begin_y + count_y > sz) count_y = sz - begin_y;
		}

		/* Если буфер маловат, увеличиваем */
		int bmp_w = count_x * 256;
		int bmp_h = count_y * 256;
		if (!bitmap_.get() || (int)bitmap_->GetWidth() < bmp_w
			|| (int)bitmap_->GetHeight() < bmp_h)
		{
			/* Параметры буфера такие же как у экрана ((HWND)0) */
			Gdiplus::Graphics screen((HWND)0, FALSE);
			bitmap_.reset( new Gdiplus::Bitmap(bmp_w, bmp_h, &screen) );
		}

		Gdiplus::Graphics bmp_canvas( bitmap_.get() );

		/* Очищаем буфер */
		{
			Gdiplus::SolidBrush brush( Gdiplus::Color(0, 0, 0, 0) );
			bmp_canvas.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
			bmp_canvas.FillRectangle(&brush, 0, 0, bmp_w, bmp_h);
			bmp_canvas.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
		}

		for (int ix = 0; ix < count_x; ix++)
		for (int iy = 0; iy < count_y; iy++) {
			int x = ix + begin_x;
			int y = iy + begin_y;
			server_.paint_tile(&bmp_canvas, ix * 256, iy * 256, z, x, y);
		}

		Gdiplus::RectF rect( x_ + begin_x * w, y_ + begin_y * w,
			bmp_w * k, bmp_h * k );
		canvas->DrawImage( bitmap_.get(), rect,
			0.0f, 0.0f, (float)bmp_w, (float)bmp_h,
			Gdiplus::UnitPixel, NULL, NULL, NULL );
	//}
}
