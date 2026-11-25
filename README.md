# threadpool-cpp

Pool de threads **header-only** em **C++17**: envie qualquer função para ser executada em paralelo e receba o resultado via `std::future`.

```cpp
#include "thread_pool.hpp"

ThreadPool pool(4);                       // 4 threads trabalhadoras

auto f = pool.submit([](int a, int b) { return a + b; }, 2, 3);
std::cout << f.get();                     // 5
```

## Benchmark

Contagem de números primos até N, sequencial × paralelo (`examples/benchmark.cpp`):

```
$ ./build/benchmark 3000000
N = 3000000, threads = 2
Sequencial: 216816 primos em 514.3 ms
Paralelo:   216816 primos em 295.6 ms
Speedup:    1.74x
```

O intervalo é dividido em `8 × nº de threads` blocos. Isso equilibra a carga, já que testar números grandes demora mais que testar os pequenos. Com mais núcleos, o speedup cresce.

## Como funciona

```
submit(f, args...) ──► [ fila de tarefas ] ──► thread 1 ─┐
        │                (mutex + cv)    ──► thread 2 ─┼─► resultado no future
        └──► devolve std::future<R>      ──► thread N ─┘
```

1. As threads esperam numa `std::condition_variable` até haver tarefa ou o pool ser encerrado.
2. `submit` embrulha a função num `std::packaged_task`, que liga a execução ao `std::future` devolvido.
3. A tarefa roda **fora** do lock, para as outras threads continuarem pegando trabalho.
4. Se a tarefa lançar exceção, ela é repassada para quem chamar `future.get()`.
5. O destrutor termina as tarefas pendentes e faz `join` em todas as threads (**RAII**).

## Conceitos de C++ usados

- `std::thread`, `std::mutex`, `std::condition_variable`, `std::lock_guard` / `std::unique_lock`
- `std::future`, `std::packaged_task`, propagação de exceções entre threads
- **Variadic templates**, *perfect forwarding* (`std::forward`), `std::invoke_result_t`, `std::apply`
- Suporte a argumentos *move-only* (ex.: `std::unique_ptr`)
- Classe não copiável (`= delete`) e `std::shared_ptr` para dividir a posse da tarefa
- Testes verificados com **ThreadSanitizer**, sem condições de corrida

## Compilar e rodar

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/benchmark 5000000 4    # N e número de threads (opcional)
ctest --test-dir build

# Checar data races:
cmake -S . -B build-tsan -DCMAKE_CXX_FLAGS="-fsanitize=thread -g"
cmake --build build-tsan && ctest --test-dir build-tsan
```

## Possíveis melhorias

- *Work stealing* (uma fila por thread)
- Prioridade de tarefas
- `wait_all()` sem destruir o pool
