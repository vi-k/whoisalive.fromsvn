/***************************************************************
 * Name:	  wx_Main.cpp
 * Purpose:   Code for Application Frame
 * Author:    vi.k (vi.k@mail.ru)
 * Created:   2010-03-30
 * Copyright: vi.k ()
 * License:
 **************************************************************/

#include <boost/config/warning_disable.hpp> /* против unsafe */

#include <wx/msgdlg.h>
#include "wx_Main.h"
#include "wx_App.h"
#include "wx_Ping.h"

#include "config.h"

#include "../common/my_xml.h"
#include "../common/my_time.h"
#include "../common/my_fs.h"
#include "../common/my_log.h"

#include <string>
#include <fstream>
using namespace std;

#include <boost/bind.hpp>
#include <boost/archive/detail/utf8_codecvt_facet.hpp>

//(*InternalHeaders(wx_Frame)
#include <wx/artprov.h>
#include <wx/bitmap.h>
#include <wx/settings.h>
#include <wx/intl.h>
#include <wx/image.h>
#include <wx/string.h>
//*)

#include <wx/filename.h>

#ifdef _DEBUG
HWND g_parent_wnd = NULL;
#endif

WNDPROC g_OldMapPanelWndProc;
extern wx_App *App;

wofstream main_log_stream;
void on_main_log(const wstring &title, const wstring &text)
{
	main_log_stream << my::time::to_fmt_wstring(L"[%Y-%m-%d %H:%M:%S] ",
		posix_time::microsec_clock::universal_time());
	main_log_stream << title << endl;
	if (!text.empty())
		main_log_stream << text << endl;
	main_log_stream << endl;
	main_log_stream.flush();
}
my::log main_log(on_main_log);


LRESULT CALLBACK MapPanelWndProc( HWND hwnd, UINT uMsg,
		WPARAM wParam, LPARAM lParam)
{
	switch( uMsg)
	{
		case WM_WINDOWPOSCHANGED:
			MoveWindow( FindWindowEx(hwnd, NULL, NULL, NULL),
					0, 0, ((WINDOWPOS*)lParam)->cx, ((WINDOWPOS*)lParam)->cy,
					FALSE );
			break;

		case WM_SETFOCUS:
			/* Фокус на первого попавшегося ребёнка */
			SetFocus( FindWindowEx(hwnd, NULL, NULL, NULL) );
			break;

		case WM_KEYDOWN:
			if( wParam == VK_ESCAPE) SetCapture(NULL);
			break;
	}

	return CallWindowProc( g_OldMapPanelWndProc, hwnd, uMsg, wParam, lParam);
}

//(*IdInit(wx_Frame)
const long wx_Frame::ID_NOTEBOOK1 = wxNewId();
const long wx_Frame::ID_PANEL1 = wxNewId();
const long wx_Frame::idMenuQuit = wxNewId();
const long wx_Frame::idMenuAbout = wxNewId();
const long wx_Frame::ID_STATUSBAR1 = wxNewId();
const long wx_Frame::ID_MENUITEM_ACK = wxNewId();
const long wx_Frame::ID_MENUITEM_UNACK = wxNewId();
const long wx_Frame::ID_MENUITEM_PING = wxNewId();
//*)

BEGIN_EVENT_TABLE(wx_Frame,wxFrame)
	//(*EventTable(wx_Frame)
	//*)
END_EVENT_TABLE()

