/***************************************************************
 * Name:      wx_Main.h
 * Purpose:   Defines Application Frame
 * Author:    vi.k (vi.k@mail.ru)
 * Author:    vi.k (vi.k@mail.ru)
 * Created:   2010-03-30
 * Copyright: vi.k ()
 * License:
 **************************************************************/

#ifndef WX_MAIN_H
#define WX_MAIN_H

#include "ipgui.h"
#include "server.h"
#include "window.h"

#include "wx\msw\winundef.h"

//(*Headers(wx_Frame)
#include <wx/notebook.h>
#include <wx/sizer.h>
#include <wx/menu.h>
#include <wx/panel.h>
#include <wx/frame.h>
#include <wx/statusbr.h>
//*)

class wx_Frame: public wxFrame
{
	public:

		wx_Frame(wxWindow* parent, wxWindowID id = -1);
		virtual ~wx_Frame();

		who::server::ptr server_;
		who::window *window_;
		ipwidget_t *menu_widget_;

		bool open_maps(bool new_tab = true);

		void map_mousewheel(who::window *win, int delta, int keys, int x, int y);
		void map_mousemove(who::window *win, int keys, int x, int y);
		void map_lbutton_down(who::window *win, int keys, int x, int y);
		void map_lbutton_up(who::window *win, int keys, int x, int y);
		void map_lbutton_dblclk(who::window *win, int keys, int x, int y);
		void map_rbutton_down(who::window *win, int keys, int x, int y);
		void map_rbutton_up(who::window *win, int keys, int x, int y);
		void map_rbutton_dblclk(who::window *win, int keys, int x, int y);
		void map_keydown(who::window *win, int key);
		void map_keyup(who::window *win, int key);

	private:

		//(*Handlers(wx_Frame)
		void OnQuit(wxCommandEvent& event);
		void OnAbout(wxCommandEvent& event);
		void OnNotebookPageChanged(wxNotebookEvent& event);
		void OnMenuItem_AckSelected(wxCommandEvent& event);
		void OnMenuItem_UnackSelected(wxCommandEvent& event);
		void skip_setfocus(wxFocusEvent& event);
		void skip_leftdown(wxMouseEvent& event);
		void OnMenuPing_Selected(wxCommandEvent& event);
		//*)

		//(*Identifiers(wx_Frame)
		static const long ID_NOTEBOOK1;
		static const long ID_PANEL1;
		static const long idMenuQuit;
		static const long idMenuAbout;
		static const long ID_STATUSBAR1;
		static const long ID_MENUITEM_ACK;
		static const long ID_MENUITEM_UNACK;
		static const long ID_MENUITEM_PING;
		//*)

		//(*Declarations(wx_Frame)
		wxStatusBar* m_statusbar;
		wxMenuItem* m_objectmenu_ack;
		wxPanel* m_map_panel;
		wxMenuItem* m_objectmenu_unack;
		wxNotebook* m_notebook;
		wxFlexGridSizer* FlexGridSizer1;
		wxMenu m_object_menu;
		//*)

		DECLARE_EVENT_TABLE()
};

#endif // WX_MAIN_H
