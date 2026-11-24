#ifndef AABB_H
#define AABB_H

#include "vector_util.h"
#include <SFML/System.hpp>

template <typename T>
struct AABB {
    constexpr AABB& operator&=(const AABB& other) {
        minPoint = {std::max(minPoint.x, other.minPoint.x), std::max(minPoint.y, other.minPoint.y)};
        maxPoint = {std::min(maxPoint.x, other.maxPoint.x), std::min(maxPoint.y, other.maxPoint.y)};
        return *this;
    }

    constexpr AABB operator&(const AABB& other) const {
        auto ret = *this;
        return ret &= other;
    }

    constexpr AABB& operator|=(const AABB& other) {
        minPoint = {std::min(minPoint.x, other.minPoint.x), std::min(minPoint.y, other.minPoint.y)};
        maxPoint = {std::max(maxPoint.x, other.maxPoint.x), std::max(maxPoint.y, other.maxPoint.y)};
        return *this;
    }

    constexpr AABB operator|(const AABB& other) const {
        auto ret = *this;
        return ret |= other;
    }

    constexpr bool empty() const { return minPoint.x > maxPoint.x || minPoint.y > maxPoint.y; }

    explicit constexpr operator bool() const { return !empty(); }

    constexpr bool intersects(const AABB& other) const { return !((*this) & other).empty(); }

    constexpr bool contains(const sf::Vector2<T>& other) const {
        return minPoint.x <= other.x && maxPoint.x >= other.x && minPoint.y <= other.y &&
               maxPoint.y >= other.y;
    }

    constexpr bool contains(const AABB& other) const {
        return contains(other.minPoint) && contains(other.maxPoint);
    }

    constexpr sf::Vector2<T> middle() const {
        return minPoint + (maxPoint - minPoint) / static_cast<T>(2);
    }

    sf::Vector2<T> minPoint;
    sf::Vector2<T> maxPoint;
};

template <typename T>
std::ostream& operator<<(std::ostream& os, const AABB<T>& v) {
    return os << "AABB{ min=" << v.minPoint << ", max=" << v.maxPoint << " }";
}

#endif  // AABB_H
