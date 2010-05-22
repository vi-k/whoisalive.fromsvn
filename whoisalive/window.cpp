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

const wchar_t* widget_type(ipwidget_t *widget)
{
	return
		dynamic_cast<ipmap_t*>(widget) ? L"scheme" :
		dynamic_cast<ipimage_t*>(widget) ? L"image" :
		dynamic_cast<ipobject_t*>(widget) ? L"object" :
		widget ? L"unknown" : L"null";
}

window::window(server &server, HWND parent)
	: server_(server)
	, timer_(server.io_service(), posix_time::microsec_clock::universal_time())
	, timer_started_(false)
	, hwnd_(NULL)
	, focused_(false)
	//, timer_(NULL)
	, anim_queue_(0)
	, w_(0)
	, h_(0)
	, bg_color_(255,255,255)
	, active_map_(NULL)
	, mouse_mode_(mousemode::none)
	, mouse_start_x_(0)
	, mouse_start_y_(0)
	, mouse_end_x_(0)
	, mouse_end_y_(0)
	, select_parent_(NULL)
	, select_rect_(0.0f, 0.0f, 0.0f, 0.0f)
	, terminate_(false)
	, canvas_mutex_()
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
}

/******************************************************************************
* Деструктор
*/
window::~window()
{
	terminate_ = true;
	
	/* Остановка таймера - он тут же должен запуститься последний раз */
	timer_.cancel();
	while (timer_started_)
		scoped_lock l(anim_mutex_);

	delete_link();

	maps_.clear();
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
	switch (uMsg) {

		case WM_NCDESTROY: {
			SetWindowLongPtr(hwnd_, GWL_USERDATA, NULL);
			hwnd_ = NULL;
			on_destroy();
			break;
		}

		case WM_ERASEBKGND:
			return TRUE;

		/* Обновление внешнего окна (WM_PAINT) - содержимое берёт из буфера.
			Прорисовывает только требуемую часть */
		case WM_PAINT: {
			/* Перерисовка окна. paint() не вызывается. Берёт из буфера.
				Прорисовывает только требуемую часть */
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);

			canvas_lock();

			if (bitmap_.get()) {
				Gdiplus::Graphics g(hdc);
				g.DrawImage( bitmap_.get(),
					(int)ps.rcPaint.left, (int)ps.rcPaint.top,
					(int)ps.rcPaint.left, (int)ps.rcPaint.top,
					(int)(ps.rcPaint.right - ps.rcPaint.left),
					(int)(ps.rcPaint.bottom - ps.rcPaint.top),
					Gdiplus::UnitPixel );
			}

			canvas_unlock();

			EndPaint(hwnd, &ps);
			return 0;
		}

		case WM_WINDOWPOSCHANGED:
			set_size( ((WINDOWPOS*)lParam)->cx, ((WINDOWPOS*)lParam)->cy);
			break;

		case WM_MOUSEWHEEL: {
			/* Место под курсором должно остаться на том же месте */
			POINT pt;
			pt.x = GET_X_LPARAM(lParam);
			pt.y = GET_Y_LPARAM(lParam);
			ScreenToClient(hwnd, &pt);

			if (on_mouse_wheel) {
				on_mouse_wheel(this, GET_WHEEL_DELTA_WPARAM(wParam) / 120,
						wparam_to_keys_(wParam), pt.x, pt.y );
			}
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
			if (wParam == VK_ESCAPE && GetCapture() == hwnd) {
				mouse_cancel();
				SetCapture(NULL);
			}
			if (on_keydown) on_keydown(this, wParam);
			break;

		case WM_KEYUP:
			if (on_keyup) on_keyup(this, wParam);
			break;

		case WM_SETCURSOR: {
			LPCTSTR cursor;
			switch (mouse_mode_) {

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
			if (uMsg != WM_NCHITTEST) {
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
	if (wparam & MK_CONTROL) keys |= mousekeys::ctrl;
	if (wparam & MK_SHIFT)   keys |= mousekeys::shift;
	if (wparam & MK_LBUTTON) keys |= mousekeys::lbutton;
	if (wparam & MK_RBUTTON) keys |= mousekeys::rbutton;
	if (wparam & MK_MBUTTON) keys |= mousekeys::mbutton;
	return keys;
}

/******************************************************************************
* Реакция на событие мыши
*/
void window::on_mouse_event_(
		const boost::function<void (window*, int keys, int x, int y)> &f,
		WPARAM wparam, LPARAM lparam) {
	if (f) {
		f(this, wparam_to_keys_(wparam),
				GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
	}
}

/******************************************************************************
* Добавление карты
*/
void window::add_maps(xml::wptree &maps)
{
	BOOST_FOREACH(xml::wptree::value_type &v, maps)
		if (v.first == L"scheme")
		{
			ipmap_t *map = new ipmap_t(server_, &v.second);
			map->set_window(this);
			maps_.push_back(map);
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
	if (::GetClientRect( parent, &rect)) {
		int w = rect.right - rect.left;
		int h = rect.bottom - rect.top;

		/* Создаём на parent'е своё окно.
			Передаём ему ссылку на this. Получим её в WM_NCCREATE.
			HWND окна возъмём там же */
		CreateWindowEx(0, L"who::window", L"window",
				WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_CLIPSIBLINGS,
				0, 0, w, h, parent, NULL, NULL, (LPVOID)this);

		/* Выравниваем все карты относительно нового parent'а */
		BOOST_FOREACH(ipmap_t &map, maps_) {
			map.set_first_activation(true);
		}

		set_size(w, h);
		if (active_map_) set_active_map_(active_map_);
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

/* Анимация карты - таймер */
void window::handle_timer(void)
{
	scoped_lock l(anim_mutex_);
	
	timer_started_ = false;

	if( terminate_)
		return;

	/* Анимируются все карты */
	bool anim = false;
	BOOST_FOREACH(ipmap_t &map, maps_)
		if (map.animate_calc())
			anim = true;

	/* ... но прорисовывается только одно - активное */
	paint_();

	/* Для отладки */
	#ifdef _DEBUG
	if (active_map_)
	{
		static int count = 0;
		wchar_t buf[200];

		swprintf_s( buf, sizeof(buf) / sizeof(*buf), L"[%08X] %d - %.3f (%d) - %d",
			hwnd_, ++count,
			active_map_->scale(), GetCurrentThreadId(),
			ipmap_t::z(active_map_->scale()) );

		SetWindowText(g_parent_wnd, buf);
	}
	#endif

	/* Если больше нечего анимировать - останавливаемся.
		За время отрисовки другой поток мог запросить
		анимацию, учитываем сей факт */
	boost::posix_time::ptime time = timer_.expires_at() + server_.anim_period();
	boost::posix_time::ptime now = posix_time::microsec_clock::universal_time();

	/* Теоретически время следующей прорисовки должно быть относительным
		от времени предыдущей, но на практике могут возникнуть торможения,
		и, тогда, программа будет пытаться запустить прорисовку в прошлом.
		В этом случае следующий запуск делаем относительно текущего времени */ 
	timer_.expires_at( now > time ? now : time );

	if (anim || InterlockedDecrement( &anim_queue_) != 0)
	{
		timer_.async_wait( boost::bind(&window::handle_timer, this) );
		timer_started_ = true;

		#if 0
		HANDLE timer = window->timer_;
		if (InterlockedDecrement( &window->anim_queue_) == 0) {
			/* Так и не получилось нормально его завершить */
			//if( !window->terminate_)
			DeleteTimerQueueTimer(NULL, timer, NULL);
			return;
		}
		#endif
	}
}

#if 0
VOID CALLBACK window::on_timer(window *window, BOOLEAN TimerOrWaitFired)
{
	if( window->terminate_) return;

	/* Анимируются все карты */
	bool anim = false;
	BOOST_FOREACH(ipmap_t &map, window->maps_) {
		if (map.animate_calc()) anim = true;
	}

	/* ... но прорисовывается только одно - активное */
	if( window->terminate_) return;

	window->paint_();

	/* Для отладки */
	#ifdef _DEBUG
	if (window->active_map_) {
		static int count = 0;
		wchar_t buf[200];

		swprintf_s( buf, sizeof(buf) / sizeof(*buf), L"[%08X] %d - %.3f (%d) - %d",
				window->hwnd_, ++count,
				window->active_map_->scale(), GetCurrentThreadId(),
				ipmap_t::z(window->active_map_->scale()) );

		SetWindowText(g_parent_wnd, buf);
	}
	#endif

	/* Если больше нечего анимировать - останавливаемся */
	if( window->terminate_) return;

	if (!anim) {
		/* За время отрисовки другой поток мог запросить
			анимацию, учитываем сей факт */
		HANDLE timer = window->timer_;
		if (InterlockedDecrement( &window->anim_queue_) == 0) {
			/* Так и не получилось нормально его завершить */
			//if( !window->terminate_)
			DeleteTimerQueueTimer(NULL, timer, NULL);
			return;
		}
	}
}
#endif

/******************************************************************************
* Анимация карты - запуск таймера
*/
void window::animate(void)
{
	scoped_lock l(anim_mutex_);

	if (terminate_)
		return;

	if (!timer_started_)
	{
		/* Даже если таймер был остановлен, перед остановкой
			он устанавливает время следующего запуска. Если же время
			его запуска уже в прошлом, то он запустится сразу */
		timer_.async_wait( boost::bind(&window::handle_timer, this) );
		timer_started_ = true;
	}
}

/******************************************************************************
* Перерисовка буфера и сброс его в окно
*/
void window::paint_( void)
{
	canvas_lock();

	if (canvas_.get()) {
		/* Рисуем в буфере */
		Gdiplus::SolidBrush brush( focused_ ? bg_color_ : Gdiplus::Color(248, 248, 248) );
		canvas_->FillRectangle(&brush, 0, 0, w_, h_);

		if (active_map_) {
			/* Выделение - расчёт */
			if (mouse_mode_ == mousemode::select) {

				if (mouse_end_x_ > mouse_start_x_) {
					select_rect_.X = (float)mouse_start_x_;
					select_rect_.Width = (float)( mouse_end_x_ - mouse_start_x_ );
				}
				else {
					select_rect_.X = (float)mouse_end_x_;
					select_rect_.Width = (float)( mouse_start_x_ - mouse_end_x_ );
				}

				if (mouse_end_y_ > mouse_start_y_) {
					select_rect_.Y = (float)mouse_start_y_;
					select_rect_.Height = (float)( mouse_end_y_ - mouse_start_y_ );
				}
				else {
					select_rect_.Y = (float)mouse_end_y_;
					select_rect_.Height = (float)( mouse_start_y_ - mouse_end_y_ );
				}

				select_parent_->window_to_client(&select_rect_);

				/*TODO: Снимаем временное(!!!) выделение */
				select_parent_->unselect_all();

				if (select_rect_.Width==0 && select_rect_.Height==0) {
					select_parent_->tmp_select();
				}
				else {
					/* Временное выделение widget'ов */
					int count = 0;
					BOOST_FOREACH(ipwidget_t &widget, select_parent_->childs()) {
						if (widget.in_rect(select_rect_)) {
							widget.tmp_select();
							count++;
						}
					}
					if (count==0) select_parent_->tmp_select();
				}
			}

			active_map_->paint( canvas_.get() );

#ifdef _DEBUG
#if 0
			POINT pt;
			RECT rt;

			GetCursorPos(&pt);
			GetWindowRect( hwnd_, &rt);

			float x = (float)( pt.x - rt.left );
			float y = (float)( pt.y - rt.top );
			active_map_->parent_to_client(&x, &y);

			ipwidget_t *widget = active_map_->hittest(x, y);

			Gdiplus::Font font(L"Tahoma", 10, 0, Gdiplus::UnitPixel);
			Gdiplus::SolidBrush brush( Gdiplus::Color(0, 0, 0) );

			wchar_t buf[200];

			swprintf_s( buf, sizeof(buf) / sizeof(*buf), L"cursor: x=%0.1f, y=%0.1f", x, y);
			canvas_->DrawString( buf, wcslen(buf),
					&font, Gdiplus::PointF(0.0f, 0.0f), &brush);

			swprintf_s( buf, sizeof(buf) / sizeof(*buf), L"object: [%s], childs=%d, parent=[%s]",
					widget_type(widget),
					widget->childs().size(),
					widget_type(widget->parent()));
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
					|| mouse_mode_ == mousemode::edit) {
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

		if (focused_) {
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

	canvas_unlock();
}

/******************************************************************************
* Установка текущей карты
*/
void window::set_active_map_(ipmap_t *map)
{
	active_map_ = map;
	if ( map->first_activation() ) {
		map->align( (float)w_, (float)h_ );
	}

	animate();
}

/******************************************************************************
* Установка текущей карты
*/
void window::set_active_map(int index)
{
	BOOST_FOREACH(ipmap_t &map, maps_) {
        if (index-- == 0) set_active_map_(&map);
	}
}

ipmap_t* window::get_map(int index)
{
	BOOST_FOREACH(ipmap_t &map, maps_) {
        if (index-- == 0) return &map;
	}

	return NULL;
}

/******************************************************************************
* Изменение размера буфера
*/
void window::set_size(int w, int h)
{
	if (w == 0 || h == 0 || w == w_ && h == h_) return;

	canvas_lock();

	/* Создаём новый */
	Gdiplus::Graphics g(NULL, FALSE);
	bitmap_.reset( new Gdiplus::Bitmap(w, h, &g) );
	canvas_.reset( new Gdiplus::Graphics(bitmap_.get()) );

	w_ = w;
	h_ = h;

	canvas_unlock();

	animate();
}

void window::do_check_state(void)
{
	BOOST_FOREACH(ipmap_t &map, maps_)
		map.do_check_state();
}

/******************************************************************************
* Начало движения мыши
*/
void window::mouse_start(mousemode::t mm, int x, int y, selectmode::t sm)
{
	if (!active_map_ || mm == mousemode::none) return;

	if (GetCapture() != hwnd_) SetCapture(hwnd_);

	mouse_mode_ = mm;
	mouse_start_x_ = x;
	mouse_start_y_ = y;
	mouse_end_x_ = x;
	mouse_end_y_ = y;

	switch (mm) {
		/* Ничего не двигаем, только захватываем окно (SetCapture) */
		case mousemode::capture:
			if (mouse_mode_ != mousemode::none) return;
			break;

		/* Двигаем карту */
		case mousemode::move:
			SetCursor( LoadCursor(NULL, IDC_HAND) );

			/* Запоминаем нынешние координаты карты, во-первых, для смещения
				относительно них, во-вторых, чтобы вернуть карту на место
				в случае отмены */
			active_map_->save_pos();
			break;

		/* Выделяем объекты на карте */
		case mousemode::select: {
		case mousemode::edit:
			float fx = (float)x;
			float fy = (float)y;
			active_map_->parent_to_client(&fx, &fy);

			select_parent_ = active_map_->hittest(fx, fy);

			/* Ограничиваем движения мыши
				Для карты не ограничиваем - при нажатии вне карты курсор
				некрасиво перемещается внутрь */
			if (select_parent_ != active_map_) {
				Gdiplus::Rect rect =
						rectF_to_rect( select_parent_->rect_in_window() );

				RECT rt = { rect.X, rect.Y,
						rect.X + rect.Width + 1, rect.Y + rect.Height + 1 };

				ClientToScreen(hwnd_, (POINT*)&rt.left);
				ClientToScreen(hwnd_, (POINT*)&rt.right);

				ClipCursor(&rt);
			}

			if (sm == selectmode::normal) {
				active_map_->unselect_all();
			}

			if (select_parent_ == active_map_) mouse_mode_ = mousemode::select;

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
	if (!active_map_) return;

	switch (mouse_mode_) {

		case mousemode::none:
		case mousemode::capture:
			animate();
			break;

		case mousemode::move:
			/* Сдвиг делаем относительно ранее сохранённых координат */
			active_map_->move_from_saved( (float)(x - mouse_start_x_),
                    (float)(y - mouse_start_y_) );
			animate();
			break;

		case mousemode::select: {
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
	if (!active_map_) return;

	mouse_move_to(x, y);

	switch (mouse_mode_) {

		case mousemode::none:
		case mousemode::capture:
		case mousemode::move:
			break;

		case mousemode::select: {
			if (select_parent_->tmp_selected()) select_parent_->select();
			else {
				BOOST_FOREACH(ipwidget_t &widget, select_parent_->childs()) {
					if (widget.tmp_selected()) widget.select();
				}
			}
			select_parent_ = NULL;
			break;
		}

		case mousemode::edit:
			break;
	}

	mouse_mode_ = mousemode::none;
	if (GetCapture() == hwnd_) SetCapture(NULL);
	ClipCursor(NULL);

	animate();
}

/******************************************************************************
* Отмена сдвига карты
*/
void window::mouse_cancel(void)
{
	switch (mouse_mode_) {

		case mousemode::none:
		case mousemode::capture:
			break;

		case mousemode::move:
			if (active_map_) active_map_->restore_pos();
			break;

		case mousemode::select:
			select_parent_->unselect_all();
			select_parent_ = NULL;
			break;

		case mousemode::edit:
			if (active_map_) {
				/*TODO: восстановление позиций выделенных объектов */
				//active_map_->restore_pos();
			}
			break;
	}

	mouse_mode_ = mousemode::none;
	::SetCursor( LoadCursor(NULL, IDC_ARROW) );
	if (GetCapture() == hwnd_) SetCapture(NULL);
	ClipCursor(NULL);

	animate();
}

/******************************************************************************
* Кто под курсором?
*/
ipwidget_t* window::hittest(int x, int y)
{
	if (!active_map_) return NULL;

	float fx = (float)x;
	float fy = (float)y;
	active_map_->window_to_client(&fx, &fy);

	return active_map_->hittest(fx, fy);
}

/******************************************************************************
* Очистить
*/
void window::clear(void)
{
	active_map_ = NULL;
	maps_.clear();
}

/******************************************************************************
* Изменение масштаба
*/
void window::zoom(float ds)
{
	if (active_map_) {
		float fx = (float)( w_ / 2 );
		float fy = (float)( h_ / 2 );
		active_map_->parent_to_client(&fx, &fy);
		active_map_->zoom(ds, fx, fy);
	}
}

}
