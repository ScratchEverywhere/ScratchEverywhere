#pragma once
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

/** Should be used for maps with very very few items where hashing the string is more expensive than just searching. */
template <typename V>
class SmallStringMap {
  private:
    using Entry = std::pair<std::string, V>;
    std::vector<Entry> entries;

    static inline bool keysEqual(const std::string &a, const std::string &b) {
        const std::size_t n = a.size();
        if (n != b.size()) return false;
        const char *pa = a.data();
        const char *pb = b.data();
        for (std::size_t i = 0; i < n; ++i) {
            if (pa[i] != pb[i]) return false;
        }
        return true;
    }

  public:
    using iterator = typename std::vector<Entry>::iterator;
    using const_iterator = typename std::vector<Entry>::const_iterator;

    void reserve(std::size_t n) { entries.reserve(n); }

    iterator find(const std::string &key) {
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            if (keysEqual(it->first, key)) return it;
        }
        return entries.end();
    }

    const_iterator find(const std::string &key) const {
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            if (keysEqual(it->first, key)) return it;
        }
        return entries.end();
    }

    iterator end() { return entries.end(); }
    const_iterator end() const { return entries.end(); }

    V &operator[](const std::string &key) {
        auto it = find(key);
        if (it != entries.end()) return it->second;
        entries.push_back({key, V{}});
        return entries.back().second;
    }

    void erase(const std::string &key) {
        auto it = find(key);
        if (it != entries.end()) entries.erase(it);
    }

    void clear() { entries.clear(); }
};
