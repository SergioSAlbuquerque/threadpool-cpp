// Benchmark: conta números primos até N de forma sequencial e
// paralela (dividindo o intervalo em blocos para o pool) e compara tempos.
#include "thread_pool.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool is_prime(std::uint64_t n) {
    if (n < 2) return false;
    if (n % 2 == 0) return n == 2;
    for (std::uint64_t d = 3; d * d <= n; d += 2)
        if (n % d == 0) return false;
    return true;
}

std::uint64_t count_primes(std::uint64_t begin, std::uint64_t end) {
    std::uint64_t count = 0;
    for (auto i = begin; i < end; ++i) count += is_prime(i);
    return count;
}

template <typename Fn>
double time_ms(Fn&& fn) {
    auto t0 = std::chrono::steady_clock::now();
    fn();
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

}  // namespace

int main(int argc, char* argv[]) {
    const std::uint64_t n = argc > 1 ? std::stoull(argv[1]) : 5'000'000;
    const std::size_t threads = argc > 2 ? std::stoul(argv[2])
                                         : std::thread::hardware_concurrency();

    std::uint64_t seq_result = 0;
    double seq_ms = time_ms([&] { seq_result = count_primes(0, n); });

    std::uint64_t par_result = 0;
    double par_ms = time_ms([&] {
        ThreadPool pool(threads);
        // Mais blocos que threads = melhor balanceamento
        // (números grandes demoram mais para testar).
        const std::uint64_t chunks = pool.size() * 8;
        const std::uint64_t step = (n + chunks - 1) / chunks;

        std::vector<std::future<std::uint64_t>> futures;
        for (std::uint64_t b = 0; b < n; b += step)
            futures.push_back(pool.submit(count_primes, b, std::min(b + step, n)));
        for (auto& f : futures) par_result += f.get();
    });

    std::cout << std::fixed << std::setprecision(1)
              << "N = " << n << ", threads = " << threads << "\n"
              << "Sequencial: " << seq_result << " primos em " << seq_ms << " ms\n"
              << "Paralelo:   " << par_result << " primos em " << par_ms << " ms\n"
              << "Speedup:    " << std::setprecision(2) << seq_ms / par_ms << "x\n";

    return seq_result == par_result ? 0 : 1;
}
