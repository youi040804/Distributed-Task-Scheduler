#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>  
#include <chrono>  
#include <thread>  
#include "client/Client.h"
namespace {

void printUsage(const char* program) {
    std::cout
        << "Usage:\n"
        << "  " << program
        << " --master <ip:port>"
        << " --tasks <count>"
        << " --priority <priority>\n";
}

bool parseInt(const char* text, int& value) {
    try {
        value = std::stoi(text);
        return true;
    } catch (...) {
        return false;
    }
}

bool parseEndpoint( const std::string& endpoint, std::string& ip, int& port ){
    const auto pos = endpoint.rfind(':');

    if (pos == std::string::npos || pos == 0 || pos + 1 >= endpoint.size()) {
        return false;
    }

    ip = endpoint.substr(0, pos);

    if (!parseInt( endpoint.substr(pos + 1).c_str(), port)) {
        return false;
    }

    return port > 0 && port <= 65535;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string master_ip;
    int master_port = 0;

    int task_count = 0;
    int priority = 1;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--master" && i + 1 < argc) {
            if (!parseEndpoint( argv[++i], master_ip, master_port)) {

                std::cerr << "Invalid master endpoint\n";

                return EXIT_FAILURE;
            }

        } else if (arg == "--tasks" && i + 1 < argc) {

            if (!parseInt( argv[++i], task_count) || task_count <= 0) {

                std::cerr << "Invalid task count\n";

                return EXIT_FAILURE;
            }

        } else if (arg == "--priority" && i + 1 < argc) {

            if (!parseInt( argv[++i], priority)) {

                std::cerr << "Invalid priority\n";

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

    if (master_ip.empty() || master_port == 0 || task_count <= 0) {

        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    dts::Client client(1);

    client.setMasterAddress( master_ip, master_port );

    if (!client.connectMaster()) {
        std::cerr << "Failed to connect to Master\n";

        return EXIT_FAILURE;
    }

    std::cout
        << "[Benchmark] connected to Master\n"
        << "[Benchmark] tasks="
        << task_count
        << ", priority="
        << priority
        << std::endl;

    
    std::vector<int> task_ids;
    task_ids.reserve(task_count);

    const auto start_time = std::chrono::steady_clock::now();

    for (int i = 0; i < task_count; ++i) {
        dts::TaskSubmitInfo task;
        task.priority = priority;
        task.payload = "benchmark-task-" + std::to_string(i + 1);

        auto task_id = client.submitTask(task);

        if (!task_id.has_value()) {
            std::cerr << "[Benchmark] failed to submit task " << (i + 1) << std::endl;
            client.stop();
            return EXIT_FAILURE;
        }

        task_ids.push_back(*task_id);
    }

    std::cout << "[Benchmark] submitted " << task_ids.size() << " tasks" << std::endl;
    

    std::vector<bool> finished(task_ids.size(), false);

    int completed_count = 0;
    int failed_count = 0;

    while (completed_count + failed_count < task_count) {

        for (std::size_t i = 0; i < task_ids.size(); ++i) {

            // 已经进入终态的任务不再重复查询
            if (finished[i]) {
                continue;
            }

            auto status = client.queryTask(task_ids[i]);

            if (!status.has_value()) {
                std::cerr << "[Benchmark] failed to query task " << task_ids[i] << std::endl;

                client.stop();
                return EXIT_FAILURE;
            }

            if (!status->found) {
                std::cerr << "[Benchmark] task " << task_ids[i] << " not found" << std::endl;

                client.stop();
                return EXIT_FAILURE;
            }

            if (status->status == dts::TaskStatus::DONE) {
                finished[i] = true;
                ++completed_count;

            } else if (
                status->status == dts::TaskStatus::FAILED) {

                finished[i] = true;
                ++failed_count;
            }
        }

        if (completed_count + failed_count < task_count) {
            std::this_thread::sleep_for( std::chrono::milliseconds(100) );
        }
    }

    const auto end_time = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end_time - start_time;

    const double elapsed_seconds = elapsed.count();

    const double throughput = static_cast<double>(completed_count) / elapsed_seconds;
    
  
    std::cout << "[Benchmark] completed=" << completed_count
                << ", failed=" << failed_count << std::endl;

    std::cout << "[Benchmark] elapsed=" << elapsed_seconds 
                << " s" << std::endl;

    std::cout << "[Benchmark] throughput=" << throughput 
                << " tasks/s" << std::endl;
    client.stop();

    return EXIT_SUCCESS;
}