#ifndef MY_MRU_H
#define MY_MRU_H

/*
	MRU-list собственного производства.

	Создан на основе:
		boost::unordered_map (далее map) - для быстрого доступа к значению
		по уникальному ключу (key). Соответственно, для класса Key
		должна быть определена функция hash_value (см. boost::hash).
		
		std::list (далее list) - для хранения значений в порядке последнего
		использования.

	Суть:
		Map строится по ключу (key) и хранит итераторы list'а (надеюсь,
		на свой страх и риск, что они не меняются в результате
		добавления/удаления элементов) и больше ничего.

		Элементы list'а хранят указатели на mru::list (об этом далее),
		ключи (key) и значения (value).

		Mru::list, соответственно, хранит и map, и list, и предоставляет
		интерфейс для работы с последним. Все функции, возвращающие
		итератор, возвращают итератор именно list'а. Map для пользователя
		не доступен. Mru::list, также, хранит ограничитель на кол-во
		элементов в списке (max_items), но сам его никак не использует.
		
		За добавление/удаление/ограничение элементов в списке отвечает (!)
		элемент list'а - его конструктор и деструктор. Это, с одной стороны,
		вынуждает хранить в каждом из них указатель на mru::list. Но,
		с другой стороны, это значительно упрощает удаление/добавление
		/перемещение элементов.
*/

#define MRU_DEBUG 0

#if MRU_DEBUG
#include <sstream>
#include <iostream>
#define SS_EXCEPTION(s) throw std::exception(((stringstream*)&(s))->str().c_str())
#endif

#include <exception>

#include <list>
#include <boost/unordered_map.hpp>

namespace my { namespace mru {

template <typename Key, typename Value>
class list
{
private:
    class item;
public:
	typedef std::list<item> list_t;
	typedef typename list_t::iterator iterator;
	typedef typename list_t::const_iterator const_iterator;
private:
	typedef boost::unordered_map<Key, typename list_t::iterator> map_t;
	typedef typename map_t::iterator map_iterator;
	typedef typename map_t::const_iterator map_const_iterator;

private:

	class item
	{
	#if MRU_DEBUG
	friend class list;
	#endif
	private:
		list &mru_;
		Key key_;
		Value value_;
		bool mapped_;

		item(list &mru, const Key &key, const Value &value)
			: mru_(mru)
			, key_(key)
			, value_(value)
			, mapped_(false) {}

	public:
		item(const item &other)
			: mru_(other.mru_)
			, key_(other.key_)
			, value_(other.value_)
			, mapped_(false)
		{
			if (other.mapped_)
				throw std::exception("other.mapped_=true");
		}

		~item()
		{
			if (mapped_)
				mru_.map_.erase(key_);
		}

		/* Создание элемента */
		static iterator create(list &mru, const Key &key, const Value &value)
		{
		    /* Ищем ключ */
		    map_t::iterator map_iter = mru.map_.find(key);

			/* Если уже есть - удаляем */
			if (map_iter != mru.map_.end())
				mru.list_.erase(map_iter->second);

	        /* Добавляем в list */
			iterator list_iter = mru.list_.insert(
				mru.list_.begin(), item(mru, key, value) );

	        /* Добавляем в map */
			std::pair<map_t::iterator, bool> p
				= mru.map_.insert(map_t::value_type(key, list_iter));

    	    list_iter->mapped_ = true;

			/* Удаляем лишние */
			if (mru.list_.size() > mru.max_items_)
				mru.list_.pop_back();

    	    return list_iter;
		}

		inline const Key& key() const
			{ return key_; }

		inline Value& value()
			{ return value_; }

		inline const Value& value() const
			{ return value_; }
	};

	map_t map_;
	list_t list_;
	list_t tmp_list_; /* временный list для перемещений
		- так работает гораздо быстрее */
	size_t max_items_;

	inline void up__(iterator *p_list_iter)
	{
		tmp_list_.splice(tmp_list_.begin(), list_, *p_list_iter);
		list_.splice(list_.begin(), tmp_list_);
		*p_list_iter = list_.begin();
	}

public:
	list(size_t max_items)
		: max_items_(max_items) {}

