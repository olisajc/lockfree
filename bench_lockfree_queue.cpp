#include <benchmark/benchmark.h>
#include "lockfree_queue.hpp"



void BM_PUSH(benchmark::State& state){
    auto lq = new lock_free_queue<int>();
    
    for(auto _ : state){
        lock_free_bench::push(lq,100,14);
    };
};




BENCHMARK(BM_PUSH);

BENCHMARK_MAIN();


// g++ -o bench -I/opt/homebrew/include -L/opt/homebrew/lib -lbenchmark bench_lockfree_queue.cpp -std=c++20 -pthread