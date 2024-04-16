#include <iostream>
#include <thread>
#include <vector>
#include <cstring>

void printHelloWorld(int threadId, const char* printingMethod) {
    // Using std::cout and std::endl
    if (std::strcmp(printingMethod, "cout") == 0) {
        std::cout << threadId << " Hello World!" << std::endl;
    }
    // Using printf and "\n"
    else if (std::strcmp(printingMethod, "printf") == 0) {
        printf("%d Hello World!\n", threadId);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " -n <number_of_threads> -t <printing_method>\n";
        return 1;
    }

    // Parse command-line arguments
    int numThreads = std::stoi(argv[2]);
    const char* printingMethod = argv[4];

    // Validate printing method
    if (std::strcmp(printingMethod, "cout") != 0 && std::strcmp(printingMethod, "printf") != 0) {
        std::cerr << "Invalid printing method. Use 'cout' or 'printf'\n";
        return 1;
    }

    // Create threads
    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(printHelloWorld, i, printingMethod);
    }

    // Join threads
    for (std::thread& t : threads) {
        t.join();
    }

    return 0;
}