    /* Добавление нового элемента - всегда наверх */
	inline iterator insert(Key const& key, Value const& value)
	{
		return item::create(*this, key, value);
	}

    /* Поиск по ключу */
	inline iterator find(Key const& key)
	{
		map_t::iterator map_iter = map_.find(key);
		return (map_iter == map_.end() ? list_.end() : map_iter->second);
	}

    /* Удаление по ключу */
	void remove(Key const& key)
	{
		map_t::iterator map_iter = map_.find(key);
		if (map_iter != map_.end())
			list_.erase(map_iter->second);
	}

	/* Поднять наверх */
	iterator up(Key const& key)
	{
		map_t::iterator map_iter = map_.find(key);
		if (map_iter == map_.end())
			return list_.end();

		up__(&map_iter->second);

		return map_iter->second;
	}

    /*
    	Доступ по ключу.
    	
    	Если ключ отсутствует, он будет создан - у класса Value
    	должен быть определён конструктор по умолчанию.

		Если ключ уже есть, он будет поднят наверх.
    */
	Value& operator[](Key const& key)
	{
		map_t::iterator map_iter = map_.find(key);
		iterator list_iter;

		/* Т.к. обязательно нужно вернуть результат,
			то, если значение ещё не существует, добавляем его.
			Для этого для Value должен быть определён конструктор
			по умолчанию! */
		if (map_iter == map_.end())
			list_iter = item::create(*this, key, Value());
		else
		{
			up__(&map_iter->second);
			list_iter = map_iter->second;
		}

		return list_iter->value();
	}

	#if MRU_DEBUG
	void test()
	{
		stringstream out;

		/* Сверяем размеры list'а и map'а */
		if (list_.size() != map_.size())
			SS_EXCEPTION( out
				<< "list.size(" << list_.size()
				<< ") != map.size(" << map_.size() << ")");

		/* Проверяем итераторы list'а, чтобы им всем было
			соответствие в map */
		for (iterator list_iter = list_.begin();
			list_iter != list_.end(); list_iter++)
		{
			map_t::iterator map_iter = map_.find(list_iter->key_);
			if (map_iter == map_.end())
				SS_EXCEPTION( out
					<< "not found map_iter("
					<< list_iter->key_ << ")");

			/* Сверяем итератор list'а с итератором,
				хранящимся в map */
			if (map_iter->second != list_iter)
				SS_EXCEPTION( out
					<< std::hex
					<< "map_iter(" << map_iter->first
						<< ")->list_iter(0x" << &*map_iter->second 
					<< ") != list_iter(0x" << &*list_iter << ")");

			/* Проверяем, что map_iter "живой" */
			bool found = false;
			for (map_t::iterator iter = map_.begin();
				iter != map_.end(); iter++)
			{
				if (map_iter == iter)
					found = true;
			}

			if (!found)
				SS_EXCEPTION( out
					<< "map_iter(" << map_iter->first
						<< ") not found in map");
		}

		for (map_t::iterator map_iter = map_.begin();
			map_iter != map_.end(); map_iter++)
		{
			bool found = false;

			for (iterator iter = list_.begin();
				iter != list_.end(); iter++)
			{
				if (map_iter->second == iter)
					found = true;
			}

			if (!found)
				SS_EXCEPTION( out
					<< "list_iter(" << map_iter->first
					<< ") not found in list");
		}
	}
	#endif

	inline void clear()
		{ list_.clear(); }

    inline list_t* operator ->()
    	{ return &list_; }

    inline const list_t& operator ->() const
    	{ return list_; }

    inline list_t& get()
    	{ return list_; }

    inline const list_t& get() const
    	{ return list_; }

	inline iterator begin()
		{ return list_.begin(); }

	inline const_iterator begin() const
		{ return list_.begin(); }
	
	inline iterator end()
		{ return list_.end(); }
	
	inline const_iterator end() const
		{ return list_.end(); }
};

} }

#endif
