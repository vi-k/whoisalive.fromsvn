#include "window.h"
#include "server.h"

#include "../common/my_time.h"

#include <math.h>
#include <stdio.h>

#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#include <windowsx.h>

#ifdef _DEBUG
extern HWND g_parent_wnd; /* Для отладки */
#endif

namespace who {

const wchar_t* widget_type(widget *widg)
{
	return
		dynamic_cast<scheme*>(widg) ? L"scheme" :
		dynamic_cast<ipimage_t*>(widg) ? L"image" :
		dynamic_cast<object*>(widg) ? L"object" :
		widg ? L"unknown" : L"null";
}

#pragma warning(disable:4355) /* 'this' : used in base member initializer list */

window::window(server &server, HWND parent)
	: server_(server)
	, hwnd_(NULL)
	, focused_(false)
	, w_(0)
	, h_(0)
	, bg_color_(255,255,255)
	, active_scheme_(NULL)
	, mouse_mode_(mousemode::none)
	, mouse_start_x_(0)
	, mouse_start_y_(0)
	, mouse_end_x_(0)
	, mouse_end_y_(0)
	, select_parent_(NULL)
	, select_rect_(0.0f, 0.0f, 0.0f, 0.0f)
	, terminate_(false)
{
	/* Создание внутреннего окна для обработки сообщений от внешнего окна */
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	wc.lpfnWndProc = &static_wndproc_;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = NULL;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = L"who::window";

	RegisterClass(&wc);

	set_link(parent);

	anim_thread_ = boost::thread( boost::bind(&window::anim_thread_proc, this) );
}

window::~window()
{
	/* Ждём завершения animate-потока */
	{
		terminate_ = true; /* Сообщаем потоку о прекращении работы */
		animate(); /* Будим поток */
		anim_thread_.join(); /* Ждём завершения */
	}
	
	delete_link();

	schemes_.clear();
}

/* Анимация карты */
void window::anim_thread_proc(void)
{
	asio::io_service io_service;
	asio::deadline_timer timer(io_service, posix_time::microsec_clock::universal_time());
	
	while (!terminate_)
	{
		/* Анимируются все карты */
		bool anim = false;
		BOOST_FOREACH(who::scheme &scheme, schemes_)
			if (scheme.animate_calc())
				anim = true;

		/* ... но прорисовывается только одно - активное */
		paint_();

		/* Для отладки */
		#ifdef _DEBUG
		if (active_scheme_)
		{
			static int count = 0;
			wchar_t buf[200];

			swprintf_s( buf, sizeof(buf) / sizeof(*buf), L"[%08X] %d - %.3f (%d) - %d",
				hwnd_, ++count,
				active_scheme_->scale(), GetCurrentThreadId(),
				scheme::z(active_scheme_->scale()) );

			SetWindowText(g_parent_wnd, buf);
		}
		#endif

		boost::posix_time::ptime time = timer.expires_at() + server_.anim_period();
		boost::posix_time::ptime now = posix_time::microsec_clock::universal_time();

		/* Теоретически время следующей прорисовки должно быть относительным
			от времени предыдущей, но на практике могут возникнуть торможения,
			и, тогда, программа будет пытаться запустить прорисовку в прошлом.
			В этом случае следующий запуск делаем относительно текущего времени */ 
		timer.expires_at( now > time ? now : time );

		if (terminate_)
			return;

		/* Если больше нечего анимировать - засыпаем */
		if (!anim)
		{
			mutex sleep_mutex;
			scoped_lock lock(sleep_mutex);
			anim_cond_.wait(lock);
		}

		timer.wait();
	}
}

/* Анимация карты - запуск потока, если он был остановлен */
void window::animate(void)
{
	anim_cond_.notify_all();
}


/******************************************************************************
* Статическая функция обработки сообщений окна
*/
LRESULT CALLBACK window::static_wndproc_(HWND hwnd, UINT uMsg, WPARAM wParam,
	LPARAM lParam)
{
	/* Ссылку на who::window берём из параметров hwnd */
	window *win = (window*)GetWindowLongPtr(hwnd, GWL_USERDATA);

	/* Первое сообщение */
	if (!win && uMsg == WM_NCCREATE) {
		CREATESTRUCT *cs = (CREATESTRUCT*)lParam;
		win = (window*)cs->lpCreateParams;
		win->hwnd_ = hwnd;
		/* Сохраняем ссылку на who::window в параметрах hwnd */
		SetWindowLongPtr(hwnd, GWL_USERDATA, (LONG_PTR)win);
		return TRUE;
	}

	/* Не должно быть win==NULL */
	assert(win);

	return win->wndproc_(hwnd, uMsg, wParam, lParam);
}

/******************************************************************************
* Функция-член класса обработки сообщений окна
*/
LRESULT window::wndproc_(HWND hwnd, UINT uMsg, WPARAM wParam,
	LPARAM lParam)
{
	switch (uMsg)
	{

		case WM_NCDESTROY:
		{
			SetWindowLongPtr(hwnd_, GWL_USERDATA, NULL);
			hwnd_ = NULL;
			on_destroy();
			break;
		}

		case WM_ERASEBKGND:
			return TRUE;

		/* Обновление внешнего окна (WM_PAINT) - содержимое берёт из буфера.
			Прорисовывает только требуемую часть */
		case WM_PAINT:
		{
			/* Перерисовка окна. paint() не вызывается. Берёт из буфера.
				Прорисовывает только требуемую часть */
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);

			{
				scoped_lock l(canvas_mutex_);

				if (bitmap_.get())
				{
					Gdiplus::Graphics g(hdc);
					g.DrawImage( bitmap_.get(),
						(int)ps.rcPaint.left, (int)ps.rcPaint.top,
						(int)ps.rcPaint.left, (int)ps.rcPaint.top,
						(int)(ps.rcPaint.right - ps.rcPaint.left),
						(int)(ps.rcPaint.bottom - ps.rcPaint.top),
						Gdiplus::UnitPixel );
				}
			}

			EndPaint(hwnd, &ps);
			return 0;
		}

		case WM_WINDOWPOSCHANGED:
			set_size( ((WINDOWPOS*)lParam)->cx, ((WINDOWPOS*)lParam)->cy);
			break;

		case WM_MOUSEWHEEL:
		{
			/* Место под курсором должно остаться на том же месте */
			POINT pt;
			pt.x = GET_X_LPARAM(lParam);
			pt.y = GET_Y_LPARAM(lParam);
			ScreenToClient(hwnd, &pt);

			if (on_mouse_wheel)
				on_mouse_wheel(this, GET_WHEEL_DELTA_WPARAM(wParam) / 120,
						wparam_to_keys_(wParam), pt.x, pt.y );
			break;
		}

		case WM_MOUSEMOVE:
			on_mouse_event_(on_mouse_move, wParam, lParam);
			break;

		case WM_LBUTTONDOWN:
			SetFocus(hwnd);
			on_mouse_event_(on_lbutton_down, wParam, lParam);
			break;

		case WM_LBUTTONUP:
			on_mouse_event_(on_lbutton_up, wParam, lParam);
			break;

		case WM_LBUTTONDBLCLK:
			on_mouse_event_(on_lbutton_dblclk, wParam, lParam);
			break;

		case WM_RBUTTONDOWN:
			SetFocus(hwnd);
			on_mouse_event_(on_rbutton_down, wParam, lParam);
			break;

		case WM_RBUTTONUP:
			on_mouse_event_(on_rbutton_up, wParam, lParam);
			break;

		case WM_RBUTTONDBLCLK:
			on_mouse_event_(on_rbutton_dblclk, wParam, lParam);
			break;

		case WM_MBUTTONDOWN:
			SetFocus(hwnd);
			on_mouse_event_(on_mbutton_down, wParam, lParam);
			break;

		case WM_MBUTTONUP:
			on_mouse_event_(on_mbutton_up, wParam, lParam);
			break;

		case WM_MBUTTONDBLCLK:
			on_mouse_event_(on_mbutton_dblclk, wParam, lParam);
			break;

		case WM_CAPTURECHANGED:
			mouse_cancel();
			break;

		case WM_SETFOCUS:
			/* Отдельная переменная по причине того, что из другого потока
				GetFocus всегда возвращает NULL */
			focused_ = true;
			animate();
			return 0;

		case WM_KILLFOCUS:
			focused_ = false;
			animate();
			return 0;

		case WM_KEYDOWN:
			if (wParam == VK_ESCAPE && GetCapture() == hwnd)
			{
				mouse_cancel();
				SetCapture(NULL);
			}
			
			if (on_keydown)
				on_keydown(this, wParam);
			
			break;

		case WM_KEYUP:
			if (on_keyup)
				on_keyup(this, wParam);		
			break;

		case WM_SETCURSOR: {
			LPCTSTR cursor;
			switch (mouse_mode_)
			{
				case mousemode::none:
				case mousemode::select:
					cursor = IDC_ARROW;
					break;

				case mousemode::capture:
				case mousemode::move:
				case mousemode::edit:
					cursor = IDC_HAND;
					break;
			}

			SetCursor(LoadCursor(NULL, cursor));
			return TRUE;
		}

		case MY_WM_CHECK_STATE:
			do_check_state();
			break;

		case MY_WM_UPDATE:
			animate();
			break;

		/*-
		default:
			if (uMsg != WM_NCHITTEST)
			{
				char buf[100];
				sprintf(buf, "msg=0x%08X", uMsg);
				SetWindowText(g_ParentHwnd, buf);
			}
		-*/
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

/******************************************************************************
* Преобразование Windows-клавиш из мышинных событий в свой собственный формат
*/
int window::wparam_to_keys_(WPARAM wparam)
{
	int keys = 0;
	
	if (wparam & MK_CONTROL)
		keys |= mousekeys::ctrl;
	
	if (wparam & MK_SHIFT)
		keys |= mousekeys::shift;
	
	if (wparam & MK_LBUTTON)
		keys |= mousekeys::lbutton;
	
	if (wparam & MK_RBUTTON)
		keys |= mousekeys::rbutton;
	
	if (wparam & MK_MBUTTON)
		keys |= mousekeys::mbutton;
	
	return keys;
}

/******************************************************************************
* Реакция на событие мыши
*/
void window::on_mouse_event_(
	const boost::function<void (window*, int keys, int x, int y)> &f,
	WPARAM wparam, LPARAM lparam)
{
	if (f)
		f(this, wparam_to_keys_(wparam),
				GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
}

/******************************************************************************
* Добавление карты
*/
void window::add_schemes(xml::wptree &config)
{
	BOOST_FOREACH(xml::wptree::value_type &v, config)
		if (v.first == L"scheme")
		{
			who::scheme *scheme = new who::scheme(server_, &v.second);
			scheme->set_window(this);
			schemes_.push_back(scheme);
		}
}

/******************************************************************************
* Создание окна
*/
void window::set_link(HWND parent)
{
	delete_link();

	RECT rect;
	/* Берём размеры parent'а и, заодно, узнаём - существует ли он вообще */
	if (::GetClientRect( parent, &rect))
	{
		int w = rect.right - rect.left;
		int h = rect.bottom - rect.top;

		/* Создаём на parent'е своё окно.
			Передаём ему ссылку на this. Получим её в WM_NCCREATE.
			HWND окна возъмём там же */
		CreateWindowEx(0, L"who::window", L"window",
			WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_CLIPSIBLINGS,
			0, 0, w, h, parent, NULL, NULL, (LPVOID)this);

		/* Выравниваем все карты относительно нового parent'а */
		BOOST_FOREACH(who::scheme &scheme, schemes_)
			scheme.set_first_activation(true);

		set_size(w, h);
		if (active_scheme_)
			set_active_scheme_(active_scheme_);
	}
}

/******************************************************************************
* Удаление связи
*/
void window::delete_link( void)
{
	if (hwnd_) DestroyWindow(hwnd_);
}

/******************************************************************************
* Реакция на "разрушение" окна. Вызывается из wndproc
*/
void window::on_destroy( void)
{
	//
}

/******************************************************************************
* Перерисовка буфера и сброс его в окно
*/
void window::paint_( void)
{
	scoped_lock l(canvas_mutex_);

	if (canvas_.get())
	{
		/* Рисуем в буфере */
		Gdiplus::SolidBrush brush( focused_ ? bg_color_ : Gdiplus::Color(248, 248, 248) );
		canvas_->FillRectangle(&brush, 0, 0, w_, h_);

		if (active_scheme_)
		{
			/* Выделение - расчёт */
			if (mouse_mode_ == mousemode::select)
			{
				if (mouse_end_x_ > mouse_start_x_)
				{
					select_rect_.X = (float)mouse_start_x_;
					select_rect_.Width = (float)( mouse_end_x_ - mouse_start_x_ );
				}
				else
				{
					select_rect_.X = (float)mouse_end_x_;
					select_rect_.Width = (float)( mouse_start_x_ - mouse_end_x_ );
				}

				if (mouse_end_y_ > mouse_start_y_)
				{
					select_rect_.Y = (float)mouse_start_y_;
					select_rect_.Height = (float)( mouse_end_y_ - mouse_start_y_ );
				}
				else
				{
					select_rect_.Y = (float)mouse_end_y_;
					select_rect_.Height = (float)( mouse_start_y_ - mouse_end_y_ );
				}

				select_parent_->window_to_client(&select_rect_);

				/*TODO: Снимаем временное(!!!) выделение */
				select_parent_->unselect_all();

				if (select_rect_.Width==0 && select_rect_.Height==0)
					select_parent_->tmp_select();
				else
				{
					/* Временное выделение widget'ов */
					int count = 0;
					BOOST_FOREACH(widget &widg, select_parent_->childs())
						if (widg.in_rect(select_rect_))
						{
							widg.tmp_select();
							count++;
						}

					if (count==0)
						select_parent_->tmp_select();
				}
			}

			active_scheme_->paint( canvas_.get() );

#ifdef _DEBUG
#if 0
			POINT pt;
			RECT rt;

			GetCursorPos(&pt);
			GetWindowRect( hwnd_, &rt);

			float x = (float)( pt.x - rt.left );
			float y = (float)( pt.y - rt.top );
			active_scheme_->parent_to_client(&x, &y);

			widget *widg = active_scheme_->hittest(x, y);

			Gdiplus::Font font(L"Tahoma", 10, 0, Gdiplus::UnitPixel);
			Gdiplus::SolidBrush brush( Gdiplus::Color(0, 0, 0) );

			wchar_t buf[200];

			swprintf_s( buf, sizeof(buf) / sizeof(*buf), L"cursor: x=%0.1f, y=%0.1f", x, y);
			canvas_->DrawString( buf, wcslen(buf),
					&font, Gdiplus::PointF(0.0f, 0.0f), &brush);

			swprintf_s( buf, sizeof(buf) / sizeof(*buf), L"object: [%s], childs=%d, parent=[%s]",
					widget_type(widg),
					widg->childs().size(),
					widget_type(widg->parent()));
			canvas_->DrawString( buf, wcslen(buf),
					&font, Gdiplus::PointF(0.0f, 12.0f), &brush);

			swprintf_s( buf, sizeof(buf) / sizeof(*buf), L"mouse_mode: %s",
					mouse_mode_ == mousemode::none ? L"none"
					: mouse_mode_ == mousemode::capture ? L"capture"
					: mouse_mode_ == mousemode::move ? L"move"
					: mouse_mode_ == mousemode::select ? L"select"
					: mouse_mode_ == mousemode::capture ? L"edit" : L"-");
			canvas_->DrawString( buf, wcslen(buf),
					&font, Gdiplus::PointF(0.0f, 24.0f), &brush);

			if (mouse_mode_ == mousemode::select
				|| mouse_mode_ == mousemode::edit)
			{
				swprintf_s( buf, sizeof(buf) / sizeof(*buf), L"selection: select_parent=[%s], x=%0.1f, y=%0.1f, w=%0.1f, h=%0.1f",
					widget_type(select_parent_),
					select_rect_.X, select_rect_.Y,
					select_rect_.Width, select_rect_.Height);
				canvas_->DrawString( buf, wcslen(buf),
					&font, Gdiplus::PointF(0.0f, 36.0f), &brush);
			}
#endif
#endif
		}

		/* Рамка */
		Gdiplus::Color color(145, 167, 180);
		Gdiplus::Pen pen(color, 1);

		canvas_->DrawRectangle(&pen, 0, 0, w_ - 1, h_ - 1);

		if (focused_)
		{
			Gdiplus::Color color1(230, 139, 44);
			Gdiplus::Color color2(255, 199, 60);

			pen.SetColor(color1);
			canvas_->DrawRectangle(&pen, 0, 0, w_ - 1, 2);

			Gdiplus::SolidBrush brush(color2);
			canvas_->FillRectangle(&brush, 1, 1, w_ - 2, 2);
		}

		/* Переносим из буфера на экран */
		Gdiplus::Graphics g(hwnd_, FALSE);
		g.DrawImage(bitmap_.get(), 0, 0);
	}
}

/******************************************************************************
* Установка текущей карты
*/
void window::set_active_scheme_(who::scheme *scheme)
{
	active_scheme_ = scheme;
	if (scheme->first_activation())
		scheme->align( (float)w_, (float)h_ );

	animate();
}

/******************************************************************************
* Установка текущей карты
*/
void window::set_active_scheme(int index)
{
	BOOST_FOREACH(who::scheme &scheme, schemes_)
        if (index-- == 0)
			set_active_scheme_(&scheme);
}

scheme* window::get_scheme(int index)
{
	BOOST_FOREACH(who::scheme &scheme, schemes_)
        if (index-- == 0)
			return &scheme;

	return NULL;
}

/******************************************************************************
* Изменение размера буфера
*/
void window::set_size(int w, int h)
{
	if (w == 0 || h == 0 || w == w_ && h == h_) return;

	{
		scoped_lock l(canvas_mutex_);

		/* Создаём новый */
		Gdiplus::Graphics g(NULL, FALSE);
		bitmap_.reset( new Gdiplus::Bitmap(w, h, &g) );
		canvas_.reset( new Gdiplus::Graphics(bitmap_.get()) );

		w_ = w;
		h_ = h;
	}

	animate();
}

void window::do_check_state(void)
{
	BOOST_FOREACH(who::scheme &scheme, schemes_)
		scheme.do_check_state();
}

/******************************************************************************
* Начало движения мыши
*/
void window::mouse_start(mousemode::t mm, int x, int y, selectmode::t sm)
{
	if (!active_scheme_ || mm == mousemode::none)
		return;

	if (GetCapture() != hwnd_)
		SetCapture(hwnd_);

	mouse_mode_ = mm;
	mouse_start_x_ = x;
	mouse_start_y_ = y;
	mouse_end_x_ = x;
	mouse_end_y_ = y;

	switch (mm)
	{
		/* Ничего не двигаем, только захватываем окно (SetCapture) */
		case mousemode::capture:
			if (mouse_mode_ != mousemode::none)
				return;
			break;

		/* Двигаем карту */
		case mousemode::move:
			SetCursor( LoadCursor(NULL, IDC_HAND) );

			/* Запоминаем нынешние координаты карты, во-первых, для смещения
				относительно них, во-вторых, чтобы вернуть карту на место
				в случае отмены */
			active_scheme_->save_pos();
			break;

		/* Выделяем объекты на карте */
		case mousemode::select:
		case mousemode::edit:
		{
			float fx = (float)x;
			float fy = (float)y;
			active_scheme_->parent_to_client(&fx, &fy);

			select_parent_ = active_scheme_->hittest(fx, fy);

			/* Ограничиваем движения мыши
				Для карты не ограничиваем - при нажатии вне карты курсор
				некрасиво перемещается внутрь */
			if (select_parent_ != active_scheme_)
			{
				Gdiplus::Rect rect =
					rectF_to_rect( select_parent_->rect_in_window() );

				RECT rt = { rect.X, rect.Y,
					rect.X + rect.Width + 1, rect.Y + rect.Height + 1 };

				ClientToScreen(hwnd_, (POINT*)&rt.left);
				ClientToScreen(hwnd_, (POINT*)&rt.right);

				ClipCursor(&rt);
			}

			if (sm == selectmode::normal)
				active_scheme_->unselect_all();

			if (select_parent_ == active_scheme_)
				mouse_mode_ = mousemode::select;

			break;
		}
	}

	animate();
}

/******************************************************************************
* Движение мыши
*/
void window::mouse_move_to(int x, int y)
{
	if (!active_scheme_)
		return;

	switch (mouse_mode_)
	{
		case mousemode::none:
		case mousemode::capture:
			animate();
			break;

		case mousemode::move:
			/* Сдвиг делаем относительно ранее сохранённых координат */
			active_scheme_->move_from_saved( (float)(x - mouse_start_x_),
				(float)(y - mouse_start_y_) );
			animate();
			break;

		case mousemode::select:
		{
			mouse_end_x_ = x;
			mouse_end_y_ = y;
			animate();
			break;
		}

		case mousemode::edit:
			animate();
			break;
	}
}

/******************************************************************************
* Завершение движения
*/
void window::mouse_end(int x, int y)
{
	if (!active_scheme_)
		return;

	mouse_move_to(x, y);

	switch (mouse_mode_)
	{
		case mousemode::none:
		case mousemode::capture:
		case mousemode::move:
			break;

		case mousemode::select:
			if (select_parent_->tmp_selected())
				select_parent_->select();
			else
				BOOST_FOREACH(widget &widg, select_parent_->childs())
					if (widg.tmp_selected())
						widg.select();
			
			select_parent_ = NULL;
			break;

		case mousemode::edit:
			break;
	}

	mouse_mode_ = mousemode::none;
	if (GetCapture() == hwnd_)
		SetCapture(NULL);
	ClipCursor(NULL);

	animate();
}

/******************************************************************************
* Отмена сдвига карты
*/
void window::mouse_cancel(void)
{
	switch (mouse_mode_)
	{
		case mousemode::none:
		case mousemode::capture:
			break;

		case mousemode::move:
			if (active_scheme_)
				active_scheme_->restore_pos();
			break;

		case mousemode::select:
			select_parent_->unselect_all();
			select_parent_ = NULL;
			break;

		case mousemode::edit:
			//if (active_scheme_)
				/*TODO: восстановление позиций выделенных объектов */
				//active_scheme_->restore_pos();
			break;
	}

	mouse_mode_ = mousemode::none;
	::SetCursor( LoadCursor(NULL, IDC_ARROW) );
	if (GetCapture() == hwnd_)
		SetCapture(NULL);
	ClipCursor(NULL);

	animate();
}

/******************************************************************************
* Кто под курсором?
*/
widget* window::hittest(int x, int y)
{
	if (!active_scheme_)
		return NULL;

	float fx = (float)x;
	float fy = (float)y;
	active_scheme_->window_to_client(&fx, &fy);

	return active_scheme_->hittest(fx, fy);
}

/******************************************************************************
* Очистить
*/
void window::clear(void)
{
	active_scheme_ = NULL;
	schemes_.clear();
}

/******************************************************************************
* Изменение масштаба
*/
void window::zoom(float ds)
{
	if (active_scheme_)
	{
		float fx = (float)( w_ / 2 );
		float fy = (float)( h_ / 2 );
		active_scheme_->parent_to_client(&fx, &fy);
		active_scheme_->zoom(ds, fx, fy);
	}
}

}
