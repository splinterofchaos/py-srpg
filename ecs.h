#pragma once

#include <vector>
#include <algorithm>
#include <tuple>
#include <iostream>

// Note: One source of knowledge:
// https://www.gamasutra.com/blogs/TobiasStein/20171122/310172/The_EntityComponentSystem__An_awesome_gamedesign_pattern_in_C_Part_1.php

enum class EcsError {
    OK,
    ALREADY_EXISTS,
    NOT_FOUND
};

// In this ECS, all objects are uniquely identified by an integer.
struct EntityId {
  static constexpr unsigned int NOT_AN_ID = 0;
  unsigned int id = NOT_AN_ID;
};

constexpr bool operator < (EntityId a, EntityId b) { return a.id < b.id; }
constexpr bool operator == (EntityId a, EntityId b) { return a.id == b.id; }
constexpr bool operator != (EntityId a, EntityId b) { return a.id != b.id; }

// Each component is stored with a pointer to its entity and some data.
template<typename T>
struct ComponentData {
    EntityId id;
    T data;

    ComponentData(EntityId id, T data)
        : id(id), data(std::move(data)) { }
};

// Each series of components is also manually sorted.
template<typename T>
using Store = std::vector<ComponentData<T>>;


// A range abstraction that allows multiple component data series to be iterated
// lazily over in a range-based for loop.
template<typename...T>
class ConstComponentRange {
public:
    using StoreTuple = std::tuple<const Store<T> &...>;

  private:
    // Note: Explicitly storing const references to the stores prevents this
    // struct from serving  clients that need mutation, however non-const
    // references would do the opposite.
    //
    // Is there a way of maintaining const correctness without code duplication?
    StoreTuple stores_;

protected:
    template<typename U> const Store<U>& get() const {
        return std::get<const Store<U>&>(stores_);
    }

    struct ConstIterator {
        // To iterate through these stores, we'll need to remember where we came
        // from, though mostly just so we know where the end is.
        const ConstComponentRange& range;

        using iterator_tuple = std::tuple<typename Store<T>::const_iterator...>;
        iterator_tuple its;

        using const_reference = std::tuple<EntityId, const T &...>;

        // To know if all iterators are pointing to the same entity, we'll neet
        // to remember what entity that is.
        EntityId max_id;

        // Make sure getting the U'th iterator is easy.
        template<typename U> auto& get() const {
            return std::get<typename Store<U>::const_iterator>(its);
        }
        template<typename U>
        auto& get() { return std::get<typename Store<U>::const_iterator>(its); }

        bool any_at_end() const {
            return ((get<T>() == range.get<T>().end()) || ...);
        }

        bool all_same() const { return ((get<T>()->id == max_id) && ...); }

        // A helper to catch up iterators that are behind.
        template<typename U>
        EntityId increment_if_lower(typename Store<U>::const_iterator& it,
            EntityId id) const {
            if (it != range.get<U>().end() && it->id < id) it++;
            return it == range.get<U>().end() ? id : it->id;
        }

        template<typename U>
        void increment_if_not_at_end(typename Store<U>::const_iterator& it) {
          if (it != range.get<U>().end()) ++it;
        }

        // After iteration, or on initialization, increment any iterators that
        // are behind.
        void catch_up() {
          if (!any_at_end()) max_id = std::max({get<T>()->id...});
          while (!any_at_end() && !all_same()) {
              max_id = std::max({increment_if_lower<T>(get<T>(), max_id)...});
          }
          if (any_at_end()) {
            its = {get<T>() = range.get<T>().end()...};
          }
        }


        ConstIterator& operator++() {
          // Increment at least once.
          if (!any_at_end()) {
            (increment_if_not_at_end<T>(get<T>()), ...);
          }
          // And then until all iterators point to the same entity.
          catch_up();

          return *this;
        }

        ConstIterator operator++(int) {
            ConstIterator old = *this;
            ++(*this);
            return old;
        }


        ConstIterator(const ConstComponentRange& range, iterator_tuple its)
            : range(range), its(std::move(its)) {
              catch_up();  // Ensure a valid initial starting position.
        }

        bool operator==(const ConstIterator& other) {
          return any_at_end() && other.any_at_end() ||
              ((get<T>() == other.get<T>()) && ...);
        }
        bool operator!=(const ConstIterator& other) {
            return !(*this == other);
        }

        const_reference operator*() const {
            return std::forward_as_tuple(max_id, get<T>()->data...);
        }
    };

public:
    explicit ConstComponentRange(std::tuple<const Store<T> &...> stores)
        : stores_(stores) { }

    ConstIterator begin() const {
        return ConstIterator(*this, { get<T>().begin()... });
    }

