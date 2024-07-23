
#include <iostream>
#include <type_traits>
#include <string>
#include <vector>
#include <cmath>
#include <memory>
#include <set>
#include <unordered_map>
#include <tuple>
#include <functional>

#ifndef __Z_CONSISTENT_HASH
#define __Z_CONSISTENT_HASH

struct DefaultHash {
        template <typename... TArg>
        uint32_t operator() (TArg... args) {
                return std::rand();
        }
};

template <typename Item, typename T>
concept HashFunc = requires(Item __item) { std::same_as<uint32_t, decltype(T(__item))>; };

template <typename Item, typename T>
concept StorageType = requires(Item __item, T __stor) {
        // have random-access iterator
        typename std::iterator_traits<typename T::iterator>::iterator_gategory;
        // can use operator[] to randomly access inner elements
        __stor.operator[](static_cast<size_t>(0));

        // check if __item exist in Storage
        __stor.exist(__item);
        __stor.exist(std::reference_wrapper<Item>(__item));
        __stor.exist(std::move(__item));

        // insert a new item
        __stor.insert(__item);
        __stor.insert(std::reference_wrapper<Item>(__item));
        __stor.insert(std::move(__item));

        // remove an item use iterator
        __stor.remove(T::iterator());
        // check current size
        __stor.size();

        __stor.begin(); // auto rbegin()
        __stor.end();   // auto rend()

} && std::is_base_of<
        std::random_access_iterator_tag,
        typename std::iterator_traits<typename T::iterator>::iterator_gategory
>::value;

/**
 * 用于实现Storage的模版，可以不使用，仅作参考
 */
template <typename Item>
class BaseStorage {
        class iterator;
public:
        virtual ~BaseStorage();

        virtual Item& operator[] (const size_t) const = 0;
        virtual bool exist(const Item&) const noexcept = 0;
        virtual bool exist(Item&&) const noexcept = 0;
        virtual size_t insert(const Item&) noexcept = 0;
        virtual size_t insert(Item&&) noexcept = 0;
        virtual bool remove(iterator&) = 0;
        virtual size_t size() const noexcept = 0;

        virtual iterator begin() noexcept = 0;
        virtual iterator end() noexcept = 0;
};

// default init size of zcycle
constexpr size_t DEFAULT_SIZE_OF_ZCYCLE = 32;

/**
 * ItemType: anything can be stored, need default constructor
 * Storage: base type to store items, will not change memory position after inserted into Storage
 * Hash: calculate hash code for ItemType
 */
template <
        typename ItemType,
        StorageType<std::decay_t<ItemType>> Storage, // StorageType Storage = std::vector<std::decay_t<ItemType>>,
        HashFunc<std::decay_t<ItemType>> Hash = DefaultHash
> class zcycle {
private:
        using InnerType = typename std::decay<ItemType>::type;

        struct CycleWrapper {
                // can not copy
                std::unique_ptr<Storage> __items;
                // item index to zcycle slot indexes
                // std::vector< std::set< size_t > > __pointers;
                std::unordered_map<uint64_t, std::vector<size_t>> __pointers;
                // store pointer to item
                uint64_t* __zcycle;
                size_t __size, __capacity;

                CycleWrapper() noexcept;
                explicit CycleWrapper(const size_t) noexcept;

                ~CycleWrapper();

                size_t expand();
        };
public:
        zcycle new_default() noexcept;
        zcycle new_with_capacity(const size_t) noexcept;

        // can not copy
        zcycle(const zcycle&) = delete;
        zcycle& operator=(const zcycle&) = delete;
        // can moved
        zcycle(zcycle&&) = default;
        zcycle& operator=(zcycle&&) = default;

        // find an item
        InnerType& access(const size_t) const;
        // load a new item into zcycle
        bool load(const InnerType&);

        // T, T&, const T&, T&&
        bool remove(const InnerType&, const std::function<bool(const InnerType&, const InnerType&)>);

private:
        CycleWrapper __cycle;

        zcycle() noexcept : __cycle(CycleWrapper()) {}
        explicit zcycle(const size_t size) noexcept : __cycle(CycleWrapper(size)) {}

};



#pragma region CycleWrapper Part

template<typename ItemType, StorageType<std::decay_t<ItemType>> Storage, HashFunc<std::decay_t<ItemType>> Hash>
inline zcycle<ItemType, Storage, Hash>::CycleWrapper::CycleWrapper() noexcept {
        __items = std::make_unique(new Storage());
        __zcycle = (uint64_t*)malloc(sizeof(uint64_t) * DEFAULT_SIZE_OF_ZCYCLE);
        memset(__zcycle, 0, sizeof(uint64_t) * DEFAULT_SIZE_OF_ZCYCLE);
        // __zcycle.resize(DEFAULT_SIZE_OF_ZCYCLE, 0);
        __size = 0, __capacity = DEFAULT_SIZE_OF_ZCYCLE;
}

template<typename ItemType, StorageType<std::decay_t<ItemType>> Storage, HashFunc<std::decay_t<ItemType>> Hash>
inline zcycle<ItemType, Storage, Hash>::CycleWrapper::CycleWrapper(const size_t size) noexcept {
        __items = std::make_unique(new Storage());
        __zcycle = (uint64_t*)malloc(sizeof(uint64_t) * size);
        memset(__zcycle, 0, sizeof(uint64_t) * size);
        // __zcycle.resize(size, 0);
        __size = 0, __capacity = size;
}

