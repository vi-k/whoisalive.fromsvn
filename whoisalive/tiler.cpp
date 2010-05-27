#include "tiler.h"
#include "server.h"

//#include "log.h"
//extern my::log main_log;

#include <sstream>
using namespace std;

#include <boost/bind.hpp>

namespace who { namespace tiler {

#pragma warning(disable:4355) /* 'this' : used in base member initializer list */

server::server(who::server &server, size_t max_tiles, boost::function<void ()> on_update_proc)
	: terminate_(false)
	, server_(server)
	, tiles_(max_tiles)
	, on_update_(on_update_proc)
{
	/* Запускаем после инициализации */
	thread_ = boost::thread( boost::bind(&server::thread_proc, this) );
}

server::~server()
{
	terminate_ = true;
	wake_up(); /* Будим поток, чтобы он завершил работу */
	thread_.join(); /* Ждём его завершения */
}

void server::thread_proc()
{
	while (!terminate_)
	{
		tiler::tile_id tile_id;

		/* Ищем первый попавшийся незагруженный тайл */
		{
			s_shared_lock l(tiles_mutex_);
			int map_id = server_.active_map_id();

			for (tiles_list::iterator it = tiles_.begin();
				it != tiles_.end(); it++)
			{
				if (!it->value() && it->key().map_id == map_id)
				{
					tile_id = it->key();
					break;
				}
			}
		}

		/* Если нет такого - засыпаем */
		if (!tile_id)
    	{
    		mutex proc_mutex;
			scoped_lock lock(proc_mutex);
			cond_.wait(lock);
			continue;
		}

		/* Загружаем тайл с диска */
		wstringstream filename;
		tiler::map *map;

		{
			scoped_lock l(maps_mutex_);
			map = &maps_[ tile_id.map_id ];
		}

		filename << L"maps/" << map->id
			<< L"/z" << tile_id.z
			<< L"/" << (tile_id.x >> 10)
			<< L"/x" << tile_id.x
			<< L"/" << (tile_id.y >> 10)
			<< L"/y" << tile_id.y
			<< L"." << map->ext;

		tiler::tile::ptr ptr( new tile(filename.str()) );

		/* Если загрузить с диска не удалось - загружаем с сервера */
		if (!ptr->loaded)
		{
			wstringstream request;
			request << L"/maps/gettile?map=" << map->id
				<< L"&z=" << tile_id.z
				<< L"&x=" << tile_id.x
				<< L"&y=" << tile_id.y;
			
			try
			{
				server_.load_file(request.str(), filename.str());
				ptr.reset( new tile(filename.str()) );
			}
			catch (...)
			{
				//
			}
		}

        /* Вносим изменения в список загруженных тайлов */
		{
			s_shared_lock l(tiles_mutex_);

			tiles_list::iterator it = tiles_.find(tile_id);
			if (it != tiles_.end())
				it->value() = ptr;
		}

		if (on_update_)
			on_update_();

	} /* while (true) */
}

int server::add_map(const tiler::map &map)
{
	scoped_lock l(maps_mutex_);
	int id = get_new_map_id_();
	maps_[id] = map;
	return id;
}

tile::ptr server::get_tile(int map_id, int z, int x, int y)
{
	tile_id id(map_id, z, x, y);

	{
		s_shared_lock l(tiles_mutex_);

		tiles_list::iterator it = tiles_.find(id);
		if (it != tiles_.end())
			return it->value();
	}

	{
		s_unique_lock l(tiles_mutex_);
	
		tile::ptr ptr = tiles_[id];
		wake_up();
		return ptr;
	}
}

} }
