
#include <thread>
#include <atomic>

#include <cstdlib>
#include <cstring>
#include <chrono>
#include <iostream>
#include <vector>




const int iterations_per_thread = 250000;  // Total iterations = num_threads * iterations_per_thread

void flush_cache() {
    size_t cache_size = 16 * 1024; // Adjust this based on your architecture's cache size
    void* buffer = std::malloc(cache_size);

    if (buffer) {
        std::memset(buffer, 0, cache_size);
        __builtin___clear_cache(reinterpret_cast<char*>(buffer), reinterpret_cast<char*>(buffer) + cache_size);
        std::free(buffer);
    }
}

void __attribute__((optimize("-O0"))) benchmark_fa_vs_wr(){
    const int iterations = 10000000;
    std::atomic<int> atomicVar(0);
    int nonAtomicVar = 0;

    auto start_time_atomic = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        atomicVar.fetch_add(1, std::memory_order_relaxed);
    }
    auto end_time_atomic = std::chrono::high_resolution_clock::now();
    auto duration_atomic = std::chrono::duration_cast<std::chrono::microseconds>(end_time_atomic - start_time_atomic);

    auto start_time_non_atomic = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        nonAtomicVar = 1;
    }
    auto end_time_non_atomic = std::chrono::high_resolution_clock::now();
    auto duration_non_atomic = std::chrono::duration_cast<std::chrono::microseconds>(end_time_non_atomic - start_time_non_atomic);

    std::cout << "Atomic Fetch-and-Add: " << duration_atomic.count() << " microseconds\n";
    std::cout << "Non-Atomic Write: " << duration_non_atomic.count() << " microseconds\n";
}


// Benchmark function for fetch-and-add on a single address
void benchmarkSingleAddress(std::atomic<int>& atomicVar) {
    auto start_time = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations_per_thread; ++i) {
        atomicVar.fetch_add(1, std::memory_order_relaxed);
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    std::cout << "Single Address: " << duration.count() << " microseconds\n";
}

// Benchmark function for fetch-and-add on different addresses
void benchmarkMultipleAddresses(std::vector<std::atomic<int>>& atomicVars) {
    auto start_time = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations_per_thread; ++i) {
        for (auto& atomicVar : atomicVars) {
            atomicVar.fetch_add(1, std::memory_order_relaxed);
        }
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    std::cout << "Multiple Addresses: " << duration.count() << " microseconds\n";
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <num_threads>\n";
        return 1;
    }

    int num_threads = std::atoi(argv[1]);
    if (num_threads <= 0) {
        std::cerr << "Invalid number of threads\n";
        return 1;
    }
    benchmark_fa_vs_wr();

    std::vector<std::atomic<int>> atomicVars(num_threads);

    // Create threads for single address benchmark
    std::vector<std::thread> singleAddressThreads;
    for (int i = 0; i < num_threads; ++i) {
        singleAddressThreads.emplace_back(benchmarkSingleAddress, std::ref(atomicVars[0]));
    }

    // Join threads
    for (auto& thread : singleAddressThreads) {
        thread.join();
    }

    // Create threads for multiple addresses benchmark
    std::vector<std::thread> multipleAddressesThreads;
    for (int i = 0; i < num_threads; ++i) {
        multipleAddressesThreads.emplace_back(benchmarkMultipleAddresses, std::ref(atomicVars));
    }

    // Join threads
    for (auto& thread : multipleAddressesThreads) {
        thread.join();
    }

    return 0;
}