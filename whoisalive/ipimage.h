#ifndef IPIMAGE_H
#define IPIMAGE_H

#include "ipgui.h"
#include "widget.h"

#include "../common/my_xml.h"

#include <string>
#include <memory>

class ipimage_t : public who::widget {
	private:
		std::wstring filename_;
		std::auto_ptr<Gdiplus::Image> image_;
		std::auto_ptr<Gdiplus::Bitmap> bitmap_;
		float w_;
		float h_;

	public:
		ipimage_t(who::server &server,
			const xml::wptree *pt = NULL);

		virtual Gdiplus::RectF own_rect( void);
		virtual void paint_self( Gdiplus::Graphics *canvas);
		virtual who::widget* hittest(float x, float y);
		virtual bool animate_calc(void);
};

#endif