template<typename ItemType, StorageType<std::decay_t<ItemType>> Storage, HashFunc<std::decay_t<ItemType>> Hash>
inline zcycle<ItemType, Storage, Hash>::CycleWrapper::~CycleWrapper() {
        if (__zcycle != nullptr) {
                free(__zcycle);
        }
}

// expand will rehash every items in current cycle
template<typename ItemType, StorageType<std::decay_t<ItemType>> Storage, HashFunc<std::decay_t<ItemType>> Hash>
inline size_t zcycle<ItemType, Storage, Hash>::CycleWrapper::expand() {
        size_t _e_size = __capacity << 1;
        if (_e_size <= __capacity) {
                throw std::bad_alloc();
        }

        __pointers.clear();

        uint64_t* __mem = (uint64_t*)malloc(sizeof(uint64_t) * _e_size);
        memset(__mem, 0, sizeof(uint64_t) * _e_size);

        // for each item, rehash in new memory
        for (auto &item : __items) {
                uint32_t _s_index = Hash(item) & _e_size;

                std::vector<size_t> _pe;

                // conflict process
                while (__mem[_s_index] != 0 && __size < _e_size) {
                        _s_index  = ( _s_index + std::rand() % std::max(_e_size >> 5, (size_t)8) ) % _e_size;
                }

                if (__mem[_s_index] != 0) {
                        throw std::bad_alloc();
                }

                // pointer to item, just reflect into one slot
                __mem[_s_index] = static_cast<uint64_t>(&item);
                __size++;

                _pe.emplace_back(_s_index);

                __pointers[static_cast<uint64_t>(&item)] = std::move(_pe);
        }

        __capacity = _e_size;
        if (__zcycle != nullptr) free(__zcycle);
        __zcycle = __mem;

        // return new capacity
        return _e_size;
}


#pragma region ZCycle Part

template <typename ItemType, StorageType<std::decay_t<ItemType>> Storage, HashFunc<std::decay_t<ItemType>> Hash> 
inline zcycle<ItemType, Storage, Hash> 
zcycle<ItemType, Storage, Hash>::new_default() noexcept {
        return zcycle();
}

template <typename ItemType, StorageType<std::decay_t<ItemType>> Storage, HashFunc<std::decay_t<ItemType>> Hash> 
inline zcycle<ItemType, Storage, Hash> 
zcycle<ItemType, Storage, Hash>::new_with_capacity(const size_t size) noexcept {
        return zcycle(size);
}

template <typename ItemType, StorageType<std::decay_t<ItemType>> Storage, HashFunc<std::decay_t<ItemType>> Hash> 
inline typename zcycle<ItemType, Storage, Hash>::InnerType& 
zcycle<ItemType, Storage, Hash>::access(const size_t index) const {
        if (index < 0 || index >= __cycle.__capacity) {
                throw std::out_of_range("index " + std::to_string(index) +" out of range (0 ~ ", std::to_string(__cycle.__capacity) + ")");
        }

        if (__cycle.__size == 0) {
                throw std::out_of_range("can not access empty zcycle, you should load item first");
        }

        uint64_t* _mem = __cycle.__zcycle;
        while (_mem[index] == 0) {
                index = (index + 1) % __cycle.__capacity;
        }

        return *static_cast<InnerType*>(_mem[index]);
}

// TODO
template <typename ItemType, StorageType<std::decay_t<ItemType>> Storage, HashFunc<std::decay_t<ItemType>> Hash>
inline bool zcycle<ItemType, Storage, Hash>::load(const InnerType& item) {
        if (__cycle.__size >= __cycle.__capacity / 3) {
                // 随着空间被占满插入时间会增加，所以增大空间
                __cycle.expand();
        }

        size_t cap = __cycle.__capacity;
        size_t index = Hash(item) % cap;

        uint64_t* __mem = __cycle.__zcycle;

        // conflict process
        while (__mem[index] != 0) {
                index  = ( index + std::rand() % std::max(cap >> 5, (size_t)8) ) % cap;
        }

        __cycle.__items.insert(item);
        auto iter = __cycle.__items.end() - 1;
        uint64_t ptr = static_cast<uint64_t>(&(*iter));
        __mem[index] = ptr;
        __cycle.__pointers[ptr].emplace_back(index);

        return true;
}


template<typename ItemType, StorageType<std::decay_t<ItemType>> Storage, HashFunc<std::decay_t<ItemType>> Hash>
inline bool zcycle<ItemType, Storage, Hash>::remove(const InnerType& inner, const std::function<bool(const InnerType&, const InnerType&)> eq_cmp) {
        for (auto iter = __cycle.__items.begin(); iter != __cycle.__items.end(); ++iter) {
                if (eq_cmp(*iter, inner)) {
                        auto vec = __cycle.__pointers[&*iter];
                        for (auto &index : vec) {
                                __cycle.__zcycle[index] = 0;
                        }
                        __cycle.__pointers.erase(&iter);
                        __cycle.__items.remove(iter);
                        return true;
                }
        }

        return false;
}

#endif


/**
 * iterator
 * 
 * *() -> InnerType&
 * ->() -> InnerType*
 */