    ConstIterator end() const {
        return ConstIterator(*this, { get<T>().end()... });
    }
};

// NOTE: Ideally, this should share definitions with the above const version.
// For now, it is copy/pasted.
// A range abstraction that allows multiple component data series to be iterated
// lazily over in a range-based for loop.
template<typename...T>
class ComponentRange {
    std::tuple<Store<T>&...> stores_;

protected:
    template<typename U> Store<U>& get() const {
        return std::get<Store<U>&>(stores_);
    }

    struct Iterator {
        // To iterate through these stores, we'll need to remember where we came
        // from, though mostly just so we know where the end is.
        const ComponentRange& range;

        using iterator_tuple = std::tuple<typename Store<T>::iterator...>;
        iterator_tuple its;

        using reference = std::tuple<EntityId, T&...>;

        // To know if all iterators are pointing to the same entity, we'll neet
        // to remember what entity that is.
        EntityId max_id;

        // Make sure getting the U'th iterator is easy.
        template<typename U> auto& get() const {
            return std::get<typename Store<U>::iterator>(its);
        }
        template<typename U>
        auto& get() { return std::get<typename Store<U>::iterator>(its); }

        bool any_at_end() const {
            return ((get<T>() >= range.get<T>().end()) || ...);
        }

        bool all_same() const { return ((get<T>()->id == max_id) && ...); }

        // A helper to catch up iterators that are behind.
        template<typename U>
        EntityId increment_if_lower(typename Store<U>::iterator& it,
                                  EntityId id) const {
            if (it != range.get<U>().end() && it->id < id) it++;
            return it == range.get<U>().end() ? id : it->id;
        }

        template<typename U>
        void increment_if_not_at_end(typename Store<U>::iterator& it) {
          if (it != range.get<U>().end()) ++it;
        }

        // After iteration, or on initialization, increment any iterators that
        // are behind.
        void catch_up() {
          if (!any_at_end()) max_id = std::max({get<T>()->id...});
          while (!any_at_end() && !all_same()) {
              max_id = std::max({increment_if_lower<T>(get<T>(), max_id)...});
          }
          if (any_at_end()) {
            its = {get<T>() = range.get<T>().end()...};
          }
        }

        Iterator& operator++() {
            // Increment at least once.
            if (!any_at_end()) {
              (increment_if_not_at_end<T>(get<T>()), ...);
            }
            // And then until all iterators point to the same entity.
            catch_up();

            return *this;
        }

        Iterator operator++(int) {
            Iterator old = *this;
            ++(*this);
            return old;
        }

        Iterator(ComponentRange& range, iterator_tuple its)
            : range(range), its(std::move(its)) {
          catch_up();  // Ensure a valid initial starting position.
        }

        bool operator==(const Iterator& other) {
          return any_at_end() && other.any_at_end() ||
              ((get<T>() == other.get<T>()) && ...);
        }
        bool operator!=(const Iterator& other) {
            return !(*this == other);
        }

        reference operator*() const {
            return std::forward_as_tuple(max_id, get<T>()->data...);
        }
    };

public:
    using StoreTuple = std::tuple<Store<T>&...>;

    explicit ComponentRange(StoreTuple stores)
        : stores_(stores) { }

    Iterator begin() {
        return Iterator(*this, {get<T>().begin()...});
    }

    Iterator end() {
        return Iterator(*this, {get<T>().end()...});
    }
};

// A single object manages components of all types. The entities themselves
// store no data. No two components may be of the same type.
template<typename...Components>
class EntityComponentSystem {
    // The series of ID's will be contiguously stored (frequently iterated
    // through) and manually sorted.
    std::vector<EntityId> entity_ids_;

    // We assign the ID's to ensure their uniqueness.
    EntityId next_id_ = { 1 };

    // And so each series of components may be stored by their type.
    std::tuple<Store<Components>...> components_;

    template<typename T>
    const Store<T>& get_store() const { return std::get<Store<T>>(components_); }
    template<typename T>
    Store<T>& get_store() { return std::get<Store<T>>(components_); }

    template<typename T>
    using FindResultConst = std::pair<bool, typename Store<T>::const_iterator>;

    template<typename T>
    using FindResult = std::pair<bool, typename Store<T>::iterator>;

    template<typename T>
    FindResultConst<T> find_component(EntityId id) const {
        const Store<T>& series = get_store<T>();
        auto pred =
            [](const ComponentData<T>& c, EntityId id) { return c.id < id; };
        auto it = std::lower_bound(series.begin(), series.end(), id, pred);
        return std::make_pair(it != series.end() && it->id == id, it);
    }

