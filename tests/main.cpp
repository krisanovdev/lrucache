#include <cassert>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include "lrucache.hpp"

// ---------------------------------------------------------------------
// Simple test macro
// ---------------------------------------------------------------------
#define TEST_ASSERT(cond, msg) \
    do { if (!(cond)) { std::cerr << "FAIL: " << msg << std::endl; return false; } } while(0)

using TestFunction = bool(*)();

// ---------------------------------------------------------------------
// 1. Construction and basic capacity
// ---------------------------------------------------------------------
bool test_construction() {
    lru::lrucache<int, int> cache(5);
    TEST_ASSERT(cache.capacity() == 5, "capacity() = 5");
    TEST_ASSERT(cache.size() == 0, "size() = 0");
    TEST_ASSERT(cache.empty(), "empty() true");

    lru::lrucache<int, int> zero_cap(0);
    TEST_ASSERT(zero_cap.capacity() == 0, "capacity 0 allowed");
    TEST_ASSERT(zero_cap.size() == 0, "size 0");

    std::cout << "test_construction: OK\n";
    return true;
}

// ---------------------------------------------------------------------
// 2. Insert, eviction
// ---------------------------------------------------------------------
bool test_insert_and_eviction() {
    lru::lrucache<int, int> cache(3);

    cache.insert(1, 100);
    cache.insert(2, 200);
    cache.insert(3, 300);

    cache.insert(4, 400);

    TEST_ASSERT(!cache.contains(1), "1 evicted");
    TEST_ASSERT(cache.contains(2), "2 present");
    TEST_ASSERT(cache.contains(3), "3 present");
    TEST_ASSERT(cache.contains(4), "4 present");

    cache.at(2);

    cache.insert(5, 500);

    TEST_ASSERT(!cache.contains(3), "3 evicted after promotion logic");
    TEST_ASSERT(cache.contains(2), "2 present");
    TEST_ASSERT(cache.contains(4), "4 present");
    TEST_ASSERT(cache.contains(5), "5 present");

    std::cout << "test_insert_and_eviction: OK\n";
    return true;
}

// ---------------------------------------------------------------------
// 3. find / contains (no promotion)
// ---------------------------------------------------------------------
bool test_find_and_contains() {
    lru::lrucache<int, int> cache(3);

    cache.insert(10, 100);
    cache.insert(20, 200);
    cache.insert(30, 300);

    cache.find(20);

    cache.insert(40, 400);

    TEST_ASSERT(!cache.contains(10), "10 evicted (no promotion)");
    TEST_ASSERT(cache.contains(20), "20 exists");
    TEST_ASSERT(cache.contains(30), "30 exists");
    TEST_ASSERT(cache.contains(40), "40 exists");

    std::cout << "test_find_and_contains: OK\n";
    return true;
}

// ---------------------------------------------------------------------
// 4. at / operator[]
// ---------------------------------------------------------------------
bool test_at_and_subscript() {
    lru::lrucache<int, int> cache(2);

    cache.insert(1, 10);
    cache.insert(2, 20);

    TEST_ASSERT(cache.at(1) == 10, "at works");
    cache.at(1) = 100;
    TEST_ASSERT(cache.at(1) == 100, "mutation works");

    cache[3] = 30;
    TEST_ASSERT(cache.contains(3), "operator[] inserts");

    std::cout << "test_at_and_subscript: OK\n";
    return true;
}

// ---------------------------------------------------------------------
// 5. insert_or_assign
// ---------------------------------------------------------------------
bool test_insert_or_assign() {
    lru::lrucache<int, int> cache(2);

    cache.insert_or_assign(1, 10);
    cache.insert_or_assign(1, 20);

    TEST_ASSERT(cache.at(1) == 20, "assign overwrites");

    std::cout << "test_insert_or_assign: OK\n";
    return true;
}

