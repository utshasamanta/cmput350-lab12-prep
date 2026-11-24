#ifndef REGISTRY_H
#define REGISTRY_H

#include <cassert>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

template <typename T>
class Registry {
public:
    // perfect forwarding implementation -- they might e.g. separate move and copy.
    template <typename U>
    size_t store(U&& toStore) {
        static_assert(std::is_constructible_v<T, U>, "Registry attempted to store invalid type");
        size_t ret;
        ++mSize;
        if (!mFreeList.empty()) {
            ret = mFreeList.back();
            mFreeList.pop_back();
            mStorage[ret] = std::forward<U>(toStore);
            mValid[ret] = true;
            return ret;
        }
        ret = mStorage.size();
        mStorage.emplace_back(std::forward<U>(toStore));
        mValid.push_back(true);
        return ret;
    }

    void remove(size_t index) {
        assert(valid(index));
        --mSize;
        mValid[index] = false;
        mStorage[index] = T{};  // by assumption, this is ok. In general you'd want to do
        // placement new and explicitly call destructor.
        mFreeList.push_back(index);
    }

    const T& operator[](size_t index) const {
        assert(valid(index));
        return mStorage[index];
    }

    T& operator[](size_t index) {
        assert(valid(index));
        return const_cast<T&>(const_cast<const Registry<T>*>(this)->operator[](index));
    }

    bool valid(const size_t index) const { return index < mValid.size() && mValid[index]; }

    size_t size() const { return mSize; }

    void reserve(size_t capacity) {
        mStorage.reserve(capacity);
        mFreeList.reserve(capacity);
        mValid.reserve(capacity);
    }

    size_t containerSize() const { return mStorage.size(); }

private:
    std::vector<T> mStorage;
    std::vector<size_t> mFreeList;
    std::vector<bool> mValid;
    size_t mSize = 0;
};

#endif  // REGISTRY_H
