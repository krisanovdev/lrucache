#pragma once

#include <unordered_map>
#include <list>
#include <utility>
#include <string_view>

namespace lru {

/// A fixed-capacity least-recently-used associative container.
///
/// `lrucache` provides an interface similar to `std::unordered_map` while
/// automatically evicting the least recently used element when capacity is
/// exceeded. Elements are stored in usage order.
///
/// Successful calls to:
/// - at()
/// - operator[]
/// - insert*() on existing keys
/// - try_emplace() on existing keys
/// - promote()
///
/// move the accessed element to the front of the cache.
///
/// Lookup operations such as find() and contains() do not affect ordering.
///
/// Complexity:
/// - Lookup:        average O(1)
/// - Insertion:     average O(1)
/// - Erasure:       average O(1)
/// - Promotion:     O(1)
/// - Eviction:      O(1)
///
/// Template parameters:
/// - Key:
///     key type.
///
/// - Value:
///     mapped value type.
///
/// - CheapCopyKeys:
///     Controls how keys are stored internally in the hash table.
///
///     If true (default), keys are copied into the hash table.
///
///     If false, the hash table stores lightweight references to keys owned
///     by the internal list nodes, reducing key duplication for expensive
///     key types such as std::string.
///
/// Iterator invalidation:
/// - Eviction/erasing invalidates iterators to erased elements only.
/// - Otherwise all iterators to non-evicted elements are always valid
///
/// Heterogeneous lookup is supported when the underlying hash/equality
/// operations are transparent.
///
/// Example:
///
///   lru::lrucache<std::string, int> cache(3);
///
///   cache.insert("a", 1);
///   cache.insert("b", 2);
///   cache.insert("c", 3);
///
///   cache.at("a"); // promotes "a"
///
///   cache.insert("d", 4); // evicts "b"
///

template <typename Key, typename Value, bool CheapCopyKeys = true>
class lrucache {

    using list_storage_t = std::list<std::pair<const Key, Value>>;

public:
    using key_type        = Key;
    using mapped_type     = Value;
    using value_type      = std::pair<const Key, Value>;
    using iterator        = typename list_storage_t::iterator;
    using const_iterator  = typename list_storage_t::const_iterator;
    using reverse_iterator       = typename list_storage_t::reverse_iterator;
    using const_reverse_iterator = typename list_storage_t::const_reverse_iterator;

    explicit lrucache(size_t capacity) : _capacity(capacity) {
        _map.reserve(_capacity);
    }

    lrucache(const lrucache&) = delete;
    lrucache& operator=(const lrucache&) = delete;
    lrucache(lrucache&&) noexcept = default;
    lrucache& operator=(lrucache&&) noexcept = default;

    iterator begin() noexcept { return _list.begin(); }
    iterator end() noexcept { return _list.end(); }
    const_iterator cbegin() const noexcept { return _list.cbegin(); }
    const_iterator cend() const noexcept { return _list.cend(); }
    reverse_iterator rbegin() noexcept { return _list.rbegin(); }
    reverse_iterator rend() noexcept { return _list.rend(); }

    size_t size() const noexcept { return _map.size(); }
    bool empty() const noexcept { return _map.empty(); }
    size_t capacity() const noexcept { return _capacity; }

    void capacity(size_t new_cap) {
        _capacity = new_cap;
        _map.reserve(_capacity);
        while (_map.size() > _capacity) {
            evict(std::prev(_list.end()));
        }
    }

    template<class K>
    iterator find(const K& key) {
        const auto it = _map.find(key);
        return it == _map.end() ? end() : it->second;
    }

    template<class K>
    const_iterator find(const K& key) const {
        const auto it = _map.find(key);
        return it == _map.end() ? cend() : it->second;
    }

    template<class K>
    bool contains(const K& key) const {
        return _map.find(key) != _map.end();
    }

    template<class K>
    Value& at(const K& key) {
        if (const auto it = find(key); it != end()) {
            promote_impl(it);
            return it->second;
        }
        throw std::out_of_range("lrucache::at : key not found");
    }

    template<class K>
    const Value& at(const K& key) const {
        if (const auto it = find(key); it != cend())
            return it->second;
        throw std::out_of_range("lrucache::at : key not found");
    }

    Value& operator[](const Key& key) requires std::default_initializable<Value> {
        if (_capacity == 0)
            throw std::out_of_range("lrucache::operator[] : called with zero capacity");
        return try_emplace(key).first->second;
    }

    std::pair<iterator, bool> insert(const Key& key, const Value& value) {
        return try_emplace_impl(key, value);
    }

