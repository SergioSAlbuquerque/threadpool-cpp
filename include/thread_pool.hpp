#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// Pool de threads com fila de tarefas.
//
// - N threads "trabalhadoras" ficam dormindo numa condition_variable.
// - submit() coloca uma tarefa na fila, acorda uma thread e devolve
//   um std::future para o chamador pegar o resultado depois.
// - O destrutor termina as tarefas pendentes e faz join em todas as
//   threads (RAII: ninguém precisa lembrar de "desligar" o pool).
class ThreadPool {
public:
    explicit ThreadPool(std::size_t num_threads = std::thread::hardware_concurrency()) {
        if (num_threads == 0) num_threads = 1;
        workers_.reserve(num_threads);
        for (std::size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] { worker_loop(); });
        }
    }

    // Não faz sentido copiar um pool de threads.
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_) t.join();
    }

    // Aceita qualquer "callable" + argumentos e devolve future<tipo_de_retorno>.
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>> {
        using R = std::invoke_result_t<F, Args...>;

        // packaged_task não é copiável, mas std::function exige cópia;
        // por isso embrulhamos num shared_ptr.
        auto task = std::make_shared<std::packaged_task<R()>>(
            [fn = std::forward<F>(f),
             tup = std::make_tuple(std::forward<Args>(args)...)]() mutable {
                return std::apply(std::move(fn), std::move(tup));
            });

        std::future<R> result = task->get_future();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) throw std::runtime_error("submit on stopped ThreadPool");
            tasks_.emplace([task] { (*task)(); });
        }
        cv_.notify_one();
        return result;
    }

    std::size_t size() const noexcept { return workers_.size(); }

private:
    void worker_loop() {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                // O predicado protege contra "spurious wakeups".
                cv_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
                if (stopping_ && tasks_.empty()) return;
                job = std::move(tasks_.front());
                tasks_.pop();
            }
            job();  // executa FORA do lock, senão as threads ficam em fila
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopping_ = false;
};
