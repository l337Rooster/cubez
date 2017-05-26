/**
* Author: Samuel Rohde (rohde.samuel@gmail.com)
*
* This file is subject to the terms and conditions defined in
* file 'LICENSE.txt', which is part of this source code package.
*/

#ifndef TABLE__H
#define TABLE__H

#ifdef __COMPILE_AS_WINDOWS__
#define _ENABLE_ATOMIC_ALIGNMENT_FIX
#endif

#include <iostream>

#include <vector>
#include <map>

#include "cubez.h"
#include "common.h"

namespace cubez
{

template<typename Key_, typename Value_>
struct BaseElement {
  IndexedBy indexed_by;
  union {
    Offset offset;
    Handle handle;
    Key_ key;
  };
  Value_ value;
};

template <typename Key_, typename Value_, typename Allocator_ = std::allocator<Value_>>
class Table {
public:
  typedef Key_ Key;
  typedef Value_ Value;

  typedef BaseElement<Key, Value> Element;

  typedef std::vector<Key> Keys;
  typedef std::vector<Value> Values;

  // For fast lookup if you have the handle to an entity.
  typedef std::vector<uint64_t> Handles;
  typedef std::vector<Handle> FreeHandles;

  // For fast lookup by Entity Id.
  typedef std::map<Key, Handle> Index;

  Table() {}

  Table(std::vector<std::tuple<Key, Value>>&& init_data) {
    for (auto& t : init_data) {
      insert(std::move(std::get<0>(t)), std::move(std::get<1>(t)));
    }
  }

  Handle insert(Key&& key, Value&& value) {
    Handle handle = make_handle();

    index_[key] = handle;

    values.push_back(std::move(value));
    keys.push_back(key);
    return handle;
  }

  Handle insert(Key&& key, const Value& value) {
    Handle handle = make_handle();

    index_[key] = handle;

    values.push_back(value);
    keys.push_back(key);
    return handle;
  }

  Handle insert(const Key& key, const Value& value) {
    Handle handle = make_handle();

    index_[key] = handle;

    values.push_back(value);
    keys.push_back(key);
    return handle;
  }

  Handle insert(const Key& key, Value&& value) {
    Handle handle = make_handle();

    index_[key] = handle;

    values.push_back(std::move(value));
    keys.push_back(key);
    return handle;
  }

  Value& operator[](Handle handle) {
    return values[handles_[handle]];
  }

  const Value& operator[](Handle handle) const {
    return values[handles_[handle]];
  }

  Handle find(Key&& key) const {
    typename Index::const_iterator it = index_.find(key);
    if (it != index_.end()) {
      return it->second;
    }
    return -1;
  }

  Handle find(const Key& key) const {
    typename Index::const_iterator it = index_.find(key);
    if (it != index_.end()) {
      return it->second;
    }
    return -1;
  }

  inline Key& key(uint64_t index) {
    return keys[index];
  }

  inline const Key& key(uint64_t index) const {
    return keys[index];
  }

  inline Value& value(uint64_t index) {
    return values[index];
  }

  inline const Value& value(uint64_t index) const {
    return values[index];
  }

  uint64_t size() const {
    return values.size();
  }

  int64_t remove(Handle handle) {
    Handle h_from = handle;
    uint64_t& i_from = handles_[h_from];
    Key& k_from = keys[i_from];

    Key& k_to = keys.back();
    Handle h_to = index_[k_to];
    uint64_t& i_to = handles_[h_to];

    Value& c_from = values[i_from];
    Value& c_to = values[i_to];

    release_handle(h_from);

    index_.erase(k_from);
    std::swap(i_from, i_to);
    std::swap(k_from, k_to); keys.pop_back();
    std::swap(c_from, c_to); values.pop_back();

    return 0;
  }

  static void* default_accessor(Collection* c, IndexedBy indexed_by,
                                const void* index) {
    Table* t = (Table*)c->collection;
    if (indexed_by == IndexedBy::KEY) {
      const Key_* key = (const Key_*)index;
      return &(*t)[t->find(*key)];
    } else if (indexed_by == IndexedBy::HANDLE) {
      const Handle* handle = (const Handle*)index;
      return &(*t)[*handle];
    } else if (indexed_by == IndexedBy::OFFSET) {
      return &t->value(*(Offset*)index);
    }
    return nullptr;
  }

  static void default_copy(const uint8_t* /* key */,
                           const uint8_t* value,
                           uint64_t offset,
                           cubez::Frame* f) {
    cubez::Mutation* mutation = &f->mutation;
    mutation->mutate_by = cubez::MutateBy::UPDATE;
    Element* el = (Element*)(mutation->element);
    el->offset = offset;
    new (&el->value) Value(*(Value*)(value) );
  }

  static void default_mutate(cubez::Collection* c,
                             const cubez::Mutation* m) {
      Table* t = (Table*)c->collection;
      Element* el = (Element*)(m->element);
      if (m->mutate_by == cubez::MutateBy::UPDATE) {
        t->values[el->offset] = std::move(el->value); 
      } else if (m->mutate_by == cubez::MutateBy::INSERT) {
        t->insert(std::move(el->key), std::move(el->value));
      }
  }

  static uint64_t default_count(cubez::Collection* c) {
    return ((Table*)c->collection)->size();
  }

  static uint8_t* default_keys(cubez::Collection* c) {
    return (uint8_t*)((Table*)c->collection)->keys.data();
  };

  static uint8_t* default_values(cubez::Collection* c) {
    return (uint8_t*)((Table*)c->collection)->values.data();
  };

  Keys keys;
  Values values;

private:
  Handle make_handle() {
    if (free_handles_.size()) {
      Handle h = free_handles_.back();
      free_handles_.pop_back();
      return h;
    }
    handles_.push_back(handles_.size());
    return handles_.back();
  }

  void release_handle(Handle h) {
    free_handles_.push_back(h);
  }

  Handles handles_;
  FreeHandles free_handles_;
  Index index_;
};

template <typename Table_>
class View {
public:
  typedef Table_ Table;
  typedef BaseElement<typename Table::Key, const typename Table::Value&> Element;

  View(Table* table) : table_(table) {}

  inline const typename Table::Value& operator[](Handle handle) const {
    return table_->operator[](handle);
  }

  inline Handle find(typename Table::Key&& key) const {
    return table_->find(std::move(key));
  }

  inline Handle find(const typename Table::Key& key) const {
    return table_->find(key);
  }

  inline const typename Table::Key& key(uint64_t index) const {
    return table_->keys[index];
  }

  inline const typename Table::Value& value(uint64_t index) const {
    return table_->values[index];
  }

  inline uint64_t size() const {
    return table_->values.size();
  }
private:
  Table* table_;
};

}  // namespace cubez

#endif
