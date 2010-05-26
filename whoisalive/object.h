﻿#ifndef WHO_OBJECT_H
#define WHO_OBJECT_H

#include "ipgui.h"
#include "widget.h"
#include "ipaddr.h"
#include "obj_class.h"

#include "../common/my_xml.h"

#include <string>
#include <list>

namespace who {

namespace link_type { enum t {wire, optics, air}; }

class object : public widget
{
private:
	std::wstring name_;
	std::list<ipaddr_t> ipaddrs_;
	who::obj_class::ptr class_;
	ipgroup_state_t state_;
	Gdiplus::ColorMatrix state_matrix_;
	Gdiplus::ColorMatrix new_state_matrix_;
	int state_step_;
	int flash_step_;
	bool flash_pause_;
	float flash_alpha_;
	float flash_new_alpha_;
	ipaddr_t link_;
	bool show_name_;
	float offs_x_;
	float offs_y_;
	link_type::t link_type_;

public:
	object(server &server, const xml::wptree *pt = NULL);
	virtual ~object();

	virtual Gdiplus::RectF own_rect( void);
	Gdiplus::RectF rect_norm( void);
	virtual bool animate_calc( void);
	virtual void paint_self( Gdiplus::Graphics *canvas);
	virtual widget* hittest(float x, float y);

	inline const std::wstring& name(void)
		{ return name_; }

	virtual inline float alpha(void)
		{ return widget::alpha() * state_matrix_.m[3][3]; }

	inline float full_alpha(void)
		{ return alpha() * flash_alpha_; }

	inline ipstate::t state(void)
		{ return state_.state(); }

	virtual void do_check_state(void);

	inline bool acknowledged(void)
		{ return state_.acknowledged(); }
	void acknowledge(void);
	void unacknowledge(void);

	inline const std::list<ipaddr_t>& ipaddrs()
		{ return ipaddrs_; };
	inline ipaddr_t link(void)
		{ return link_; }

	inline bool show_name(void)
		{ return show_name_; }
	inline void set_show_name(bool show_name)
	{
		show_name_ = show_name;
		animate();
	}

	inline float offs_x()
		{ return offs_x_; }
	inline float offs_y()
		{ return offs_y_; }
		
	inline link_type::t link_type(void)
		{ return link_type_; }
};

}

#endif
