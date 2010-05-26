#ifndef WX_PING_H
#define WX_PING_H

#include "server.h"
#include "ipaddr.h"
#include "../pinger/ping_result.h"

#include "../common/my_inet.h"
#include "../common/my_http.h"
#include "../common/my_thread.h"
#include "../common/my_mru.h"

#include <memory>

#include <boost/config/warning_disable.hpp> /* ������ unsafe */

//(*Headers(wx_Ping)
#include <wx/frame.h>
class wxPanel;
class wxTextCtrl;
class wxFlexGridSizer;
//*)

#include <wx/textctrl.h> 
#include <wx/bitmap.h> 

class wx_Ping: public wxFrame
{
private:
	typedef my::mru::list<unsigned short, pinger::ping_result> pings_list;

	bool terminate_;
	who::server &server_;
	tcp::socket socket_;
	who::object *object_;
	my::http::reply reply_;
	mutex read_mutex_;
	pings_list pings_;
	mutex pings_mutex_;
	bool archive_mode_;
	wxTextAttr last_archive_style_;
	std::wstring last_archive_text_;
	wxBitmap bitmap_;
	mutex bitmap_mutex_;
	int active_index_;

	void handle_read(const boost::system::error_code& error,
		size_t bytes_transferred);

	void repaint();

	//(*Handlers(wx_Ping)
	void OnClose(wxCloseEvent& event);
	void on_pingpanel_paint(wxPaintEvent& event);
	void on_pingpanel_mousemove(wxMouseEvent& event);
	void on_pingpanel_erasebackground(wxEraseEvent& event);
	void on_pingpanel_mouseleave(wxMouseEvent& event);
	//*)

	DECLARE_EVENT_TABLE()

protected:

	//(*Identifiers(wx_Ping)
	static const long ID_TEXTCTRL2;
	static const long ID_PANEL1;
	static const long ID_PINGPANEL;
	static const long ID_PINGTEXTCTRL;
	//*)
	
public:
	wx_Ping(wxWindow* parent, who::server &server, who::object *object);
	virtual ~wx_Ping();

	//(*Declarations(wx_Ping)
	wxPanel* m_pingpanel;
	wxPanel* Panel1;
	wxTextCtrl* m_ping_textctrl;
	wxTextCtrl* TextCtrl2;
	//*)
};

#endif
