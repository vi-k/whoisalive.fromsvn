#include "tiler.h"
#include "server.h"

//#include "log.h"
//extern my::log main_log;

#include <sstream>
using namespace std;

namespace who { namespace tiler {

server::server(who::server &server, size_t max_tiles)
	: server_(server)
	, tiles_(max_tiles)
{
}

void server::run()
{
	while (true)
	{
		tiler::tile_id tile_id;

		/* Ищем первый попавшийся незагруженный тайл */
		{
			scoped_lock lock(server_mutex_);

			for (tiles_list::iterator it = tiles_.begin();
				it != tiles_.end(); it++)
			{
				if (!it->value().get())
				{
					tile_id = it->key();
					break;
				}
			}

			/* Если нет такого - засыпаем */
			if (!tile_id)
    		{
    			cond_.wait(lock);
				continue;
			}
		}

		/* Загружаем тайл с диска */
		wstringstream filename;
		tiler::map &map = maps_[ tile_id.map_id ];

		filename << L"maps/" << map.id
			<< L"/z" << tile_id.z
			<< L"/" << (tile_id.x >> 10)
			<< L"/x" << tile_id.x
			<< L"/" << (tile_id.y >> 10)
			<< L"/y" << tile_id.y
			<< L"." << map.ext;

		tiler::tile_ptr ptr( new tile(filename.str()) );

		/* Если загрузить с диска не удалось - загружаем с сервера */
		if (!ptr->loaded)
		{
			wstringstream request;
			request << L"/maps/gettile?map=" << map.id
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
			scoped_lock lock(server_mutex_);

			tiles_list::iterator it = tiles_.find(tile_id);
			if (it != tiles_.end())
				it->value() = ptr;
		}

		if (on_update)
			on_update();

	} /* while (true) */
}

void server::start()
{
	cond_.notify_all();
}

int server::add_map(const tiler::map &map)
{
	scoped_lock l(server_mutex_);
	int id = get_new_map_id_();
	maps_[id] = map;
	return id;
}

tile_ptr server::get_tile(int map_id, int z, int x, int y)
{
	scoped_lock l(server_mutex_);
	
	tile_ptr ptr = tiles_[ tile_id(map_id, z, x, y) ];
	start();
	return ptr;
}

} }
