#ifndef WHO_WINDOW_H
#define WHO_WINDOW_H

#include "ipgui.h"
#include "ipaddr.h"
#include "widget.h"
#include "scheme.h"

#include "../common/my_thread.h"
#include "../common/my_http.h"
#include "../common/my_inet.h"

#include <memory>

#include <boost/function.hpp>
#include <boost/ptr_container/ptr_list.hpp>

#define MY_WM_CHECK_STATE WM_USER
#define MY_WM_UPDATE WM_USER+1

namespace who {

namespace mousemode
{ enum t { none, capture, move, select, edit }; }

namespace selectmode
{ enum t { normal, add, remove }; }

namespace mousekeys
{ enum : int { ctrl = 1, shift = 2, lbutton = 4, rbutton = 8, mbutton = 16 }; }

class window
{
private:
	bool terminate_;
	server &server_;
	boost::thread anim_thread_;
	condition_variable anim_cond_;
	HWND hwnd_;
	bool focused_;
	int w_;
	int h_;
	std::auto_ptr<Gdiplus::Bitmap> bitmap_;
	std::auto_ptr<Gdiplus::Graphics> canvas_;
	Gdiplus::Color bg_color_;
	scheme *active_scheme_;
	boost::ptr_list<scheme> schemes_;
	mousemode::t mouse_mode_;
	int mouse_start_x_;
	int mouse_start_y_;
	int mouse_end_x_;
	int mouse_end_y_;
	widget *select_parent_;
	Gdiplus::RectF select_rect_;
	mutex canvas_mutex_;

	void paint_(void);

	static LRESULT CALLBACK static_wndproc_(HWND hwnd, UINT uMsg,
			WPARAM wParam, LPARAM lParam);
	inline LRESULT wndproc_(HWND hwnd, UINT uMsg,
			WPARAM wParam, LPARAM lParam);

	void set_active_scheme_(who::scheme *scheme);
		
	inline void on_mouse_event_(
		const boost::function<void (window*, int keys, int x, int y)> &f,
		WPARAM wparam, LPARAM lparam);
		
	static int window::wparam_to_keys_(WPARAM wparam);

	void anim_thread_proc(void);

public:
	/* События */
	boost::function<void (window*, int delta, int keys, int x, int y)> on_mouse_wheel;
	boost::function<void (window*, int keys, int x, int y)> on_mouse_move;
	boost::function<void (window*, int keys, int x, int y)> on_lbutton_down;
	boost::function<void (window*, int keys, int x, int y)> on_rbutton_down;
	boost::function<void (window*, int keys, int x, int y)> on_mbutton_down;
	boost::function<void (window*, int keys, int x, int y)> on_lbutton_up;
	boost::function<void (window*, int keys, int x, int y)> on_rbutton_up;
	boost::function<void (window*, int keys, int x, int y)> on_mbutton_up;
	boost::function<void (window*, int keys, int x, int y)> on_lbutton_dblclk;
	boost::function<void (window*, int keys, int x, int y)> on_rbutton_dblclk;
	boost::function<void (window*, int keys, int x, int y)> on_mbutton_dblclk;
	boost::function<void (window*, int key)> on_keydown;
	boost::function<void (window*, int key)> on_keyup;

	window(server &server, HWND parent);
	~window();

	inline HWND hwnd(void)
		{ return hwnd_; }
	inline widget* select_parent(void)
		{ return select_parent_; }
	inline Gdiplus::RectF select_rect(void)
		{ return select_rect_; }
	inline int w(void)
		{ return w_; }
	inline int h(void)
		{ return h_; }

	void animate(void);

	void add_schemes(xml::wptree &config);

	void set_link(HWND parent);
	void delete_link(void);
	void on_destroy(void);

	void set_active_scheme(int index);
	inline scheme* active_scheme(void)
		{ return active_scheme_; }
	scheme* get_scheme(int index);
	inline int get_schemes_count(void)
		{ return schemes_.size(); }

	void set_size(int w, int h);

	void mouse_start(mousemode::t mm, int x, int y,
		selectmode::t sm = selectmode::normal);
	void mouse_move_to(int x, int y);
	void mouse_end(int x, int y);
	void mouse_cancel(void);

	virtual void do_check_state(void);

	void zoom(float ds);

	void set_scale(float scale)
	{
		if (active_scheme_)
			active_scheme_->set_scale(scale);
	}

	void set_pos(float x, float y)
	{
		if (active_scheme_)
			active_scheme_->set_pos(x, y);
	}

	inline mousemode::t mouse_mode(void)
		{ return mouse_mode_; }

	void align(void)
	{
		if (active_scheme_)
			active_scheme_->align( (float)w_, (float)h_ );
	}

	widget* hittest(int x, int y);

	void clear(void);
};

}

#endif
