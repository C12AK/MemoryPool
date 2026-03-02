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
        size_t used_slots{};    // 已使用的 slot 数
    };

    std::vector<Block> blocks;
    Slot* free_head{};          // 第一个空闲 slot


    // 查找一个 slot 属于哪个 block
    Block* find_block_containing_slot(Slot* slot) {
        for (auto& block : blocks) {
            Slot* begin = static_cast<Slot*>(block.begin), * end = begin + SLOTS_PER_BLOCK - 1;
            if (slot >= begin && slot <= end) return &block;
        }
        return nullptr;
    }


    // 扩容，申请一个新块
    void allocate_new_block() {
        void* new_block = ::operator new(BLOCK_SIZE);  // 明确指定全局分配器，避免调用 class/namespace 重载的版本
        blocks.push_back({new_block, 0});

        // 从后往前将 slot 依次加到链表头前面
        Slot* begin = reinterpret_cast<Slot*>(new_block), * end = begin + SLOTS_PER_BLOCK - 1;
        for (Slot* cur = end; cur >= begin; --cur) {
            cur->next = free_head;
            free_head = cur;
        }
    }


    // 重建空闲 slot 链表。重建后顺序和原来相反
    inline void rebuild_freelist() {
        Slot* new_head = nullptr, * cur = free_head;
        while (cur) {
            Slot* next = cur->next;
            
            // 如果有 block 含这个 slot ，保留，否则直接跳过
            if (find_block_containing_slot(cur)) {
                cur->next = new_head;
                new_head = cur;
            }
            cur = next;
        }
        free_head = new_head;
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

        if (Block* block = find_block_containing_slot(slot)) ++block->used_slots;
        return reinterpret_cast<T*>(slot);
    }

    
    // 释放内存
    void deallocate(T* ptr) {
        if (!ptr) return;
        ptr->~T();                 // 释放前先析构

        Slot* slot = reinterpret_cast<Slot*>(ptr);
        if (Block* block = find_block_containing_slot(slot)) --block->used_slots;
        // 需要用户自己避免重复释放
        
        slot->next = free_head;     // 插回空闲链表
        free_head = slot;
    }


    // 用户主动调用，清除所有空 block
    void clear_blocks() {
        std::vector<size_t> to_remove;
        for (size_t i = 0; i < blocks.size(); ++i) {
            if (!blocks[i].used_slots) to_remove.push_back(i);
        }

        // 倒序遍历要删除的块，以免迭代器失效；先记录后释放，避免 rebuild_freelist() use-after-free
        std::vector<void*> begins;
        for (auto i : std::views::reverse(to_remove)) {
            begins.push_back(blocks[i].begin);
            blocks.erase(blocks.begin() + i);
        }
        
        rebuild_freelist();

        for(auto begin : begins) ::operator delete(begin);
    }
};

#endif  // MEMORY_POOL_HPP