/***************************************************************
 * Name:	  wx_App.cpp
 * Purpose:   Code for Application Class
 * Author:    vi.k (vi.k@mail.ru)
 * Created:   2010-03-30
 * Copyright: vi.k ()
 * License:
 **************************************************************/

#include <boost/config/warning_disable.hpp> /* против unsafe */

#include "ipgui.h"
#include "wx_App.h"

//(*AppHeaders
#include "wx_Main.h"
#include <wx/image.h>
//*)

#include "../common/my_str.h"
#include "../common/my_exception.h"
#include "../common/my_log.h"
extern my::log main_log;

#include <exception>
using namespace std;

IMPLEMENT_APP(wx_App);

wx_App *App;

bool wx_App::OnInit()
{
	App =  this;

	wxHandleFatalExceptions(true);

	//(*AppInitialize
	bool wxsOK = true;
	wxInitAllImageHandlers();
	if ( wxsOK )
	{
	wx_Frame* Frame = new wx_Frame(0);
	Frame->Show();
	SetTopWindow(Frame);
	}
	//*)
	return wxsOK;

}

bool wx_App::OnExceptionInMainLoop()
{
	try
	{
		throw;
	}
	catch (...)
	{
		main_log(L"unknown exception (App::ExceptionInMainLoop)");
		wxMessageBox( L"Неизвестное исключение в MainLoop",
			L"Ошибка", wxOK | wxICON_ERROR);
	}

	return true;
}

void wx_App::OnUnhandledException()
{
	try
	{
		throw;
	}
	catch (my::exception &e)
	{
		wstring error = e.message();
		main_log(L"my::exception (App::UnhandledException)", error);
		wxMessageBox(error.c_str(), L"Ошибка", wxOK | wxICON_ERROR);
	}
	catch (exception &e)
	{
		my::exception my_e(e);
		wstring error = my_e.message();
		main_log(L"std::exception (App::UnhandledException)", error);
		wxMessageBox(error.c_str(),
			L"std::exception", wxOK | wxICON_ERROR);
	}
	catch ( ... )
	{
		main_log(L"unknown exception (App::UnhandledException)");
		wxMessageBox(L"Неизвестное исключение, программа будет закрыта.",
			L"Ошибка", wxOK | wxICON_ERROR);
	}
}

void wx_App::OnFatalException()
{
	main_log(L"unknown exception (App::FatalException)");
	wxMessageBox( L"Program has crashed and will terminate.",
		L"Критическая ошибка", wxOK | wxICON_ERROR);
}
