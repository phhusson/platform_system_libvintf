/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef ANDROID_VINTF_MAP_VALUE_ITERATOR_H
#define ANDROID_VINTF_MAP_VALUE_ITERATOR_H

#include <iterator>
#include <map>

namespace android {
namespace vintf {

// Iterator over all values of a std::map<K, V>
template<typename K, typename V, bool is_const>
struct MapValueIteratorImpl : public std::iterator <
        std::bidirectional_iterator_tag, /* Category */
        V,
        ptrdiff_t, /* Distance */
        typename std::conditional<is_const, const V *, V *>::type /* Pointer */,
        typename std::conditional<is_const, const V &, V &>::type /* Reference */
    >
{
    using traits = std::iterator_traits<MapValueIteratorImpl>;
    using ptr_type = typename traits::pointer;
    using ref_type = typename traits::reference;
    using diff_type = typename traits::difference_type;

    using map_iter = typename std::conditional<is_const,
            typename std::map<K, V>::const_iterator, typename std::map<K, V>::iterator>::type;

    MapValueIteratorImpl(map_iter i) : mIter(i) {}

    inline MapValueIteratorImpl &operator++()    {
        mIter++;
        return *this;
    };
    inline MapValueIteratorImpl  operator++(int) {
        MapValueIteratorImpl i = *this;
        mIter++;
        return i;
    }
    inline MapValueIteratorImpl &operator--()    {
        mIter--;
        return *this;
    }
    inline MapValueIteratorImpl  operator--(int) {
        MapValueIteratorImpl i = *this;
        mIter--;
        return i;
    }
    inline ref_type operator*() const  { return mIter->second; }
    inline ptr_type operator->() const { return &(mIter->second); }
    inline bool operator==(const MapValueIteratorImpl &rhs) const { return mIter == rhs.mIter; }
    inline bool operator!=(const MapValueIteratorImpl &rhs) const { return mIter != rhs.mIter; }

private:
    map_iter mIter;
};

template<typename K, typename V>
using MapValueIterator = MapValueIteratorImpl<K, V, false>;
template<typename K, typename V>
using ConstMapValueIterator = MapValueIteratorImpl<K, V, true>;

template<typename K, typename V, bool is_const>
struct MapValueIterableImpl {
    using map_ref = typename std::conditional<is_const,
        const std::map<K, V> &, std::map<K, V> &>::type;
    MapValueIterableImpl(map_ref map) : mMap(map) {}

    MapValueIteratorImpl<K, V, is_const> begin() {
        return MapValueIteratorImpl<K, V, is_const>(mMap.begin());
    }

    MapValueIteratorImpl<K, V, is_const> end() {
        return MapValueIteratorImpl<K, V, is_const>(mMap.end());
    }

private:
    map_ref mMap;
};

template<typename K, typename V>
using MapValueIterable = MapValueIterableImpl<K, V, false>;

template<typename K, typename V>
using ConstMapValueIterable = MapValueIterableImpl<K, V, true>;

} // namespace vintf
} // namespace android

#endif // ANDROID_VINTF_MAP_VALUE_ITERATOR_H