    template<typename T>
    FindResult<T> find_component(EntityId id) {
        Store<T>& series = get_store<T>();
        auto pred =
            [](const ComponentData<T>& c, EntityId id) { return c.id < id; };
        auto it = std::lower_bound(series.begin(), series.end(), id, pred);
        return std::make_pair(it != series.end() && it->id == id, it);
    }

    template<typename Iterator, typename T>
    void emplace_at(EntityId id, Iterator it, T data) {
        get_store<T>().emplace(it, id, std::move(data));
    }

    const EntityComponentSystem<Components...>* const_this() const {
        return this;
    }

public:
    EntityComponentSystem() = default;

    EntityId new_entity() {
        entity_ids_.push_back(next_id_);
        next_id_.id++;
        return entity_ids_.back();
    }

    enum WriteAction { CREATE_ENTRY, CREATE_OR_UPDATE };

    // Adds data to a component, although that the entity exists is taken for
    // granted.
    template<typename T>
    EcsError write(EntityId id, T data,
                   WriteAction action = WriteAction::CREATE_ENTRY) {
        auto [found, insert] = find_component<T>(id);
        if (found && insert->id == id && action == WriteAction::CREATE_ENTRY) {
          return EcsError::ALREADY_EXISTS;
        }
        emplace_at(id, insert, std::move(data));
        return EcsError::OK;
    }

    // Adds data to a component, although that the entity exists is taken for
    // granted.
    template<typename T, typename U, typename...V>
    EcsError write(EntityId id, T data, U next, V...rest) {
        EcsError e = write(id, std::move(data));
        return e != EcsError::OK ?
            e : write(id, std::move(next), std::move(rest)...);
    }

    template<typename...T>
    EntityId write_new_entity(T...components) {
        EntityId id = new_entity();
        write(id, std::move(components)...);
        return id;
    }

    template<typename T>
    EcsError read(EntityId id, const T** out) const {
        auto [found, it] = find_component<T>(id);
        if (!found) return EcsError::NOT_FOUND;
        *out = &it->data;
        return EcsError::OK;
    }

    template<typename T>
    void read_or_panic(EntityId id, const T** out) const {
      *out = &read_or_panic<T>(id);
    }

    // Unsafe version of read that ignores NOT_FOUND errors.
    template<typename T>
    const T& read_or_panic(EntityId id) const {
        auto [found, it] = find_component<T>(id);
        if (!found) {
          std::cerr << "Exiting because of entity not found." << std::endl;
          *(char*)nullptr = '0';
        }
        return it->data;
    }

    template<typename T>
    EcsError read(EntityId id, T** out) {
        return const_this()->read(id, const_cast<const T**>(out));
    }

    template<typename T>
    void read_or_panic(EntityId id, T** out) {
      *out = &read_or_panic<T>(id);
    }

    // Unsafe version of read that ignores NOT_FOUND errors.
    template<typename T>
    T& read_or_panic(EntityId id) {
      auto [found, it] = find_component<T>(id);
      if (!found) {
        std::cerr << "Exiting because of entity not found." << std::endl;
        *(char*)nullptr = '0';
      }
      return it->data;
    }

    template<typename...T>
    EcsError read(EntityId id, const T**...out) const {
        std::vector<EcsError> errors{ read(id, out)... };
        for (EcsError e : errors) if (e != EcsError::OK) return e;
        return EcsError::OK;
    }

    template<typename...T>
    void read_or_panic(EntityId id, const T**...out) const {
      (read_or_panic(id, out), ...);
    }

    template<typename...T>
    EcsError read(EntityId id, T**...out) {
        return const_this()->read(id, const_cast<const T**>(out)...);
    }

    template<typename...T>
    void read_or_panic(EntityId id, T**...out) {
      (read_or_panic(id, out), ...);
    }

    template<typename...U>
    ConstComponentRange<U...> read_all() const {
        using Range = ConstComponentRange<U...>;
        return Range(typename Range::StoreTuple(get_store<U>()...));
    }

    template<typename...U>
    ComponentRange<U...> read_all() {
      using Range = ComponentRange<U...>;
      return Range(typename Range::StoreTuple(get_store<U>()...));
    }

    bool has_entity(EntityId id) const {
      auto it = std::lower_bound(entity_ids_.begin(), entity_ids_.end(), id);
      return it != entity_ids_.end() && *it == id;
    }

    template<typename U>
    void erase_component(EntityId id) {
      auto [found, it] = find_component<U>(id);
      if (found) get_store<U>().erase(it);
    }

    void erase(EntityId id) {
      (erase_component<Components>(id), ...);
      auto it = std::lower_bound(entity_ids_.begin(), entity_ids_.end(), id);
      if (it == entity_ids_.end()) return;
      entity_ids_.erase(it);
    }
};
