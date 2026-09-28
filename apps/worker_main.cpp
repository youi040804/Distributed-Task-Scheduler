#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>

#include "worker/Worker.h"

namespace {

void printUsage(const char* program) {
    std::cout
        << "Usage: " << program
        << " --id <worker-id>"
        << " --master <ip:port>"
        << " [--threads <count>]"
        << " [--worker-ip <ip>]"
        << " [--worker-port <port>]\n";
}

bool parseInt(const char* text, int& value) {
    try {
        value = std::stoi(text);
        return true;
    } catch (...) {
        return false;
    }
}

bool parseEndpoint(
    const std::string& endpoint,
    std::string& ip,
    int& port
) {
    const auto pos = endpoint.rfind(':');

    if (pos == std::string::npos ||
        pos == 0 ||
        pos + 1 >= endpoint.size()) {
        return false;
    }

    ip = endpoint.substr(0, pos);

    if (!parseInt(endpoint.substr(pos + 1).c_str(), port)) {
        return false;
    }

    return port > 0 && port <= 65535;
}

}  // namespace

int main(int argc, char* argv[]) {
    int worker_id = -1;

    std::string master_ip;
    int master_port = 0;
    
    int executor_threads = 1;
    // 当前 Worker 并不会监听来自 Master 的反向连接，
    // 这里的 IP/port 主要作为注册信息保留。
    std::string worker_ip = "127.0.0.1";
    int worker_port = 0;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--id" && i + 1 < argc) {
            if (!parseInt(argv[++i], worker_id) ||
                worker_id < 0) {
                std::cerr << "Invalid worker id\n";
                return EXIT_FAILURE;
            }

        } else if (arg == "--master" && i + 1 < argc) {
            if (!parseEndpoint(
                    argv[++i],
                    master_ip,
                    master_port)) {
                std::cerr << "Invalid master endpoint\n";
                return EXIT_FAILURE;
            }

        } else if (arg == "--worker-ip" &&
                   i + 1 < argc) {
            worker_ip = argv[++i];

        } else if (arg == "--worker-port" &&
                   i + 1 < argc) {
            if (!parseInt(argv[++i], worker_port) ||
                worker_port < 0 ||
                worker_port > 65535) {
                std::cerr << "Invalid worker port\n";
                return EXIT_FAILURE;
            }

        } else if (arg == "--threads" &&
                    i + 1 < argc) {

            if (!parseInt(
                    argv[++i],
                    executor_threads) ||
                executor_threads <= 0) {

                std::cerr
                    << "Invalid thread count\n";
                return EXIT_FAILURE;
            }
        } else if (arg == "--help" ||
                   arg == "-h") {
            printUsage(argv[0]);
            return EXIT_SUCCESS;

        } else {
            printUsage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (worker_id < 0 ||
        master_ip.empty() ||
        master_port == 0) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    // 与 Master CLI 一样：
    // 主线程同步等待 SIGINT / SIGTERM。
    sigset_t signals;

    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);

    pthread_sigmask(
        SIG_BLOCK,
        &signals,
        nullptr
    );

    dts::Worker worker(worker_id,static_cast<size_t>(executor_threads));
    
    if (!worker.start(
            master_ip,
            master_port,
            worker_ip,
            worker_port)) {
        std::cerr << "Failed to start Worker\n";
        return EXIT_FAILURE;
    }

    std::cout
        << "[Worker " << worker_id
        << "] connected to "
        << master_ip
        << ":"
        << master_port
        << ", executor_threads="
        << executor_threads
        << std::endl;
    
    int signal_number = 0;

    sigwait(
        &signals,
        &signal_number
    );

    std::cout
        << "\n[Worker " << worker_id
        << "] shutdown signal received"
        << std::endl;

    worker.stop();

    return EXIT_SUCCESS;
}