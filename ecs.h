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
    template<typename U> const Store<U>& Get() const {
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
        template<typename U> auto& Get() const {
            return std::get<typename Store<U>::const_iterator>(its);
        }
        template<typename U>
        auto& Get() { return std::get<typename Store<U>::const_iterator>(its); }

        bool AnyAtEnd() const {
            return ((Get<T>() == range.Get<T>().end()) || ...);
        }

        bool AllSame() const { return ((Get<T>()->id == max_id) && ...); }

        // A helper to catch up iterators that are behind.
        template<typename U>
        EntityId IncrementIfLower(typename Store<U>::const_iterator& it,
            EntityId id) const {
            if (it != range.Get<U>().end() && it->id < id) it++;
            return it == range.Get<U>().end() ? id : it->id;
        }

        template<typename U>
        void IncrementIfNotAtEnd(typename Store<U>::const_iterator& it) {
          if (it != range.Get<U>().end()) ++it;
        }

        // After iteration, or on initialization, increment any iterators that
        // are behind.
        void CatchUp() {
          if (!AnyAtEnd()) max_id = std::max({Get<T>()->id...});
          while (!AnyAtEnd() && !AllSame()) {
              max_id = std::max({IncrementIfLower<T>(Get<T>(), max_id)...});
          }
          if (AnyAtEnd()) {
            its = {Get<T>() = range.Get<T>().end()...};
          }
        }


        ConstIterator& operator++() {
          // Increment at least once.
          if (!AnyAtEnd()) {
            (IncrementIfNotAtEnd<T>(Get<T>()), ...);
          }
          // And then until all iterators point to the same entity.
          CatchUp();

          return *this;
        }

        ConstIterator operator++(int) {
            ConstIterator old = *this;
            ++(*this);
            return old;
        }


        ConstIterator(const ConstComponentRange& range, iterator_tuple its)
            : range(range), its(std::move(its)) {
              CatchUp();  // Ensure a valid initial starting position.
        }

        bool operator==(const ConstIterator& other) {
          return AnyAtEnd() && other.AnyAtEnd() ||
              ((Get<T>() == other.Get<T>()) && ...);
        }
        bool operator!=(const ConstIterator& other) {
            return !(*this == other);
        }

        const_reference operator*() const {
            return std::forward_as_tuple(max_id, Get<T>()->data...);
        }
    };

