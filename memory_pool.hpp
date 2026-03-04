#ifndef MEMORY_POOL_HPP
#define MEMORY_POOL_HPP

#include <iostream>
#include <vector>
#include <queue>
#include <cstdint>
#include <type_traits>
#include <ranges>


template <typename T, size_t BLOCK_SIZE = 4096>
class MemoryPool {
  private:
    union Slot {
        T data;         // 使用时存用户数据
        Slot* next{};   // 未使用时存下一个空闲位置
    };

    static_assert(std::is_standard_layout_v<T>, "Type must be standard layout");        // T 必须能放入 union
    static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");  // T 必须可平凡复制
    static_assert(alignof(Slot) >= alignof(T), "Type alignment too large");             // T 的对齐不能太宽，否则严重降低性能
    static_assert(BLOCK_SIZE >= sizeof(Slot), "BLOCK_SIZE too small");                  // 一个 block 至少要容得下一个 slot

    static constexpr size_t SLOTS_PER_BLOCK = BLOCK_SIZE / sizeof(Slot);

    struct Block {
        void* begin{};          // 起始地址
    };

    std::vector<Block> blocks;
    Slot* free_head{};          // 第一个空闲 slot


    // 扩容，申请一个新块
    void allocate_new_block() {
        void* new_block = ::operator new(BLOCK_SIZE);  // 明确指定全局分配器，避免调用 class/namespace 重载的版本
        blocks.push_back({new_block});

        // 从后往前将 slot 依次加到链表头前面
        Slot* begin = reinterpret_cast<Slot*>(new_block), * end = begin + SLOTS_PER_BLOCK - 1;
        for (Slot* cur = end; cur >= begin; --cur) {
            cur->next = free_head;
            free_head = cur;
        }
    }


  public:
    MemoryPool() : free_head(nullptr) { allocate_new_block(); }

    ~MemoryPool() { for (auto& block : blocks) ::operator delete(block.begin); }


    // 分配内存
    T* allocate() {
        // 没有空闲 slot 了就新申请一块
        if (free_head == nullptr) allocate_new_block();

        Slot* slot = free_head;
        free_head = slot->next;

        return reinterpret_cast<T*>(slot);
    }

    
    // 释放内存
    void deallocate(T* ptr) {
        if (!ptr) return;
        ptr->~T();                 // 释放前先析构

        Slot* slot = reinterpret_cast<Slot*>(ptr);
        // 需要用户自己避免重复释放
        
        slot->next = free_head;     // 插回空闲链表
        free_head = slot;
    }
};

#endif  // MEMORY_POOL_HPP