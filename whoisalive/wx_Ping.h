#ifndef WX_PING_H
#define WX_PING_H

//(*Headers(wx_Ping)
#include <wx/frame.h>
class wxPanel;
class wxStaticBitmap;
class wxTextCtrl;
class wxFlexGridSizer;
//*)

#include "ipaddr.h"

class wx_Ping: public wxFrame
{
private:
	ipaddr_t addr_;

	//(*Handlers(wx_Ping)
	void OnClose(wxCloseEvent& event);
	//*)

	DECLARE_EVENT_TABLE()

protected:

	//(*Identifiers(wx_Ping)
	static const long ID_TEXTCTRL2;
	static const long ID_PANEL1;
	static const long ID_STATICBITMAP2;
	static const long ID_TEXTCTRL1;
	//*)

public:
		wx_Ping(wxWindow* parent,wxWindowID id=wxID_ANY,const wxPoint& pos=wxDefaultPosition,const wxSize& size=wxDefaultSize);
		virtual ~wx_Ping();

		void Run(ipaddr_t addr);

		//(*Declarations(wx_Ping)
		wxStaticBitmap* StaticBitmap2;
		wxPanel* Panel1;
		wxTextCtrl* TextCtrl2;
		wxTextCtrl* TextCtrl1;
		//*)
};

#endif
