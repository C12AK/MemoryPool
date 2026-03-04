// g++-13 -std=c++23 -O2 -DNDEBUG -o ptest performance_test.cpp

#include "memory_pool.hpp"

#include <chrono>
#include <iostream>
#include <vector>
#include <format>


struct TestStruct {
    int i;
    double d;
    char ch[16];
};


// 返回当前时间
inline auto now() { return std::chrono::high_resolution_clock::now(); }

// 返回时间间隔
inline double elapsed_ms(auto start, auto end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

// 防止编译器优化
inline void do_not_optimize(const void* p) {
    asm volatile("" : : "g"(p) : "memory");
}


// 1. 短生命周期交替分配和释放
inline void test_single_alloc_dealloc() {
    constexpr int ITER = 5e6;

    auto t1 = now();
    {
        MemoryPool<TestStruct> pool;
        for (int i = 0; i < ITER; ++i) {
            TestStruct* p = pool.allocate();
            p->i = i, p->d = i * 0.1;
            do_not_optimize(p);
            pool.deallocate(p);
        }
    }
    auto t2 = now();

    auto t3 = now();
    for (int i = 0; i < ITER; ++i) {
        TestStruct* p = new TestStruct;
        p->i = i, p->d = i * 0.1;
        do_not_optimize(p);
        delete p;
    }
    auto t4 = now();

    double pool_time = elapsed_ms(t1, t2), newdel_time = elapsed_ms(t3, t4);
    std::cout << std::format("{:.3f} ", newdel_time / pool_time);
}


// 2. 批量分配和释放
inline void test_batch_alloc_dealloc() {
    constexpr int ITER = 3000;

    auto t1 = now();
    {
        MemoryPool<TestStruct> pool;
        for (int i = 0; i < ITER; ++i) {
            std::vector<TestStruct*> batch;
            for (int j = 0; j < ITER + i; ++j) batch.push_back(pool.allocate());
            for (auto* p : batch) pool.deallocate(p);
        }
    }
    auto t2 = now();

    auto t3 = now();
    for (int i = 0; i < ITER; ++i) {
        std::vector<TestStruct*> batch;
        for (int j = 0; j < ITER + i; ++j) batch.push_back(new TestStruct);
        for (auto* p : batch) delete p;
    }
    auto t4 = now();

    double pool_time = elapsed_ms(t1, t2), newdel_time = elapsed_ms(t3, t4);
    std::cout << std::format("{:.3f}\n", newdel_time / pool_time);
}


// 主函数：调用测试。会输出两个数：单对象和批量的加速比，用于被 plot_perf.py 捕获
int main() {
    test_single_alloc_dealloc();
    test_batch_alloc_dealloc();
    return 0;
}