wx_Frame::wx_Frame(wxWindow* parent, wxWindowID id)
	: ipwindow_(NULL)
	, menu_widget_(NULL)
{
	//(*Initialize(wx_Frame)
	wxMenuItem* MenuItem2;
	wxMenuItem* MenuItem1;
	wxMenu* Menu1;
	wxMenuBar* m_menubar;
	wxMenuItem* m_objectmenu_ping;
	wxMenu* Menu2;

	Create(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE, _T("wxID_ANY"));
	SetClientSize(wxSize(263,119));
	SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
	{
	wxIcon FrameIcon;
	FrameIcon.CopyFromBitmap(wxArtProvider::GetBitmap(wxART_MAKE_ART_ID_FROM_STR(_T("wxART_TIP")),wxART_FRAME_ICON));
	SetIcon(FrameIcon);
	}
	FlexGridSizer1 = new wxFlexGridSizer(3, 1, 0, 0);
	FlexGridSizer1->AddGrowableCol(0);
	FlexGridSizer1->AddGrowableRow(1);
	m_notebook = new wxNotebook(this, ID_NOTEBOOK1, wxDefaultPosition, wxSize(363,32), 0, _T("ID_NOTEBOOK1"));
	FlexGridSizer1->Add(m_notebook, 1, wxTOP|wxLEFT|wxRIGHT|wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	m_map_panel = new wxPanel(this, ID_PANEL1, wxDefaultPosition, wxSize(616,348), wxTAB_TRAVERSAL, _T("ID_PANEL1"));
	FlexGridSizer1->Add(m_map_panel, 1, wxALL|wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	SetSizer(FlexGridSizer1);
	m_menubar = new wxMenuBar();
	Menu1 = new wxMenu();
	MenuItem1 = new wxMenuItem(Menu1, idMenuQuit, _("Выход\tAlt-F4"), wxEmptyString, wxITEM_NORMAL);
	Menu1->Append(MenuItem1);
	m_menubar->Append(Menu1, _("Файл"));
	Menu2 = new wxMenu();
	MenuItem2 = new wxMenuItem(Menu2, idMenuAbout, _("О программе...\tF1"), wxEmptyString, wxITEM_NORMAL);
	Menu2->Append(MenuItem2);
	m_menubar->Append(Menu2, _("Помощь"));
	SetMenuBar(m_menubar);
	m_statusbar = new wxStatusBar(this, ID_STATUSBAR1, 0, _T("ID_STATUSBAR1"));
	int __wxStatusBarWidths_1[1] = { -1 };
	int __wxStatusBarStyles_1[1] = { wxSB_NORMAL };
	m_statusbar->SetFieldsCount(1,__wxStatusBarWidths_1);
	m_statusbar->SetStatusStyles(1,__wxStatusBarStyles_1);
	SetStatusBar(m_statusbar);
	m_objectmenu_ack = new wxMenuItem((&m_object_menu), ID_MENUITEM_ACK, _("Квитировать"), wxEmptyString, wxITEM_NORMAL);
	m_object_menu.Append(m_objectmenu_ack);
	m_objectmenu_unack = new wxMenuItem((&m_object_menu), ID_MENUITEM_UNACK, _("Убрать квитирование"), wxEmptyString, wxITEM_NORMAL);
	m_object_menu.Append(m_objectmenu_unack);
	m_objectmenu_ping = new wxMenuItem((&m_object_menu), ID_MENUITEM_PING, _("Состояние хоста"), wxEmptyString, wxITEM_NORMAL);
	m_object_menu.Append(m_objectmenu_ping);
	FlexGridSizer1->SetSizeHints(this);

	Connect(ID_NOTEBOOK1,wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGED,(wxObjectEventFunction)&wx_Frame::OnNotebookPageChanged);
	Connect(idMenuQuit,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&wx_Frame::OnQuit);
	Connect(idMenuAbout,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&wx_Frame::OnAbout);
	Connect(ID_MENUITEM_ACK,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&wx_Frame::OnMenuItem_AckSelected);
	Connect(ID_MENUITEM_UNACK,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&wx_Frame::OnMenuItem_UnackSelected);
	Connect(ID_MENUITEM_PING,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&wx_Frame::OnMenuPing_Selected);
	Connect(wxID_ANY,wxEVT_LEFT_DOWN,(wxObjectEventFunction)&wx_Frame::skip_leftdown);
	//*)

	bool log_exists = fs::exists(L"whoisalive.log");

	main_log_stream.open(L"whoisalive.log", ios::app);

	if (!log_exists)
		main_log_stream << L"\xEF\xBB\xBF";

	main_log_stream.imbue( locale( main_log_stream.getloc(),
		new boost::archive::detail::utf8_codecvt_facet) );

	main_log(L"Start");

	m_object_menu.Remove(m_objectmenu_unack);

	m_notebook->Connect(wxID_ANY,wxEVT_SET_FOCUS,(wxObjectEventFunction)&wx_Frame::skip_setfocus,0,this);
	Connect(ID_NOTEBOOK1,wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGED,(wxObjectEventFunction)&wx_Frame::OnNotebookPageChanged);

#ifdef _DEBUG
	g_parent_wnd = (HWND)GetHandle();
	SetLabel( NAME L" " VERSION L" " BUILDNO L" " BUILDDATE L" " BUILDTIME );
#else
	SetLabel( NAME L" " VERSION);
#endif

	/* Подменяем у формы WndProc, чтобы перехватывать WM_MOUSEWHEEL
		и отправлять их своему окну. Другого выхода не нашёл */
	g_OldMapPanelWndProc =
			(WNDPROC)GetWindowLongPtr( (HWND)m_map_panel->GetHandle(), GWLP_WNDPROC);

	SetWindowLongPtr( (HWND)m_map_panel->GetHandle(),
			GWLP_WNDPROC, (ULONG_PTR)MapPanelWndProc);

	ShowWindow( (HWND)GetHandle(), SW_MAXIMIZE);

	/*-
	wxString str = wxFileName::GetCwd();
	wxMessageBox(str, L"Warning");
	wxMessageBox(_("Привет"), L"Warning");
	-*/

	xml::wptree config;
	my::xml::load(L"config.xml", config);

	server_.reset( new who::server(config.get_child(L"config.server")) );

	ipwindow_ = server_->add_window( (HWND)m_map_panel->GetHandle() );

	ipwindow_->on_mouse_wheel = boost::bind(&wx_Frame::map_mousewheel,this,_1,_2,_3,_4,_5);
	ipwindow_->on_mouse_move = boost::bind(&wx_Frame::map_mousemove,this,_1,_2,_3,_4);
	ipwindow_->on_lbutton_down = boost::bind(&wx_Frame::map_lbutton_down,this,_1,_2,_3,_4);
	ipwindow_->on_lbutton_up = boost::bind(&wx_Frame::map_lbutton_up,this,_1,_2,_3,_4);
	ipwindow_->on_lbutton_dblclk = boost::bind(&wx_Frame::map_lbutton_dblclk,this,_1,_2,_3,_4);
	ipwindow_->on_rbutton_down = boost::bind( &wx_Frame::map_rbutton_down,this,_1,_2,_3,_4);
	ipwindow_->on_rbutton_up = boost::bind( &wx_Frame::map_rbutton_up,this,_1,_2,_3,_4);
	ipwindow_->on_rbutton_dblclk = boost::bind(&wx_Frame::map_rbutton_dblclk,this,_1,_2,_3,_4);
	ipwindow_->on_keydown = boost::bind( &wx_Frame::map_keydown,this,_1,_2);
	ipwindow_->on_keyup = boost::bind( &wx_Frame::map_keyup,this,_1,_2);

	open_maps();

	m_notebook->SetSelection(0);
}

wx_Frame::~wx_Frame()
{
	//(*Destroy(wx_Frame)
	//*)

	SetWindowLongPtr( (HWND)m_map_panel->GetHandle(), GWLP_WNDPROC,
			(ULONG_PTR)g_OldMapPanelWndProc );
}

bool wx_Frame::open_maps(bool new_tab)
{
	my::http::reply reply;
	server_->get(reply, L"/schemes.xml");

	xml::wptree maps;
	try
	{
		reply.to_xml(maps);
	}
	catch(my::exception &e)
	{
		throw e << my::param(L"request", L"/schemes.xml");
	}

	ipwindow_->add_maps(maps.get_child(L"schemes"));

	if (new_tab)
	{
		int count = ipwindow_->get_maps_count();
		for (int i = 0; i < count; i++)
		{
			ipmap_t *map = ipwindow_->get_map(i);
			wxPanel *panel = new wxPanel(m_notebook);
			panel->Connect(wxID_ANY,wxEVT_LEFT_DOWN,(wxObjectEventFunction)&wx_Frame::skip_leftdown);
			m_notebook->AddPage(panel, map->get_name(), false);
		}
	}

	return true;
}

void wx_Frame::OnNotebookPageChanged(wxNotebookEvent& event)
{
	/* Если GetSelection() вернёт значение вне диапазон, ничего не произойдёт */
	ipwindow_->set_active_map( m_notebook->GetSelection() );
}

void wx_Frame::OnQuit(wxCommandEvent& event)
{
	Close();
}

void wx_Frame::OnAbout(wxCommandEvent& event)
{
	wxMessageBox( L"Навигация по карте - мышкой.\n\n"
		L"Изменение масштаба: +, -, колесо мыши\n\n"
		L"Переключение карт:\n1 - спутник;\n2 - карта.\n\n"
		L"Состояние объектов:\n"
		L"-  зелёный цвет - пингуется;\n"
		L"-  жёлтый - пропал сигнал;\n"
		L"-  красный - не ответил больше 4 раз;\n"
		L" - любой другой цвет - в группе находятся объекты с различным состоянием.\n\n"
		L"После перехода в красный режим, объект начинает мигать,"
		L" чтобы обратить на себя внимание оператора."
		L" Он продолжит мигать, даже если позже вернётся в зелёный."
		L" Это сделано на случай, если оператор отсутствовал и мог"
		L" не заметить исчезновение сигнала. Единственный способ прекратить"
		L" мигание - квитировать (правая кнопка мыши на объекте, либо кнопка"
		L" Enter на клавиатуре (для всех объектов сразу))\n\n"
		L"\"Раскрыть\" группу объектов - левая кнопка мыши на объекте.\n"
		L"\"Спрятать\" объектов в группу - Shift + левая кнопка мыши на объекте.\n\n"
		L"Индикатор над значком: состояние объектов в группе."
		, L"whoisalive");
}

void wx_Frame::map_mousewheel(ipwindow_t *win, int delta, int keys, int x, int y)
{
	/* Изменение масштаба */
	float ds;

	if (delta >= 0)
		ds = 2.0f /*1.414214f*/ * delta;
	else
		ds = 1.0f / (2.0f * (-delta));

	//win->zoom(ds);

	ipmap_t *map = ipwindow_->active_map();
	if (map)
	{
		float fx = (float)x;
		float fy = (float)y;
		map->window_to_client(&fx, &fy);
		map->zoom(ds, fx, fy, 2);
	}
}

void wx_Frame::map_mousemove(ipwindow_t *win, int keys, int x, int y)
{
//	Button2->Caption = (String)x + L"," + y;
	win->mouse_move_to(x, y);
}

void wx_Frame::map_lbutton_down(ipwindow_t *win, int keys, int x, int y)
{
	//if (GetKeyState(' ') & 0x80)
	//	win->mouse_start(mousemode::move, x, y);
	//else
	if (keys & mousekeys::ctrl)
		win->mouse_start(mousemode::select, x, y);
	else
		win->mouse_start(mousemode::move, x, y);
}

void wx_Frame::map_lbutton_up(ipwindow_t *win, int keys, int x, int y)
{
	win->mouse_end(x, y);

	ipwidget_t *widget = win->hittest(x, y);
	if (!widget)
		return;

	ipobject_t *object = dynamic_cast<ipobject_t*>(widget);
	if (!object)
		return;

	if ( !(keys & mousekeys::shift) )
	{
		float scale = object->lim_scale_max();
		if (scale != 0.0f)
		{
			int new_z = ipmap_t::z(scale) + 1;
			scale = 1 << (new_z - 1);
			int steps = new_z - ipmap_t::z(win->active_map()->scale());
			if (steps)
			{
				win->active_map()->scale__( scale, object->x(), object->y(),
					2 * steps);
				return;
			}
		}
	}
	else
	{
		float scale = object->lim_scale_min();
		if (scale != 0.0f)
		{
			int new_z = ipmap_t::z(scale);
			scale = 1 << (new_z - 1);
			int steps = ipmap_t::z(win->active_map()->scale()) - new_z;
			if (steps)
			{
				win->active_map()->scale__( scale, object->x(), object->y(),
					2 * steps);
				return;
			}
		}
	}
}

void wx_Frame::map_lbutton_dblclk(ipwindow_t *win, int keys, int x, int y)
{
	ipwidget_t *widget = win->hittest(x, y);

	if (widget)
	{
		ipobject_t *object = dynamic_cast<ipobject_t*>(widget);
		if (object)
			object->acknowledge();
	}
}

void wx_Frame::map_rbutton_down(ipwindow_t *win, int keys, int x, int y)
{
	menu_widget_ = win->hittest(x, y);
}

void wx_Frame::map_rbutton_up(ipwindow_t *win, int keys, int x, int y)
{
	if (menu_widget_)
	{
		ipobject_t *object = dynamic_cast<ipobject_t*>(menu_widget_);
		if (object)
		{
			bool acknowledged = object->acknowledged();
			m_objectmenu_ack->Enable( !acknowledged );
			m_objectmenu_unack->Enable( acknowledged );

			m_map_panel->PopupMenu(&m_object_menu, x, y);
		}
	}
}

void wx_Frame::map_rbutton_dblclk(ipwindow_t *win, int keys, int x, int y)
{
}

void wx_Frame::map_keydown(ipwindow_t *win, int key)
{
	switch (key)
	{
		case VK_ESCAPE:
			if (win->active_map()) win->active_map()->unselect_all();
			break;

		case ' ':
			if (win->mouse_mode() == mousemode::none)
			{
				win->mouse_start(mousemode::capture, 0, 0);
				::SetCursor( LoadCursor(NULL, IDC_HAND) );
			}
			break;
	}
}

void wx_Frame::map_keyup(ipwindow_t *win, int key)
{
	switch (key)
	{
		case ' ':
			if (win->mouse_mode() == mousemode::capture)
				win->mouse_cancel();
			break;

		case VK_DIVIDE:
			//win->set_scale(1.0f);
			break;

		case VK_ADD:
			win->zoom(2.0f);
			break;

		case VK_SUBTRACT:
			win->zoom(0.5f);
			break;

		case VK_RETURN:
			server_->acknowledge_all();
			break;

		case VK_F8:
			server_->unacknowledge_all();
			break;

		case VK_MULTIPLY:
			ipwindow_->align();
			::SetFocus( FindWindowEx( (HWND)m_map_panel->GetHandle(), NULL, NULL, NULL ) );
			break;

		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			server_->set_active_map(key - '0');
			win->animate();
			break;

		case VK_F5:
		{
			server_->acknowledge_all();
			SleepEx(300, TRUE);

			ipmap_t *map = ipwindow_->active_map();
			float x = map->x();
			float y = map->y();
			float scale = map->scale();

			ipwindow_->clear();
			open_maps(false);

			ipwindow_->set_active_map( m_notebook->GetSelection() );

			map = ipwindow_->active_map();
			map->set_pos(x, y, 0);
			map->set_scale(scale, 0);
			break;
		}
	}
}

void wx_Frame::OnMenuItem_AckSelected(wxCommandEvent& event)
{
	if (menu_widget_)
	{
		ipobject_t *object = dynamic_cast<ipobject_t*>(menu_widget_);
		if (object)
			object->acknowledge();
	}
}

void wx_Frame::OnMenuItem_UnackSelected(wxCommandEvent& event)
{
	if (menu_widget_)
	{
		ipobject_t *object = dynamic_cast<ipobject_t*>(menu_widget_);
		if (object)
			object->unacknowledge();
	}
}

void wx_Frame::skip_setfocus(wxFocusEvent& event)
{
	::SetFocus( FindWindowEx( (HWND)m_map_panel->GetHandle(), NULL, NULL, NULL ) );
}

void wx_Frame::skip_leftdown(wxMouseEvent& event)
{
	event.Skip(false);
}

void wx_Frame::OnMenuPing_Selected(wxCommandEvent& event)
{
	if (menu_widget_)
	{
		ipobject_t *object = dynamic_cast<ipobject_t*>(menu_widget_);
		if (object)
		{
			try
			{
				new wx_Ping(this, *server_, object); /* Удалит себя сам */
			}
			catch(my::exception &e)
			{
				wxMessageBox(e.message(), L"Ошибка", wxOK | wxICON_ERROR);
			}
		}
	}
}
