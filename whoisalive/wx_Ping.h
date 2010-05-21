#ifndef WX_PING_H
#define WX_PING_H

#include "server.h"
#include "ipaddr.h"

#include "../common/my_inet.h"
#include "../common/my_http.h"
#include "../common/my_thread.h"

#include <boost/config/warning_disable.hpp> /* против unsafe */

//(*Headers(wx_Ping)
#include <wx/frame.h>
class wxPanel;
class wxStaticBitmap;
class wxTextCtrl;
class wxFlexGridSizer;
//*)

class wx_Ping: public wxFrame
{
private:
	bool terminate_;
	who::server &server_;
	tcp::socket socket_;
	ipobject_t *object_;
	my::http::reply reply_;
	mutex read_mutex_;

	void handle_read(const boost::system::error_code& error,
		size_t bytes_transferred);

	void handle_stop(void);

	//(*Handlers(wx_Ping)
	void OnClose(wxCloseEvent& event);
	//*)

	DECLARE_EVENT_TABLE()

protected:

	//(*Identifiers(wx_Ping)
	static const long ID_TEXTCTRL2;
	static const long ID_PANEL1;
	static const long ID_STATICBITMAP2;
	static const long ID_PINGTEXTCTRL;
	//*)
	
public:
	wx_Ping(wxWindow* parent, who::server &server, ipobject_t *object);
	virtual ~wx_Ping();

	//(*Declarations(wx_Ping)
	wxStaticBitmap* m_bitmap;
	wxPanel* Panel1;
	wxTextCtrl* m_ping_textctrl;
	wxTextCtrl* TextCtrl2;
	//*)
};

#endif