public:
    explicit ConstComponentRange(std::tuple<const Store<T> &...> stores)
        : stores_(stores) { }

    ConstIterator begin() const {
        return ConstIterator(*this, { Get<T>().begin()... });
    }

    ConstIterator end() const {
        return ConstIterator(*this, { Get<T>().end()... });
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
    template<typename U> Store<U>& Get() const {
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
        template<typename U> auto& Get() const {
            return std::get<typename Store<U>::iterator>(its);
        }
        template<typename U>
        auto& Get() { return std::get<typename Store<U>::iterator>(its); }

        bool AnyAtEnd() const {
            return ((Get<T>() >= range.Get<T>().end()) || ...);
        }

        bool AllSame() const { return ((Get<T>()->id == max_id) && ...); }

        // A helper to catch up iterators that are behind.
        template<typename U>
        EntityId IncrementIfLower(typename Store<U>::iterator& it,
                                  EntityId id) const {
            if (it != range.Get<U>().end() && it->id < id) it++;
            return it == range.Get<U>().end() ? id : it->id;
        }

        template<typename U>
        void IncrementIfNotAtEnd(typename Store<U>::iterator& it) {
          if (it != range.Get<U>().end()) ++it;
        }

        // After iteration, or on initialization, increment any iterators that
        // are behind.
        void CatchUp() {
          if (!AnyAtEnd()) max_id = std::max({Get<T>()->id...});
          while (!AnyAtEnd() && !AllSame()) {
              max_id = std::max({IncrementIfLower<T>(Get<T>(), max_id)...});
          }
          if (AnyAtEnd()) {
            its = {Get<T>() = range.Get<T>().end()...};
          }
        }

        Iterator& operator++() {
            // Increment at least once.
            if (!AnyAtEnd()) {
              (IncrementIfNotAtEnd<T>(Get<T>()), ...);
            }
            // And then until all iterators point to the same entity.
            CatchUp();

            return *this;
        }

        Iterator operator++(int) {
            Iterator old = *this;
            ++(*this);
            return old;
        }

        Iterator(ComponentRange& range, iterator_tuple its)
            : range(range), its(std::move(its)) {
          CatchUp();  // Ensure a valid initial starting position.
        }

        bool operator==(const Iterator& other) {
          return AnyAtEnd() && other.AnyAtEnd() ||
              ((Get<T>() == other.Get<T>()) && ...);
        }
        bool operator!=(const Iterator& other) {
            return !(*this == other);
        }

        reference operator*() const {
            return std::forward_as_tuple(max_id, Get<T>()->data...);
        }
    };

public:
    using StoreTuple = std::tuple<Store<T>&...>;

    explicit ComponentRange(StoreTuple stores)
        : stores_(stores) { }

    Iterator begin() {
        return Iterator(*this, {Get<T>().begin()...});
    }

    Iterator end() {
        return Iterator(*this, {Get<T>().end()...});
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
    const Store<T>& GetStore() const { return std::get<Store<T>>(components_); }
    template<typename T>
    Store<T>& GetStore() { return std::get<Store<T>>(components_); }

    template<typename T>
    using FindResultConst = std::pair<bool, typename Store<T>::const_iterator>;

    template<typename T>
    using FindResult = std::pair<bool, typename Store<T>::iterator>;

    template<typename T>
    FindResultConst<T> FindComponent(EntityId id) const {
        const Store<T>& series = GetStore<T>();
        auto pred =
            [](const ComponentData<T>& c, EntityId id) { return c.id < id; };
        auto it = std::lower_bound(series.begin(), series.end(), id, pred);
        return std::make_pair(it != series.end() && it->id == id, it);
    }

    template<typename T>
    FindResult<T> FindComponent(EntityId id) {
        Store<T>& series = GetStore<T>();
        auto pred =
            [](const ComponentData<T>& c, EntityId id) { return c.id < id; };
        auto it = std::lower_bound(series.begin(), series.end(), id, pred);
        return std::make_pair(it != series.end() && it->id == id, it);
    }

    template<typename Iterator, typename T>
    void EmplaceAt(EntityId id, Iterator it, T data) {
        GetStore<T>().emplace(it, id, std::move(data));
    }

    const EntityComponentSystem<Components...>* ConstThis() const {
        return this;
    }

public:
    EntityComponentSystem() = default;

    EntityId NewEntity() {
        entity_ids_.push_back(next_id_);
        next_id_.id++;
        return entity_ids_.back();
    }

    enum WriteAction { CREATE_ENTRY, CREATE_OR_UPDATE };

    // Adds data to a component, although that the entity exists is taken for
    // granted.
    template<typename T>
    EcsError Write(EntityId id, T data,
                   WriteAction action = WriteAction::CREATE_ENTRY) {
        auto [found, insert] = FindComponent<T>(id);
        if (found && insert->id == id && action == WriteAction::CREATE_ENTRY) {
          return EcsError::ALREADY_EXISTS;
        }
        EmplaceAt(id, insert, std::move(data));
        return EcsError::OK;
    }

    // Adds data to a component, although that the entity exists is taken for
    // granted.
    template<typename T, typename U, typename...V>
    EcsError Write(EntityId id, T data, U next, V...rest) {
        EcsError e = Write(id, std::move(data));
        return e != EcsError::OK ?
            e : Write(id, std::move(next), std::move(rest)...);
    }

    template<typename...T>
    EntityId WriteNewEntity(T...components) {
        EntityId id = NewEntity();
        Write(id, std::move(components)...);
        return id;
    }

    template<typename T>
    EcsError Read(EntityId id, const T** out) const {
        auto [found, it] = FindComponent<T>(id);
        if (!found) return EcsError::NOT_FOUND;
        *out = &it->data;
        return EcsError::OK;
    }

    template<typename T>
    void ReadOrPanic(EntityId id, const T** out) const {
      *out = &ReadOrPanic<T>(id);
    }

    // Unsafe version of read that ignores NOT_FOUND errors.
    template<typename T>
    const T& ReadOrPanic(EntityId id) const {
        auto [found, it] = FindComponent<T>(id);
        if (!found) {
          std::cerr << "Exiting because of entity not found." << std::endl;
          *(char*)nullptr = '0';
        }
        return it->data;
    }

    template<typename T>
    EcsError Read(EntityId id, T** out) {
        return ConstThis()->Read(id, const_cast<const T**>(out));
    }

    template<typename T>
    void ReadOrPanic(EntityId id, T** out) {
      *out = &ReadOrPanic<T>(id);
    }

    // Unsafe version of read that ignores NOT_FOUND errors.
    template<typename T>
    T& ReadOrPanic(EntityId id) {
      auto [found, it] = FindComponent<T>(id);
      if (!found) {
        std::cerr << "Exiting because of entity not found." << std::endl;
        *(char*)nullptr = '0';
      }
      return it->data;
    }

    template<typename...T>
    EcsError Read(EntityId id, const T**...out) const {
        std::vector<EcsError> errors{ Read(id, out)... };
        for (EcsError e : errors) if (e != EcsError::OK) return e;
        return EcsError::OK;
    }

    template<typename...T>
    void ReadOrPanic(EntityId id, const T**...out) const {
      (ReadOrPanic(id, out), ...);
    }

    template<typename...T>
    EcsError Read(EntityId id, T**...out) {
        return ConstThis()->Read(id, const_cast<const T**>(out)...);
    }

    template<typename...T>
    void ReadOrPanic(EntityId id, T**...out) {
      (ReadOrPanic(id, out), ...);
    }

    template<typename...U>
    ConstComponentRange<U...> ReadAll() const {
        using Range = ConstComponentRange<U...>;
        return Range(typename Range::StoreTuple(GetStore<U>()...));
    }

    template<typename...U>
    ComponentRange<U...> ReadAll() {
      using Range = ComponentRange<U...>;
      return Range(typename Range::StoreTuple(GetStore<U>()...));
    }

    bool HasEntity(EntityId id) const {
      auto it = std::lower_bound(entity_ids_.begin(), entity_ids_.end(), id);
      return it != entity_ids_.end() && *it == id;
    }

    template<typename U>
    void EraseComponent(EntityId id) {
      auto [found, it] = FindComponent<U>(id);
      if (found) GetStore<U>().erase(it);
    }

    void Erase(EntityId id) {
      (EraseComponent<Components>(id), ...);
      auto it = std::lower_bound(entity_ids_.begin(), entity_ids_.end(), id);
      if (it == entity_ids_.end()) return;
      entity_ids_.erase(it);
    }
};
