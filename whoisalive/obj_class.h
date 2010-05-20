#ifndef IPCLASS_H
#define IPCLASS_H

#include "ipgui.h"
#include "ipaddr.h"

#include "../common/my_xml.h"
#include "../common/my_ptr.h"

#include <string>
#include <memory>

namespace who
{

class server;

typedef shared_ptr<Gdiplus::Bitmap> bitmap_ptr;

class obj_class
{
public:
	typedef shared_ptr<who::obj_class> ptr;

private:
	who::server &server_;
	std::wstring name_;
	xml::wptree config_;
	bitmap_ptr bitmap_[4];
	float w_;
	float h_;
	float xc_;
	float yc_;

public:
	obj_class(who::server &server, const xml::wptree &config);
	~obj_class() {}

	inline const std::wstring& name(void)
		{ return name_; }

	inline float w(void)
		{ return w_; }
	inline float h(void)
		{ return h_; }

	inline float xc(void)
		{ return xc_; }
	inline float yc(void)
		{ return yc_; }

	inline bitmap_ptr bitmap(ipstate::t state)
		{ return bitmap_[state]; }

	//void paint( Gdiplus::Graphics *canvas, float offs_x, float offs_y,
	//		float scale, float x, float y, float w, float h);
};

}

#endif
