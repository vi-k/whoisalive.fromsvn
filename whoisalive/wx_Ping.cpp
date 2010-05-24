#include "wx_Ping.h"

#include "../common/my_exception.h"
#include "../common/my_str.h"

#include <sstream>
#include <istream>
using namespace std;

#include <boost/bind.hpp>
#include <boost/system/system_error.hpp>

#include <wx/msgdlg.h>

//(*InternalHeaders(wx_Ping)
#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <wx/panel.h>
#include <wx/settings.h>
#include <wx/intl.h>
#include <wx/statbmp.h>
#include <wx/string.h>
//*)

//(*IdInit(wx_Ping)
const long wx_Ping::ID_TEXTCTRL2 = wxNewId();
const long wx_Ping::ID_PANEL1 = wxNewId();
const long wx_Ping::ID_STATICBITMAP2 = wxNewId();
const long wx_Ping::ID_PINGTEXTCTRL = wxNewId();
//*)

BEGIN_EVENT_TABLE(wx_Ping,wxFrame)
	//(*EventTable(wx_Ping)
	//*)
END_EVENT_TABLE()

wx_Ping::wx_Ping(wxWindow* parent, who::server &server, ipobject_t *object)
	: terminate_(false)
	, server_(server)
	, socket_(server.io_service())
	, object_(object)
{
	wxWindowID id = -1;

	//(*Initialize(wx_Ping)
	wxFlexGridSizer* FlexGridSizer1;

	Create(parent, id, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER|wxFRAME_TOOL_WINDOW|wxFRAME_FLOAT_ON_PARENT, _T("id"));
	SetClientSize(wxSize(544,333));
	Move(wxDefaultPosition);
	SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
	FlexGridSizer1 = new wxFlexGridSizer(3, 1, 0, 0);
	FlexGridSizer1->AddGrowableCol(0);
	FlexGridSizer1->AddGrowableRow(2);
	Panel1 = new wxPanel(this, ID_PANEL1, wxDefaultPosition, wxSize(173,24), wxTAB_TRAVERSAL, _T("ID_PANEL1"));
	TextCtrl2 = new wxTextCtrl(Panel1, ID_TEXTCTRL2, wxEmptyString, wxPoint(8,0), wxSize(384,24), 0, wxDefaultValidator, _T("ID_TEXTCTRL2"));
	FlexGridSizer1->Add(Panel1, 1, wxALL|wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	m_bitmap = new wxStaticBitmap(this, ID_STATICBITMAP2, wxNullBitmap, wxDefaultPosition, wxSize(400,58), 0, _T("ID_STATICBITMAP2"));
	FlexGridSizer1->Add(m_bitmap, 1, wxALL|wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	m_ping_textctrl = new wxTextCtrl(this, ID_PINGTEXTCTRL, wxEmptyString, wxDefaultPosition, wxSize(400,140), wxTE_AUTO_SCROLL|wxTE_MULTILINE|wxTE_READONLY|wxTE_RICH2|wxTE_NOHIDESEL, wxDefaultValidator, _T("ID_PINGTEXTCTRL"));
	m_ping_textctrl->SetForegroundColour(wxColour(192,192,192));
	m_ping_textctrl->SetBackgroundColour(wxColour(0,0,0));
	FlexGridSizer1->Add(m_ping_textctrl, 1, wxALL|wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	SetSizer(FlexGridSizer1);
	FlexGridSizer1->SetSizeHints(this);

	Connect(wxID_ANY,wxEVT_CLOSE_WINDOW,(wxObjectEventFunction)&wx_Ping::OnClose);
	//*)


	if (object->ipaddrs().size() == 0)
		throw my::exception(L"������ �� �������� �� ���� �����");

	if (object->ipaddrs().size() > 1)
		throw my::exception(L"������ �������� ������ ������ ������");

	ipaddr_t addr = object->ipaddrs().front();
	wstring addr_s = addr.str<wchar_t>();

	wstring name = object->name() + L" / " + addr_s;
	SetLabel(name);

	server_.get_header(socket_, reply_,
		wstring(L"/pinger/ping.log?address=") + addr_s);

	reply_.buf_.consume(reply_.buf_.size());
	reply_.buf_.prepare(65536);

	asio::async_read_until(
		socket_, reply_.buf_, "\r\n",
		boost::bind(&wx_Ping::handle_read, this,
            boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred) );

	server_.io_wake_up();

	Show();
}

wx_Ping::~wx_Ping()
{
	terminate_ = true;

	scoped_lock l(read_mutex_);
	socket_.close();

	//(*Destroy(wx_Ping)
	//*)
}

void wx_Ping::OnClose(wxCloseEvent& event)
{
	delete this;
}

void wx_Ping::handle_read(const boost::system::error_code& error,
	size_t bytes_transferred)
{
	scoped_lock l(read_mutex_);

	if (terminate_)
		return;

	if (!error)
	{
		reply_.body.resize(bytes_transferred);
		reply_.buf_.sgetn((char*)reply_.body.c_str(), bytes_transferred);

		xml::wptree pt;
		reply_.to_xml(pt);

		wstring node_name = pt.front().first;
		xml::wptree &node = pt.front().second.get_child(L"<xmlattr>");

		if (node_name == L"reply")
		{
			posix_time::ptime time = my::time::utc_to_local(
				my::time::to_time( node.get<wstring>(L"start") ) );

			m_ping_textctrl->SetDefaultStyle(wxTextAttr(*wxGREEN));
			*m_ping_textctrl
				<< my::time::to_fmt_wstring(L"%Y-%m-%d %H:%M:%S", time)
				<< L": ok icmp_seq=" << node.get<wstring>(L"icmp_seq", L"?")
				<< L", time=" << node.get<wstring>(L"time", L"?")
				<< L", ttl=" << node.get<wstring>(L"ttl", L"?")
				<< L"\n";
		}
		else if (node_name == L"timeout")
		{
			posix_time::ptime time = my::time::utc_to_local(
				my::time::to_time( node.get<wstring>(L"start") ) );

			m_ping_textctrl->SetDefaultStyle(wxTextAttr(*wxRED));
			*m_ping_textctrl
				<< my::time::to_fmt_wstring(L"%Y-%m-%d %H:%M:%S", time)
				<< L": timeout icmp_seq=" << node.get<wstring>(L"icmp_seq", L"?")
				<< L"\n";
		}
		else
		{
			m_ping_textctrl->SetDefaultStyle(wxTextAttr(*wxRED));
			*m_ping_textctrl << L"unknown message\n";
		}

		asio::async_read_until(
			socket_, reply_.buf_, "\r\n",
			boost::bind(&wx_Ping::handle_read, this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred) );
	}
	else
	{
		boost::system::system_error se(error);
		wstring str = my::str::to_wstring( se.what() );

		wxMessageBox(str, L"������ ������ ������",
			wxOK | wxICON_ERROR, this);
		Close();
	}
}
