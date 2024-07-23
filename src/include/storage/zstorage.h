
// 以页为单位，位图slot存储数据，存入以后内存不再改变

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

#pragma region zSlot

// is_clone_type<cls>::value : true(yes), false(no)
template <typename T>
struct is_clone_type {
private:
        typedef char __can;
        typedef int  __cant;

        template <typename cls, typename... TArgs, cls&(cls::*)(TArgs...)>
        struct macher;

        // check lval reference assignment
        template <typename cls>
        static __can check1(macher<cls, const cls&, &cls::operator=>*);
        // check operator=(cls&&) rval assignment
        template <typename cls>
        static __can check2(macher<cls, cls&&, &cls::operator=>*);

        template <typename cls>
        template __cant check_other(...);

public:
        static constexpr bool value = !(sizeof(macher<T>(nullptr)) == sizeof(__cant));
};

template <typename T>
concept CloneType = requires(T __case) {
        // T __new_case = std::move(__case);
        is_clone_type<T>::value;
};

template <typename T> requires CloneType<T>
class zSlot {
        using Elem = std::decay_t<T>;
public:
        template <typename Y>
        static zSlot new_slot(void* mem, Y&& e, zSlot* next = nullptr, zSlot* front = nullptr) noexcept {
                // 确保传入的类型与slot中存储类型一致
                static_assert(std::is_same_v<Elem, std::decay_t< decltype(e) >>);
                zSlot* __slot = (zSlot*)mem;
                Elem* __elem = __slot + (size_t)( (char*)( &((zPage*)0)->elem ) );

                // 将传入的e赋值拷贝到目标slot中
                __elem->operator=(std::forward<decltype(e)>(e));
                __slot->next = next;
                __slot->front = front;
        }

        zSlot* next_slot() noexcept {
                return next;
        }
        zSlot* front_slot() noexcept {
                return front;
        }

        void set_next(zSlot* __next) noexcept {
                if (this->next != nullptr) {
                        __next->set_next(this->next);
                }

                next = __next;
                __next->set_front(this);
        }

        void set_front(zSlot* __front) noexcept {
                if (this->front != nullptr) {
                        __front->set_front(this->front);
                }

                front = __front;
                __front->set_next(this);
        }
private:
        Elem elem;

        zSlot* next;    // next slot
        zSlot* front;   // front slot
};


#pragma region zPage

// allocate unit is 4096 (4KB) for 32bit system, 8KB for 64bit system
constexpr size_t BASE_ALLOCATOR_UNIT = 4096;

// page中包括多个slot，每个slot存储一个T，位图表示当前slot是否存在T
// T被包装为双向链表，rbt用于加速查找T，指向链表中的节点
template <typename T, size_t ALLOC_UNIT = BASE_ALLOCATOR_UNIT>
class zPage {
        using Elem = std::decay_t<T>;
public:
        // random accessable iterator for page
        class iterator : public std::random_access_iterator_tag {
        public:

        private:
                zPage* __parent;
                // how long for each slot
                const size_t __slot_len;
                // count of slot index
                const size_t __index;

                iterator() = delete;
                friend zPage<T, ALLOC_UNIT>;
        };

        // get an allocated and usable page
        static zPage new_page();

        // get next page of this
        zPage* next_page() noexcept;
        // get front page, return nullptr if front is none
        zPage* front_page() noexcept;

        // get 1 count in __bm. 1 represents that slot is occupied
        size_t size() const noexcept;
private:
        void* _mem; // Elem* or zPage* in tail
        std::bitset<ALLOC_UNIT / sizeof(Elem)> __bm;

        // 链接前后两页
        zPage* next  {nullptr};
        zPage* front {nullptr};

        // mmap分配页内存，初始化页
        zPage();
};




















































#pragma region zStorage

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