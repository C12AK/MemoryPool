// g++-13 -std=c++23 -o ftest -Wall -Wextra functional_test.cpp

#include "memory_pool.hpp"

#include <iostream>
#include <cstring>
#include <cassert>
#include <vector>
#include <format>


// 一个满足 "standard layout" 和 "trivially copyable" 的结构体
struct TestStruct {
    int i;
    double d;
    char ch[16];

    bool operator==(const TestStruct& other) const {
        return i == other.i && d == other.d && !strcmp(ch, other.ch);
    }
};


// 1. 基础分配和释放
void test_basic_alloc_dealloc() {
    {
        MemoryPool<int> pool;

        int* p1 = pool.allocate(), * p2 = pool.allocate();
        assert(p1 != nullptr && p2 != nullptr);
        *p1 = 114, *p2 = 514;
        assert(*p1 == 114 && *p2 == 514);

        pool.deallocate(p1), pool.deallocate(p2);
        pool.deallocate(nullptr);
    }
    {
        MemoryPool<double> pool;

        double *p1 = pool.allocate(), * p2 = pool.allocate();
        assert(p1 != nullptr && p2 != nullptr);
        *p1 = 19.19, *p2 = 8.10;
        assert(*p1 == 19.19 && *p2 == 8.10);

        pool.deallocate(p1), pool.deallocate(p2);
    }

    std::cout << "Basic allocate & deallocate passed." << std::endl;
}


// 2. 块级扩容及清除
void test_multi_blocks() {
    MemoryPool<int, 256> pool;      // 小的 block 便于触发扩容
    constexpr size_t ALLOC_NUM = 1000;
    std::vector<int*> ptrs(ALLOC_NUM);
    
    for (size_t i = 0; i < ALLOC_NUM; ++i) {
        int* p = pool.allocate();
        assert(p != nullptr);
        *p = static_cast<int>(i);
        ptrs[i] = p;
    }
    for (size_t i = 0; i < ALLOC_NUM; ++i) assert(*ptrs[i] == static_cast<int>(i));

    for(int* p : ptrs) pool.deallocate(p);

    std::cout << "Multi-block allocate passed." << std::endl;
}


// 3. 空闲链表机制
void test_free_list() {
    MemoryPool<int> pool;
    int* p1 = pool.allocate(), * p2 = pool.allocate();
    *p1 = 1, *p2 = 2;
    pool.deallocate(p1), pool.deallocate(p2);

    int* p3 = pool.allocate(), * p4 = pool.allocate();  // 应复用刚刚释放的 slot ，3 复用 2 ，4 复用 1
    assert(p3 == p2 && p4 == p1);

    pool.deallocate(p3), pool.deallocate(p4);
    std::cout << "Free list passed." << std::endl;
}


// 4. 支持复杂类型
void test_struct() {
    MemoryPool<TestStruct> pool;

    TestStruct* ptr = pool.allocate();
    assert(ptr != nullptr);

    *ptr = {0x0d000721, 0.721, "hello hell"};
    assert(*ptr == TestStruct(0x0d000721, 0.721, "hello hell"));

    pool.deallocate(ptr);

    std::cout << "Struct passed." << std::endl;
}


// 主函数：执行所有测试
int main() {
    try {
        test_basic_alloc_dealloc();
        test_multi_blocks();
        test_free_list();
        test_struct();
        std::cout << "Done." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << std::format("Test failed with exception: {}\n", e.what());
        return 1;
    } catch (...) {
        std::cerr << "Test failed with UKE\n";
        return 1;
    }
}