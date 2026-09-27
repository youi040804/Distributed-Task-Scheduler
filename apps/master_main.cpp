#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include "master/Master.h"

namespace {

void printUsage(const char* program) {
    std::cout
        << "Usage: " << program
        << " --port <port>\n";
}

bool parsePort(const char* text, int& port) {
    try {
        int value = std::stoi(text);

        if (value <= 0 || value > 65535) {
            return false;
        }

        port = value;
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    // 默认端口
    int port = 9000;

    // 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--port" && i + 1 < argc) {
            if (!parsePort(argv[++i], port)) {
                std::cerr << "Invalid port\n";
                return EXIT_FAILURE;
            }
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return EXIT_SUCCESS;
        } else {
            printUsage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    // 屏蔽 SIGINT / SIGTERM，
    // 后面统一通过 sigwait() 等待退出信号。
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);

    pthread_sigmask(SIG_BLOCK, &signals, nullptr);

    dts::Master master(port);

    if (!master.start()) {
        std::cerr << "Failed to start Master\n";
        return EXIT_FAILURE;
    }

    std::cout
        << "[Master] listening on port "
        << port
        << std::endl;

    // Master::run() 内部会阻塞等待连接，
    // 因此放到独立线程中运行。
    std::thread run_thread([&master]() {
        master.run();
    });

    // 主线程等待 Ctrl+C(SIGINT) 或 SIGTERM。
    int signal_number = 0;
    sigwait(&signals, &signal_number);

    std::cout
        << "\n[Master] shutdown signal received"
        << std::endl;

    master.stop();

    if (run_thread.joinable()) {
        run_thread.join();
    }

    return EXIT_SUCCESS;
}