#ifndef WHO_SCHEME_H
#define WHO_SCHEME_H

#include "ipgui.h"
#include "widget.h"
#include "ipimage.h"
#include "object.h"

#include "../common/my_xml.h"
#include "../common/my_thread.h"

#include <map>
#include <string>
#include <sstream>
#include <fstream>
#include <memory>
using namespace std;

#include <boost/ptr_container/ptr_list.hpp>

namespace who {

class window;

class scheme : public widget
{
private:
	window *window_;
	wstring name_;
	bool first_activation_;
	mutex scheme_mutex_;
	float min_scale_;
	float max_scale_;
	std::auto_ptr<Gdiplus::Bitmap> bitmap_;
	bool show_names_;
	bool show_map_;

public:
	scheme(server &server, const xml::wptree *pt = NULL);
	virtual ~scheme() {}

	virtual scoped_lock create_lock()
		{ return scoped_lock(scheme_mutex_); }

	inline const wchar_t* get_name(void)
		{ return name_.c_str(); }

	inline bool first_activation(void)
		{ return first_activation_; }
	inline void set_first_activation(bool f)
		{ first_activation_ = f; }

	virtual Gdiplus::RectF own_rect(void);
	virtual Gdiplus::RectF client_rect(void);
	virtual void paint_self(Gdiplus::Graphics *canvas);
	virtual bool animate_calc(void);
	virtual widget* hittest(float x, float y);

	virtual void set_parent(widget *parent) {} /* Блокируем изменение parent'а */

	virtual window* get_window(void)
		{ return window_; }
	inline void set_window(who::window *window)
		{ window_ = window; }
	virtual scheme* get_scheme(void)
		{ return this; }

	virtual void animate(void);

	inline void zoom(float ds, float fix_x, float fix_y, int steps = 2)
		{ scale__(new_scale_ * ds, fix_x, fix_y, steps); }

	void scale__(float new_scale, float fix_x, float fix_y, int steps = 2);

	void align(float scr_w, float scr_h, int steps = 2);

	static inline int z(float scale)
	{
		int _z = 0;
		int zi = (int)scale;
		while (zi)
		{
			zi = zi >> 1;
			_z++;
		}
		return _z ? _z : 1;
	}

	inline bool show_names(void)
		{ return show_names_; }
};

}

#endif
