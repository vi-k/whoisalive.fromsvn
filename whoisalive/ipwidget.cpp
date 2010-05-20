#include "ipwidget.h"
#include "ipmap.h"
#include "ipwindow.h"
#include "server.h"

#include <boost/foreach.hpp>

ipwidget_t::ipwidget_t(who::server &server, const xml::wptree *pt)
	: server_(server)
	, parent_(NULL)
	, x_(0.0f)
	, y_(0.0f)
	, new_x_(0.0f)
	, new_y_(0.0f)
	, xy_step_(0)
	, saved_x_(0.0f)
	, saved_y_(0.0f)
	, scale_(1.0f)
	, new_scale_(1.0f)
	, scale_step_(0)
	, lim_scale_min_(0.0f)
	, lim_scale_max_(0.0f)
	, alpha_(1.0f)
	, new_alpha_(1.0f)
	, alpha_step_(0)
	, selected_(false)
	, tmp_selected_(false)
{
	if (pt)
	{
		x_ = pt->get<float>(L"<xmlattr>.x", x_);
		y_ = pt->get<float>(L"<xmlattr>.y", y_);
		scale_ = pt->get<float>(L"<xmlattr>.scale", scale_);
		lim_scale_min_ = pt->get<float>(L"<xmlattr>.scale_min", lim_scale_min_);
		lim_scale_max_ = pt->get<float>(L"<xmlattr>.scale_max", lim_scale_max_);
		alpha_ = pt->get<float>(L"<xmlattr>.alpha", alpha_);
	}
}

bool ipwidget_t::animate_calc(void)
{
	bool anim = false;

	/* Сначала себя */
	if (xy_step_) {
		x_ += (new_x_ - x_) / xy_step_;
		y_ += (new_y_ - y_) / xy_step_;
		xy_step_--;
	}

	if (scale_step_) {
		scale_ += (new_scale_ - scale_) / scale_step_;
		scale_step_--;
	}

	/* Выход за рамки масштаба */
	float map_scale = get_map()->scale();
	if (map_scale < lim_scale_min_ || lim_scale_max_ != 0.0f
			&& get_map()->scale() >= lim_scale_max_) {
		if (new_alpha_ > 0.0f) {
			new_alpha_ = 0.0f;
			alpha_step_ = server_.def_anim_steps();
		}
	}
	else if (new_alpha_ == 0.0f) {
		new_alpha_ = 1.0f;
		alpha_step_ = server_.def_anim_steps();
	}

	if (alpha_step_) {
		alpha_ += (new_alpha_ - alpha_) / alpha_step_;
		alpha_step_--;
	}

	/* ... затем и "детей" */
	BOOST_FOREACH(ipwidget_t &widget, childs_) {
		if (widget.animate_calc()) anim = true;
	}

	/* Ещё надо будет анимировать? */
	return anim || xy_step_ || scale_step_ || alpha_step_;
}

void ipwidget_t::set_scale(float new_scale, int steps)
{
	lock();

	if (steps <= 0) steps = 1;
	else steps *= server_.def_anim_steps();

	new_scale_ = new_scale;
	scale_step_ = steps;

	unlock();

	animate();
}

void ipwidget_t::set_pos(float new_x, float new_y, int steps)
{
	lock();

	if (steps <= 0) steps = 1;
	else steps *= server_.def_anim_steps();

	new_x_ = new_x;
	new_y_ = new_y;
	xy_step_ = steps;

	unlock();

	animate();
}

ipwidget_t* ipwidget_t::hittest(float x, float y)
{
	BOOST_REVERSE_FOREACH(ipwidget_t &child, childs_) {
		float cx = x;
		float cy = y;
		child.parent_to_client(&cx, &cy);

		ipwidget_t *widget = child.hittest(cx, cy);

		if (widget) return widget;
	}

	return NULL;
}

/* Выделение объекта. Для этого снимаем выделение со всех родителей и детей,
	т.к. у нас родитель не может быть выделен одновременно с детьми */