// ---------------------------------------------------------------------
// 6. erase
// ---------------------------------------------------------------------
bool test_erase() {
    lru::lrucache<int, int> cache(3);

    cache.insert(1, 1);
    cache.insert(2, 2);
    cache.insert(3, 3);

    cache.erase(2);

    TEST_ASSERT(!cache.contains(2), "erase key");

    auto it = cache.find(1);
    cache.erase(it);

    TEST_ASSERT(!cache.contains(1), "erase iterator");

    std::cout << "test_erase: OK\n";
    return true;
}

// ---------------------------------------------------------------------
// 7. promote
// ---------------------------------------------------------------------
bool test_promote() {
    lru::lrucache<int, int> cache(3);

    cache.insert(1, 1);
    cache.insert(2, 2);
    cache.insert(3, 3);

    cache.promote(cache.find(1));

    cache.insert(4, 4);

    TEST_ASSERT(!cache.contains(2), "2 evicted after promote");
    TEST_ASSERT(cache.contains(1), "1 present");

    std::cout << "test_promote: OK\n";
    return true;
}

// ---------------------------------------------------------------------
// 8. capacity change
// ---------------------------------------------------------------------
bool test_capacity_change() {
    lru::lrucache<int, int> cache(3);

    cache.insert(1, 1);
    cache.insert(2, 2);
    cache.insert(3, 3);

    cache.capacity(2);

    TEST_ASSERT(cache.size() == 2, "shrunk");

    std::cout << "test_capacity_change: OK\n";
    return true;
}

// ---------------------------------------------------------------------
// 9. clear / swap
// ---------------------------------------------------------------------
bool test_clear_and_swap() {
    lru::lrucache<int, int> a(3), b(3);

    a.insert(1, 1);
    b.insert(2, 2);

    a.swap(b);

    TEST_ASSERT(b.contains(1), "swap works");

    a.clear();

    TEST_ASSERT(a.empty(), "clear works");

    std::cout << "test_clear_and_swap: OK\n";
    return true;
}

// ---------------------------------------------------------------------
// 10. iterators
// ---------------------------------------------------------------------
bool test_iterators() {
    lru::lrucache<int, int> cache(3);

    cache.insert(1, 1);
    cache.insert(2, 2);
    cache.insert(3, 3);

    std::vector<int> v;
    for (auto& [k, v2] : cache)
        v.push_back(k);

    TEST_ASSERT(v == std::vector<int>({3, 2, 1}), "iterators work");

    std::cout << "test_iterators: OK\n";
    return true;
}

// ---------------------------------------------------------------------
// 11. heterogeneous lookup
// ---------------------------------------------------------------------
bool test_heterogeneous_lookup() {
    lru::lrucache<int, int> cache(3);

    cache.insert(100, 999);

    long key = 100;

    TEST_ASSERT(cache.contains(key), "heterogeneous contains");
    TEST_ASSERT(cache.at(key) == 999, "heterogeneous at");

    std::cout << "test_heterogeneous_lookup: OK\n";
    return true;
}

// ---------------------------------------------------------------------
// 12. string keys non cheap
// ---------------------------------------------------------------------
bool test_string_keys_non_cheap() {
    lru::lrucache<std::string, int, false> cache(3);

    cache.insert("a", 1);
    cache.insert("b", 2);

    TEST_ASSERT(cache.contains("a"), "string lookup");

    std::cout << "test_string_keys_non_cheap: OK\n";
    return true;
}

// ---------------------------------------------------------------------
// 13. initializer list
// ---------------------------------------------------------------------
bool test_initializer_list_insert() {
    lru::lrucache<int, int> cache(3);

    cache.insert({
        {1, 10},
        {2, 20},
        {3, 30}
    });

    TEST_ASSERT(cache.size() == 3, "ilist");

    std::cout << "test_initializer_list_insert: OK\n";
    return true;
}

