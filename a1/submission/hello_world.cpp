#include <iostream>
#include <thread>
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
    int numThreads = 0;
    const char* printingMethod = nullptr;

    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " -n <number_of_threads> -t <printing_method>\n";
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-n") == 0) {
            if (i + 1 < argc) {
                numThreads = std::stoi(argv[i + 1]);
                ++i; // Skip the next argument (value for -n)
            } else {
                std::cerr << "Missing value for -n\n";
                return 1;
            }
        } else if (std::strcmp(argv[i], "-t") == 0) {
            if (i + 1 < argc) {
                printingMethod = argv[i + 1];
                ++i; // Skip the next argument (value for -t)
            } else {
                std::cerr << "Missing value for -t\n";
                return 1;
            }
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            return 1;
        }
    }

    // Check if both -n and -t are provided
    if (numThreads == 0 || printingMethod == nullptr) {
        std::cerr << "Usage: " << argv[0] << " -n <number_of_threads> -t <printing_method>\n";
        return 1;
    }
    

    
    // Validate printing method
    if (std::strcmp(printingMethod, "cout") != 0 && std::strcmp(printingMethod, "printf") != 0) {
        std::cerr << "Invalid printing method. Use 'cout' or 'printf'\n";
        return 1;
    }

    // Create threads
    std::thread threads[numThreads];

    // Start threads
    for (int i = 0; i < numThreads; ++i) {
        threads[i] = std::thread(printHelloWorld, i, printingMethod);
    }

    // Join threads
    for (int i = 0; i < numThreads; ++i) {
        threads[i].join();
    }

    return 0;
}