void ipwidget_t::select(void)
{
	ipwidget_t *parent = parent_;
	while (parent) {
		parent->unselect();
		parent = parent->parent_;
	}

	unselect_all();

	tmp_selected_ = false;
	selected_ = true;

	animate();
}

void ipwidget_t::unselect(void)
{
	if (tmp_selected_) {
		tmp_selected_ = false;
		animate();
	}

	if (selected_) {
		selected_ = false;
		animate();
	}
}

void ipwidget_t::unselect_all(void)
{
	unselect();

	BOOST_FOREACH(ipwidget_t &widget, childs_) {
		widget.unselect_all();
	}
}

bool ipwidget_t::in_rect(Gdiplus::RectF in_r)
{
	Gdiplus::RectF obj_r = rect_in_parent();

//	/* Объект полностью входит в in_r */
//	return r.X >= in_r.X && r.X + r.Width <= in_r.X + in_r.Width
//			&& r.Y >= in_r.Y && r.Y + r.Height <= in_r.Y + in_r.Height;

	float l = obj_r.X;
	float t = obj_r.Y;
	float r = obj_r.X + obj_r.Width;
	float b = obj_r.Y + obj_r.Height;

	float L = in_r.X;
	float T = in_r.Y;
	float R = in_r.X + in_r.Width;
	float B = in_r.Y + in_r.Height;

	/* Объект хотя бы одной гранью входит в in_r, либо in_r полностью входит
		в объект */
	//return
		//(l>=L && l<R || r>L && r<=R || L>=l && R<=r)
			//&&
		//(t>=T && t<B || b>T && b<=B || T>=t && B<=b);

	/* Intersect rect существует (по результату аналогично предыдущему ) */
	return max(l,L) < min(r,R) && max(t,T) < min(b,B);
}

