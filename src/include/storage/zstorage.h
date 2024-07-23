
#include <iostream>
#include <memory>
#include <unistd.h>
#include <bitset>

#ifndef __Z_STORAGE
#define __Z_STORAGE

/**
 * 
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
 */

// allocate unit is 4096 (4KB) for 32bit system, 8KB for 64bit system
constexpr size_t BASE_ALLOCATOR_UNIT = 4096;

// page中包括多个slot，每个slot存储一个T，位图表示当前slot是否存在T
// T被包装为双向链表，rbt用于加速查找T，指向链表中的节点
template <typename T, size_t ALLOC_UNIT = BASE_ALLOCATOR_UNIT>
class zPage {
        using Elem = std::decay_t<T>;
public:
        
private:
        Elem* _mem;
        std::bitset<ALLOC_UNIT / sizeof(Elem)> __bm;
};





// storage中为page的链表，当存储空间不足时链接新的page到链表
// 分为空链、未满链与满链
template <typename T>
class zStorage {
        
public:
        class iterator : public std::random_access_iterator_tag {
        public:
                iterator() = default;
        private:
                
        };

        


private:
        std::unique_ptr<zPage<T>> __list;
        size_t __total_size, __total_capacity;

        struct weak_list {
                zPage<T>* page {nullptr};
                weak_list* next {nullptr};
                weak_list* fromt {nullptr};
        };
        weak_list empty_head, half_head, full_head;

        // 从当前链上移除节点
        void __w_remove_from_list(weak_list& node) noexcept;
        // 将node链接到list链上
        void __w_link_list(weak_list& list, weak_list& node) noexcept;
};


#endif