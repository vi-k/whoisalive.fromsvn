#include "wx_Ping.h"

#include "../common/my_exception.h"
#include "../common/my_str.h"
#include "../common/my_num.h"
#include "../common/my_utf8.h"

#include <sstream>
#include <istream>
using namespace std;

#include <boost/bind.hpp>
#include <boost/system/system_error.hpp>

#include <wx/msgdlg.h>
#include <wx/dcclient.h>
#include <wx/dcmemory.h>

//(*InternalHeaders(wx_Ping)
#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <wx/panel.h>
#include <wx/settings.h>
#include <wx/intl.h>
#include <wx/string.h>
//*)

//(*IdInit(wx_Ping)
const long wx_Ping::ID_TEXTCTRL2 = wxNewId();
const long wx_Ping::ID_PANEL1 = wxNewId();
const long wx_Ping::ID_PINGPANEL = wxNewId();
const long wx_Ping::ID_PINGTEXTCTRL = wxNewId();
//*)

BEGIN_EVENT_TABLE(wx_Ping,wxFrame)
	//(*EventTable(wx_Ping)
	//*)
END_EVENT_TABLE()

#define BLOCK_W 4

wx_Ping::wx_Ping(wxWindow* parent, who::server &server, who::object *object)
	: terminate_(false)
	, server_(server)
	, socket_(server.io_service())
	, object_(object)
	, pings_(1000)
	, archive_mode_(false)
	, active_index_(-1)
{
	wxWindowID id = -1;

	//(*Initialize(wx_Ping)
	wxFlexGridSizer* FlexGridSizer1;

	Create(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE|wxFRAME_TOOL_WINDOW|wxFRAME_FLOAT_ON_PARENT, _T("wxID_ANY"));
	SetClientSize(wxSize(544,333));
	SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
	FlexGridSizer1 = new wxFlexGridSizer(3, 1, 0, 0);
	FlexGridSizer1->AddGrowableCol(0);
	FlexGridSizer1->AddGrowableRow(2);
	Panel1 = new wxPanel(this, ID_PANEL1, wxDefaultPosition, wxSize(400,24), wxTAB_TRAVERSAL, _T("ID_PANEL1"));
	TextCtrl2 = new wxTextCtrl(Panel1, ID_TEXTCTRL2, wxEmptyString, wxPoint(8,0), wxSize(384,24), 0, wxDefaultValidator, _T("ID_TEXTCTRL2"));
	FlexGridSizer1->Add(Panel1, 1, wxALL|wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	m_pingpanel = new wxPanel(this, ID_PINGPANEL, wxDefaultPosition, wxSize(400,61), wxTAB_TRAVERSAL, _T("ID_PINGPANEL"));
	FlexGridSizer1->Add(m_pingpanel, 1, wxALL|wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	m_ping_textctrl = new wxTextCtrl(this, ID_PINGTEXTCTRL, wxEmptyString, wxDefaultPosition, wxSize(400,97), wxTE_AUTO_SCROLL|wxTE_MULTILINE|wxTE_READONLY|wxTE_RICH2|wxTE_NOHIDESEL, wxDefaultValidator, _T("ID_PINGTEXTCTRL"));
	m_ping_textctrl->SetForegroundColour(wxColour(192,192,192));
	m_ping_textctrl->SetBackgroundColour(wxColour(0,0,0));
	FlexGridSizer1->Add(m_ping_textctrl, 1, wxALL|wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	SetSizer(FlexGridSizer1);
	FlexGridSizer1->SetSizeHints(this);

	m_pingpanel->Connect(ID_PINGPANEL,wxEVT_PAINT,(wxObjectEventFunction)&wx_Ping::on_pingpanel_paint,0,this);
	m_pingpanel->Connect(ID_PINGPANEL,wxEVT_ERASE_BACKGROUND,(wxObjectEventFunction)&wx_Ping::on_pingpanel_erasebackground,0,this);
	m_pingpanel->Connect(ID_PINGPANEL,wxEVT_MOTION,(wxObjectEventFunction)&wx_Ping::on_pingpanel_mousemove,0,this);
	m_pingpanel->Connect(ID_PINGPANEL,wxEVT_LEAVE_WINDOW,(wxObjectEventFunction)&wx_Ping::on_pingpanel_mouseleave,0,this);
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

		wstringstream out;
		wxTextAttr style;

		if (reply_.body == "START ARCHIVE\r\n")
		{
			archive_mode_ = true;
			m_ping_textctrl->SetDefaultStyle(*wxLIGHT_GREY);
			*m_ping_textctrl << L"������ ������\n";
		}
		else if (reply_.body == "END ARCHIVE\r\n")
		{
			archive_mode_ = false;
			m_ping_textctrl->SetDefaultStyle(*wxLIGHT_GREY);
			*m_ping_textctrl << L"����� ������\n";

			/*
			m_ping_textctrl->SetDefaultStyle(last_archive_style_);
			*m_ping_textctrl << last_archive_text_;
			repaint();
			*/
		}
		else
		{
			wstringstream ss( my::utf8::decode(reply_.body) );
			wstring host;
			ss >> host;

			pinger::ping_result ping;
			ss >> ping;

			wstringstream out;
			wxTextAttr style;

			unsigned short num = ping.sequence_number();

			if (ping.state() == pinger::ping_result::unknown)
			{
				style = wxTextAttr(*wxLIGHT_GREY);
				out << L"unknown message\n";
			}
			else
			{
				{
					scoped_lock l(pings_mutex_);
					pings_[num] = ping;
					active_index_ = -1;
				}

				switch (ping.state())
				{
					case pinger::ping_result::ok:
						style = wxTextAttr(*wxGREEN);
						out << my::time::to_fmt_wstring(L"[%H:%M:%S] ", ping.time())
							<< ping.ipv4_hdr().header_length()
							<< L" bytes from " << ping.ipv4_hdr().source_address()
							<< L", icmp_seq=" << num
							<< L", ttl=" << ping.ipv4_hdr().time_to_live()
							<< L", time=" << ping.duration().total_milliseconds() << L" ms"
							<< L"\n";
						break;

					case pinger::ping_result::timeout:
						style = wxTextAttr(*wxRED);
						out << my::time::to_fmt_wstring(L"[%H:%M:%S] ", ping.time())
							<< L"timeout (icmp_seq=" << num
							<< L", time=" << ping.duration().total_milliseconds() << L" ms"
							<< L")\n";
						break;
				}
			}

			m_ping_textctrl->SetDefaultStyle(style);
			*m_ping_textctrl << out.str();
			repaint();
				
			if (archive_mode_)
			{
				last_archive_style_ = style;
				last_archive_text_ = out.str();
			}
			//else
			//	repaint();
		
		} /* ������ ��������� ������ */

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

void wx_Ping::repaint()
{
	scoped_lock l(bitmap_mutex_);

	int w, h;
	m_pingpanel->GetClientSize(&w, &h);

	if (bitmap_.GetWidth() != w || bitmap_.GetHeight() != h)
		bitmap_.Create(w, h);

	wxMemoryDC dc(bitmap_);

	int y = h / 2;

	dc.SetBrush(*wxBLACK_BRUSH);
	dc.DrawRectangle(0, 0, w, h);

	dc.SetPen(*wxGREY_PEN);
	dc.DrawLine(0, y, w, y);

	pinger::ping_result prev_ping;
	int index = 0;

	scoped_lock ll(pings_mutex_);

	for (pings_list::iterator iter = pings_.begin();
		iter != pings_.end(); iter++)
	{
		pinger::ping_result ping = iter->value();
		int size = ping.duration().total_milliseconds() / 100;
		if (size > y)
			size = y;

		wxColour c1, c2;

		switch (ping.state())
		{
			case pinger::ping_result::ok:
				c1 = wxColour(0, 255, 0);
				c2 = wxColour(0, 224, 0);
				break;

			case pinger::ping_result::timeout:
				c1 = wxColour(255, 0, 0);
				c2 = wxColour(224, 0, 0);
				break;
		}

		int x = w - index * BLOCK_W - BLOCK_W;

		if (index == 0 || prev_ping.state() != ping.state())
		{
			/* ��������������� ���� */
			dc.SetPen(c1);
			dc.SetBrush(c2);
			dc.DrawRectangle(x, y - size, BLOCK_W, size * 2 + 1);
		}
		else
		{
			/* ����, ����������� � ������������ �� ����� */
			dc.SetPen(c1);
			dc.SetBrush(c2);
			dc.DrawRectangle(x, y - size, BLOCK_W + 1, size * 2 + 1);

			size = min(size, prev_ping.duration().total_milliseconds() / 100);

			if (size)
			{
				dc.SetPen(c2);
				dc.DrawLine(x + BLOCK_W, y - size + 1, x + BLOCK_W, y + size);
			}
		}

		prev_ping = ping;
		index++;
	}

	m_pingpanel->Refresh();
}

void wx_Ping::on_pingpanel_paint(wxPaintEvent& event)
{
	scoped_lock l(bitmap_mutex_);

	if (bitmap_.IsOk())
	{
		wxPaintDC dc(m_pingpanel);
		dc.DrawBitmap(bitmap_, 0, 0);

		if (active_index_ >= 0)
		{
			int w, h;
			m_pingpanel->GetClientSize(&w, &h);

			dc.SetPen(*wxWHITE_PEN);
			int x = w - active_index_ * BLOCK_W - BLOCK_W / 2;

			dc.DrawLine(x, 0, x, h);
		}
	}

	event.Skip(false);
}

void wx_Ping::on_pingpanel_erasebackground(wxEraseEvent& event)
{
	event.Skip(false);
}

void wx_Ping::on_pingpanel_mousemove(wxMouseEvent& event)
{
	int w, h;
	m_pingpanel->GetClientSize(&w, &h);

	active_index_ = (w - event.GetX() - 1) / BLOCK_W;
	m_pingpanel->Refresh();

	scoped_lock l(pings_mutex_);

	int index = active_index_;
	pings_list::iterator iter = pings_.begin();

	while (iter != pings_.end() && index-- != 0)
		iter++;

	if (iter != pings_.end())
	{
		wstringstream out;

		out << iter->value().state_to_wstring() << endl
			<< my::time::to_fmt_wstring(L"%Y-%m-%d %H:%M:%S", iter->value().time()) << endl
			<< L"icmp_seq=" << iter->key() << endl
			<< L"time=" << iter->value().duration().total_milliseconds() << L" ms";

		m_pingpanel->SetToolTip(out.str());
	}
}

void wx_Ping::on_pingpanel_mouseleave(wxMouseEvent& event)
{
	active_index_ = -1;
	m_pingpanel->Refresh();
}
