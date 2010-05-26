#ifndef IPWIDGET_H
#define IPWIDGET_H

#include "ipgui.h"

#include "../common/my_xml.h"
#include "../common/my_thread.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/format.hpp>
#include <boost/format/format_fwd.hpp>
#include <boost/ptr_container/ptr_list.hpp>

namespace who
{

class server;
class window;
class scheme;

class widget
{
protected:
	server &server_;
	widget *parent_;
	float x_; /* Куда именно указывают координаты (левый верхний угол */
	float y_; /* объекта или его центр) - зависит от самого объекта */
	float new_x_; /* Новые координаты для перемещений */
	float new_y_;
	int xy_step_;
	float saved_x_; /* "Старые" координаты для отмены перемещений */
	float saved_y_;
	float scale_;
	float new_scale_;
	int scale_step_;
	float lim_scale_min_;
	float lim_scale_max_;
	float alpha_;
	float new_alpha_;
	int alpha_step_;
	bool selected_;
	bool tmp_selected_;
	boost::ptr_list<widget> childs_;

public:
	widget(server &server, const xml::wptree *pt = NULL);
	virtual ~widget() {}

	/****************************************
	* Виртуальные функции
	*/

	/* Вернуть rect объекта. Ф-я должна быть перегружена */
	virtual Gdiplus::RectF own_rect(void) = 0;
	/* Rect объекта вместе с детьми */
	virtual Gdiplus::RectF client_rect(void);
	/* Только объекты и никто другой */
	Gdiplus::RectF objects_rect(void);

	/* Анимация объекта (без прорисовки). При перегрузке
		класс-наследник может/должен вызвать ф-ю наследуемого класса */
	virtual bool animate_calc(void);

	/* Нарисовать объект. Ф-я должна быть перегружена */
	virtual void paint_self(Gdiplus::Graphics *canvas) = 0;

	/* Запуск анимации */
	virtual void animate(void)
		{ if (parent_) parent_->animate(); }

	/* Кто в координатах? */
	virtual widget* hittest(float x, float y);

	/* Входит ли объект в rect */
	virtual bool in_rect(Gdiplus::RectF rect);

	/* Блокировка */
	virtual scoped_lock create_lock()
		{ return parent_->create_lock(); }
		//{ return parent_ ? parent_->create_lock() : scoped_lock(); }

	virtual window* get_window(void)
		{ return parent_ ? parent_->get_window() : NULL; }
	virtual who::scheme* get_scheme(void)
		{ return parent_ ? parent_->get_scheme() : NULL; }

	virtual float alpha(void)
		{ return (parent_ ? parent_->alpha() : 1.0f) * alpha_; }
		
	inline widget* parent(void)
		{ return parent_; }
	virtual void set_parent(widget *parent);

	/* Rect объекта вместе с детьми в родителе */
	Gdiplus::RectF rect_in_parent(void)
	{
		Gdiplus::RectF rect = client_rect();
		client_to_parent(&rect);
		return rect;
	}
	Gdiplus::RectF rect_in_window(void)
	{
		Gdiplus::RectF rect = client_rect();
		client_to_window(&rect);
		return rect;
	}

	/* Прорисовка объекта вместе с детьми */
	void paint(Gdiplus::Graphics *canvas);

	/* Масштаб */
	float scale(void) { return scale_; }
	void set_scale( float new_scale, int steps = 1);

	/* Функции перемещения объекта */
	float x(void)
		{ return x_; }
	float y(void)
		{ return y_; }
	void set_pos(float new_x, float new_y, int steps = 1);
	void move(float dx, float dy, int steps = 1)
		{ set_pos(x_ + dx, y_ + dy, steps); }
	void save_pos(void)
	{
		saved_x_ = x_;
		saved_y_ = y_;
	}
	void restore_pos(int steps = 1)
		{ set_pos(saved_x_, saved_y_, steps); }
	void move_from_saved(float dx, float dy, int steps = 1)
		{ set_pos(saved_x_ + dx, saved_y_ + dy, steps); }

	/* Функции выделения объекта */
	inline bool selected(void)
		{ return selected_ || tmp_selected_; }

	void select(void);
	void unselect(void);
	void unselect_all(void);

	bool tmp_selected(void)
		{ return tmp_selected_; }
	void tmp_select(void)
		{ tmp_selected_ = true; }
	void tmp_unselect(void)
		{ tmp_selected_ = false; }

	boost::ptr_list<widget>& childs() { return childs_; }

	/* Преобразование координат */
	void client_to_parent(float *px, float *py)
	{
		*px = x_ + *px * scale_;
		*py = y_ + *py * scale_;
	}
	void client_to_parent(Gdiplus::RectF *prect)
	{
		client_to_parent(&prect->X, &prect->Y);
		prect->Width *= scale_;
		prect->Height *= scale_;
	}

	void parent_to_client(float *px, float *py)
	{
		*px = (*px - x_) / scale_;
		*py = (*py - y_) / scale_;
	}
	void parent_to_client(Gdiplus::RectF *prect)
	{
		parent_to_client(&prect->X, &prect->Y);
		prect->Width /= scale_;
		prect->Height /= scale_;
	}

	void client_to_window(float *px, float *py)
	{
		widget *parent = this;
		while (parent)
		{
			parent->client_to_parent(px, py);
			parent = parent->parent_;
		}
	}
	void client_to_window(Gdiplus::RectF *prect)
	{
		widget *parent = this;
		while (parent)
		{
			parent->client_to_parent(prect);
			parent = parent->parent_;
		}
	}

	void window_to_client(float *px, float *py)
	{
		if (parent_)
			parent_->window_to_client(px, py);
		parent_to_client(px, py);
	}
	void window_to_client(Gdiplus::RectF *prect)
	{
		if (parent_)
			parent_->window_to_client(prect);
		parent_to_client(prect);
	}

	virtual void do_check_state(void);

	inline bool operator <(const widget &rhs) const
		{ return y_ < rhs.y_ || y_ == rhs.y_ && x_ < rhs.x_; }

	inline float lim_scale_min(void)
		{ return lim_scale_min_; };
	inline float lim_scale_max(void)
		{ return lim_scale_max_; };
};

}

#endif