    template<class P>
    requires std::constructible_from<value_type, P&&>
    std::pair<iterator, bool> insert(P&& value) {
        if (_capacity == 0)
            return {_list.end(), false};

        if (iterator it = insert_lookup(value.first); it != end())
            return {it, false};

        _list.emplace_front(std::forward<P>(value));
        return insert_epilogue();
    }

    void insert(std::initializer_list<value_type> ilist) {
        for (const auto& [key, value] : ilist) {
            insert(key, value);
        }
    }

    template <typename M>
    requires std::constructible_from<Value, M&&>
    std::pair<iterator, bool> insert_or_assign(const Key& key, M&& obj) {
        if (_capacity == 0)
            return {_list.end(), false};

        if (iterator it = insert_lookup(key); it != end()) {
            it->second = std::forward<M>(obj);
            return {it, false};
        }

        _list.emplace_front(key, std::forward<M>(obj));
        return insert_epilogue();
    }

    template<class... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        return insert(value_type(std::forward<Args>(args)...));
    }

    template<class... Args>
    std::pair<iterator, bool> try_emplace(const Key& k, Args&&... args) {
        return try_emplace_impl(k, std::forward<Args>(args)...);
    }

    template<class... Args>
    std::pair<iterator, bool> try_emplace(Key&& k, Args&&... args) {
        return try_emplace_impl(std::move(k), std::forward<Args>(args)...);
    }

    template<class K>
    size_t erase(const K& key) {
        const auto it = _map.find(key);
        return it == _map.end() ? 0 : (evict(it->second), 1);
    }

    iterator erase(iterator it) {
        return evict(it);
    }

    iterator erase(const_iterator it) {
        return evict(it);
    }

    iterator erase(const_iterator first, const_iterator last) {
        while (first != last) {
            first = evict(first);
        }
        return _list.erase(last, last);
    }

    void promote(const_iterator it) {
        promote_impl(it);
    }

    void clear() noexcept {
        _map.clear();
        _list.clear();
    }

    void swap(lrucache& other) noexcept {
        _list.swap(other._list);
        _map.swap(other._map);
        std::swap(_capacity, other._capacity);
    }

private:
    struct key_view_t {
        const Key* ptr;
        key_view_t(const Key& ref) : ptr(&ref) {}
        bool operator==(const key_view_t& other) const { return *ptr == *other.ptr; }

        template<typename T>
        bool operator==(const T& other) const { return *ptr == other; }
    };

    using map_key_t = std::conditional_t<CheapCopyKeys, Key, key_view_t>;

    struct map_hash_t {
        using is_transparent = void;
        size_t operator()(const Key& k) const noexcept { return std::hash<Key>{}(k); }
        size_t operator()(const key_view_t& k) const noexcept { return std::hash<Key>{}(*k.ptr); }
        size_t operator()(std::string_view k) const noexcept { return std::hash<std::string_view>{}(k); }

        template <class T>
        std::size_t operator()(T&& v) const noexcept {
            if constexpr (std::is_convertible_v<T, std::string_view>) {
                return (*this)(std::string_view(v));
            } else {
                return std::hash<std::decay_t<T>>{}(std::forward<T>(v));
            }
        }
    };

    struct map_equal_t {
        using is_transparent = void;

        template <typename T, typename U>
        bool operator()(const T& lhs, const U& rhs) const noexcept { return lhs == rhs; }
    };

private:
    size_t _capacity;
    list_storage_t _list;
    std::unordered_map<map_key_t, iterator, map_hash_t, map_equal_t> _map;

    iterator evict(const_iterator it) {
        _map.erase(it->first);
        return _list.erase(it);
    }

    void promote_impl(const_iterator it) {
        _list.splice(_list.begin(), _list, it);
    }

    template<class K>
    iterator insert_lookup(const K& key) {
        iterator it = find(key);
        if (it != end())
            promote_impl(it);
        return it;
    }

    std::pair<iterator, bool> insert_epilogue() {
        try {
            _map.emplace(_list.front().first, _list.begin());
            if (_map.size() > _capacity)
                evict(std::prev(_list.end()));
        } catch (...) {
            _list.pop_front();
            throw;
        }
        return {begin(), true};
    }

    template<class K, class... Args>
    std::pair<iterator, bool>
    try_emplace_impl(K&& k, Args&&... args) {
        if (_capacity == 0)
            return {_list.end(), false};

        if (iterator it = insert_lookup(k); it != end())
            return {it, false};

        _list.emplace_front(
            std::piecewise_construct,
            std::forward_as_tuple(std::forward<K>(k)),
            std::forward_as_tuple(std::forward<Args>(args)...)
        );
        return insert_epilogue();
    }
};

} // namespace lru
