﻿#ifndef WHO_TILER_H
#define WHO_TILER_H

#include "ipgui.h"

#include "../common/my_thread.h"
#include "../common/my_inet.h"
#include "../common/my_mru.h"

#include <string>
#include <list>
#include <map>

#include <boost/functional/hash.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

namespace who {

class server;

namespace tiler {

struct map
{
	enum projection_t {wgs84, spheroid};

	std::wstring id;
	std::wstring name;
	bool is_layer;
	std::wstring tile_type;
	std::wstring ext;
	projection_t projection;
};

struct tile_id
{
	int map_id;
	int z;
	int x;
	int y;

	tile_id()
		: map_id(0), z(0), x(0), y(0) {}

	tile_id(int map_id, int z, int x, int y)
		: map_id(map_id), z(z), x(x), y(y) {}

	tile_id(const tile_id &other)
		: map_id(other.map_id)
		, z(other.z)
		, x(other.x)
		, y(other.y) {}

	inline bool operator !() const
	{
		return map_id == 0
			&& z == 0
			&& x == 0
			&& y == 0;
	}

	inline bool operator ==(const tile_id &other) const
	{
		return map_id == other.map_id
			&& z == other.z
			&& x == other.x
			&& y == other.y;
	}

	friend std::size_t hash_value(const tile_id &t)
	{
		std::size_t seed = 0;
		boost::hash_combine(seed, t.map_id);
		boost::hash_combine(seed, t.z);
		boost::hash_combine(seed, t.x);
		boost::hash_combine(seed, t.y);

		return seed;
	}
};

struct tile
{
	typedef boost::shared_ptr<tile> ptr;

	bool loaded;
	Gdiplus::Image image;

	tile(const std::wstring &filename)
		: loaded(false)
		, image(filename.c_str())
	{
		if (image.GetWidth() && image.GetHeight())
			loaded = true;
	}
};

/* Tiler-сервер */
class server
{
private:
	typedef std::map<int, map> maps_list;
	typedef my::mru::list<tile_id, tile::ptr> tiles_list;

	bool terminate_;
	boost::thread thread_;
	who::server &server_;
	maps_list maps_;
	tiles_list tiles_;
	s_mutex tiles_mutex_; /* shared_mutex */
	mutex maps_mutex_;
	condition_variable cond_;
	boost::function<void ()> on_update_;

	static int get_new_map_id_()
	{
		static int id = 0;
		return ++id;
	}
	
	void thread_proc();

public:
	server(who::server &server, size_t max_tiles, boost::function<void ()> on_update_proc);
	~server();

	inline void wake_up()
		{ cond_.notify_all(); }

	int add_map(const tiler::map &map);
	tile::ptr get_tile(int map_id, int z, int x, int y);
};

} }

#endif
