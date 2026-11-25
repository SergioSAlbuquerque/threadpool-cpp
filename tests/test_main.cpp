#include "thread_pool.hpp"

#include <atomic>
#include <iostream>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::cerr << __FILE__ << ':' << __LINE__ << " FALHOU: " #cond "\n"; \
            ++failures;                                                    \
        }                                                                  \
    } while (0)

static void test_returns_value() {
    ThreadPool pool(2);
    auto f = pool.submit([](int a, int b) { return a + b; }, 2, 3);
    CHECK(f.get() == 5);
}

static void test_many_tasks() {
    ThreadPool pool(4);
    std::vector<std::future<int>> fs;
    for (int i = 0; i < 1000; ++i) fs.push_back(pool.submit([i] { return i * i; }));
    long long sum = 0;
    for (auto& f : fs) sum += f.get();
    long long expected = 0;
    for (long long i = 0; i < 1000; ++i) expected += i * i;
    CHECK(sum == expected);
}

static void test_exception_propagates() {
    ThreadPool pool(1);
    auto f = pool.submit([]() -> int { throw std::runtime_error("boom"); });
    bool caught = false;
    try { f.get(); } catch (const std::runtime_error& e) { caught = std::string(e.what()) == "boom"; }
    CHECK(caught);
}

static void test_destructor_finishes_pending() {
    std::atomic<int> counter{0};
    {
        ThreadPool pool(2);
        for (int i = 0; i < 100; ++i) pool.submit([&counter] { counter.fetch_add(1); });
    }  // destrutor espera tudo terminar
    CHECK(counter.load() == 100);
}

static void test_void_task() {
    ThreadPool pool(2);
    std::atomic<bool> ran{false};
    pool.submit([&ran] { ran = true; }).get();
    CHECK(ran.load());
}

static void test_move_only_argument() {
    ThreadPool pool(1);
    auto ptr = std::make_unique<int>(42);
    auto f = pool.submit([](std::unique_ptr<int> p) { return *p; }, std::move(ptr));
    CHECK(f.get() == 42);
}

int main() {
    test_returns_value();
    test_many_tasks();
    test_exception_propagates();
    test_destructor_finishes_pending();
    test_void_task();
    test_move_only_argument();

    if (failures == 0) std::cout << "Todos os testes passaram.\n";
    return failures == 0 ? 0 : 1;
}