// ---------------------------------------------------------------------
// 14. insert(P&&)
// ---------------------------------------------------------------------
bool test_insert_pair_overload() {
    lru::lrucache<int, std::string> cache(2);

    cache.insert(std::pair{1, std::string("a")});

    std::pair<int, std::string> p{2, "b"};
    cache.insert(p);

    TEST_ASSERT(cache.contains(1), "rvalue pair insert");
    TEST_ASSERT(cache.contains(2), "lvalue pair insert");

    std::cout << "test_insert_pair_overload: OK\n";
    return true;
}

// ---------------------------------------------------------------------
// 15. emplace
// ---------------------------------------------------------------------
bool test_emplace() {
    lru::lrucache<int, std::string> cache(2);

    cache.emplace(1, "x");

    TEST_ASSERT(cache.contains(1), "emplace");

    std::cout << "test_emplace: OK\n";
    return true;
}

// ---------------------------------------------------------------------
// 16. try_emplace
// ---------------------------------------------------------------------
bool test_try_emplace_lvalue_key() {
    lru::lrucache<int, std::string> cache(2);

    int key = 1;

    auto [it1, ins1] = cache.try_emplace(key, "a");

    TEST_ASSERT(ins1, "insert via lvalue key");
    TEST_ASSERT(cache.at(1) == "a", "value correct");

    auto [it2, ins2] = cache.try_emplace(key, "b");

    TEST_ASSERT(!ins2, "duplicate not inserted");
    TEST_ASSERT(cache.at(1) == "a", "value unchanged");

    std::cout << "test_try_emplace_lvalue_key: OK\n";
    return true;
}

// ---------------------------------------------------------------------
// 17. try_emplace (rvalue key)
// ---------------------------------------------------------------------
bool test_try_emplace_rvalue_key() {
    lru::lrucache<std::string, int> cache(2);

    std::string key = "hello";

    auto [it1, ins1] = cache.try_emplace(std::move(key), 10);

    TEST_ASSERT(ins1, "rvalue insert");
    TEST_ASSERT(cache.contains("hello"), "key exists");

    auto [it2, ins2] = cache.try_emplace(std::string("hello"), 20);

    TEST_ASSERT(!ins2, "duplicate rejected");

    std::cout << "test_try_emplace_rvalue_key: OK\n";
    return true;
}

// ---------------------------------------------------------------------
// 18. try_emplace + eviction
// ---------------------------------------------------------------------
bool test_try_emplace_eviction() {
    lru::lrucache<int, int> cache(2);

    cache.try_emplace(1, 1);
    cache.try_emplace(2, 2);

    cache.try_emplace(3, 3);

    TEST_ASSERT(!cache.contains(1), "eviction works");
    TEST_ASSERT(cache.contains(3), "3 exists");

    std::cout << "test_try_emplace_eviction: OK\n";
    return true;
}

int main() {
    std::vector<std::pair<std::string, TestFunction>> tests = {
        {"Construction", test_construction},
        {"Insert & eviction", test_insert_and_eviction},
        {"Find & contains", test_find_and_contains},
        {"at & []", test_at_and_subscript},
        {"insert_or_assign", test_insert_or_assign},
        {"erase", test_erase},
        {"promote", test_promote},
        {"capacity change", test_capacity_change},
        {"clear & swap", test_clear_and_swap},
        {"iterators", test_iterators},
        {"heterogeneous", test_heterogeneous_lookup},
        {"string keys", test_string_keys_non_cheap},
        {"initializer list", test_initializer_list_insert},
        {"insert(P&&)", test_insert_pair_overload},
        {"emplace", test_emplace},
        {"try_emplace lvalue", test_try_emplace_lvalue_key},
        {"try_emplace rvalue", test_try_emplace_rvalue_key},
        {"try_emplace eviction", test_try_emplace_eviction},
    };

    int passed = 0;

    for (auto& [name, fn] : tests) {
        std::cout << "\n--- " << name << " ---\n";
        if (fn()) ++passed;
        else std::cout << "FAILED: " << name << "\n";
    }

    std::cout << "\nPassed " << passed << " / " << tests.size() << "\n";
    return passed == tests.size() ? 0 : 1;
}