/******************************************************************************
*/
void ipwidget_t::paint(Gdiplus::Graphics *canvas)
{
	if (alpha() == 0.0f) return;

	paint_self(canvas);

	/* Рисуем линии */
	BOOST_FOREACH(ipwidget_t &widget, childs_) {
		ipobject_t *object1 = dynamic_cast<ipobject_t*>(&widget);

		if (object1 && object1->alpha() != 0.0f) {
			BOOST_FOREACH(const ipaddr_t &addr, object1->ipaddrs()) {
				BOOST_FOREACH(ipwidget_t &widget, childs_) {
					ipobject_t *object2 = dynamic_cast<ipobject_t*>(&widget);
					if (object2 && addr == object2->link()) {
						float x1 = object1->x_;
						float y1 = object1->y_;
						float x2 = object2->x_;
						float y2 = object2->y_;
						client_to_window(&x1, &y1);
						client_to_window(&x2, &y2);
						
						x1 += object1->offs_x();
						y1 += object1->offs_y();
						x2 += object2->offs_x();
						y2 += object2->offs_y();

						BYTE a = (int)( 255.0f * object2->full_alpha() * 0.7f);
						
						if (a) {
							Gdiplus::Color color(a, 0, 0, 0);
							switch ( object2->state() ) {
								case ipstate::unknown: color = Gdiplus::Color(a, 0, 0, 255); break;
								case ipstate::ok: color = Gdiplus::Color(a, 0, 192, 0); break;
								case ipstate::warn: color = Gdiplus::Color(a, 192, 192, 0); break;
								case ipstate::fail: color = Gdiplus::Color(a, 255, 0, 0); break;
							}

							float thickness = 0.0f;
							
							switch ( object2->link_type() ) {
								case iplink_type::wire:
									thickness = 6.0f;
									break;
								case iplink_type::optics:
									thickness = 12.0f;
									break;
								case iplink_type::air:
									thickness = 2.0f;
									break;
							}

							Gdiplus::Pen pen(color, thickness);
							canvas->DrawLine(&pen, x1, y1, x2, y2);
						}
					}
				}
			}

		}
	}

	BOOST_FOREACH(ipwidget_t &widget, childs_) {
		widget.paint(canvas);
	}

	if (selected()) {
		Gdiplus::Rect rs = rectF_to_rect( rect_in_window() );

		Gdiplus::Color color_pen;
		color_pen.SetFromCOLORREF( GetSysColor(COLOR_HIGHLIGHT) );
		Gdiplus::Pen pen(color_pen, 1);

		Gdiplus::Color color_brush(color_pen.GetValue() & 0xFFFFFF |
				(tmp_selected_ ? 0x80000000 : 0x40000000));
		Gdiplus::SolidBrush brush(color_brush);

		/* FillRectangle рисует ровно нужный нам прямоугольник */
		canvas->FillRectangle(&brush, rs.X + 1, rs.Y + 1, rs.Width - 2, rs.Height - 2);
		/* DrawRectangle хитрый - рисует прямоугольник на 1 пиксель и шире, и ниже */
		canvas->DrawRectangle(&pen, rs.X, rs.Y, rs.Width - 1, rs.Height - 1);

		brush.SetColor(color_pen);
		canvas->FillRectangle(&brush, rs.X - 1, rs.Y - 1, 4, 4);
		canvas->FillRectangle(&brush, rs.X - 1, rs.Y + rs.Height - 3, 4, 4);
		canvas->FillRectangle(&brush, rs.X + rs.Width - 3, rs.Y - 1, 4, 4);
		canvas->FillRectangle(&brush, rs.X + rs.Width - 3, rs.Y + rs.Height - 3, 4, 4);

		float x = x_;
		float y = y_;
		if (parent_) parent_->client_to_window(&x, &y);
		canvas->DrawLine( &pen, x, (float)rs.Y, x, (float)(rs.Y + rs.Height - 1) );
		canvas->DrawLine( &pen, (float)rs.X, y, (float)(rs.X + rs.Width - 1), y );
	}

	ipwindow_t *win = window();

	/* Выделение */
	if (win->select_parent() == this) {

		/* Рамка вокруг select_parent'а */
		Gdiplus::Rect rect = rectF_to_rect( rect_in_window() );
		Gdiplus::Color color(0, 0, 0);
		Gdiplus::Pen pen(color, 1);

		canvas->DrawRectangle(&pen, rect.X, rect.Y,
				rect.Width - 1, rect.Height - 1);

		/* Выделительная рамка */
		color.SetFromCOLORREF( GetSysColor(COLOR_HIGHLIGHT) );
		pen.SetColor(color);

		color.SetValue(color.GetValue() & 0xFFFFFF | 0x40000000);
		Gdiplus::SolidBrush brush(color);

		Gdiplus::RectF select_rect = win->select_rect();
		client_to_window(&select_rect);
		rect = rectF_to_rect(select_rect);

		canvas->FillRectangle(&brush, rect);
		canvas->DrawRectangle(&pen, rect.X, rect.Y,
				rect.Width - 1, rect.Height - 1);
	}
}

/******************************************************************************
* Координаты и размеры widget'а
*/
Gdiplus::RectF ipwidget_t::client_rect(void)
{
	Gdiplus::RectF rect = own_rect();

	BOOST_FOREACH(ipwidget_t &child, childs_) {
		Gdiplus::RectF child_rect = child.client_rect();
		child.client_to_parent(&child_rect);
		rect = maxrect(rect, child_rect);
	}

	return rect;
}

/******************************************************************************
* Координаты и размеры widget'а
*/
Gdiplus::RectF ipwidget_t::objects_rect(void)
{
	Gdiplus::RectF rect(0.0f, 0.0f, -10.0f, -10.0f);

	ipobject_t *object = dynamic_cast<ipobject_t*>(this);
	if (object) rect = own_rect();

	BOOST_FOREACH(ipwidget_t &child, childs_) {
		Gdiplus::RectF child_rect = child.objects_rect();
		child.client_to_parent(&child_rect);
		if (rect.Width < 0.0f) rect = child_rect;
		else if (child_rect.Width >= 0.0f) rect = maxrect(rect, child_rect);
	}

	return rect;
}

void ipwidget_t::set_parent(ipwidget_t *parent)
{
	parent_ = parent;
	animate();
}

void ipwidget_t::do_check_state(void)
{
	BOOST_FOREACH(ipwidget_t &widget, childs_)
		widget.do_check_state();
}
