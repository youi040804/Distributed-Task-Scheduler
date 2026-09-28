#include <cstdlib>
#include <iostream>
#include <string>

#include "client/Client.h"

namespace {

void printUsage(const char* program) {
    std::cout
        << "Usage:\n"
        << "  Submit task:\n"
        << "    " << program
        << " --master <ip:port>"
        << " --priority <priority>"
        << " --payload <text>\n"
        << "\n"
        << "  Query task:\n"
        << "    " << program
        << " --master <ip:port>"
        << " --query <task_id>\n";
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

    if (!parseInt(
            endpoint.substr(pos + 1).c_str(),
            port)) {
        return false;
    }

    return port > 0 && port <= 65535;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string master_ip;
    int master_port = 0;

    int priority = 0;
    bool has_priority = false;

    std::string payload;
    
    int query_task_id = 0;
    bool has_query = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--master" &&
            i + 1 < argc) {

            if (!parseEndpoint(
                    argv[++i],
                    master_ip,
                    master_port)) {
                std::cerr
                    << "Invalid master endpoint\n";
                return EXIT_FAILURE;
            }

        } else if (arg == "--priority" &&
                   i + 1 < argc) {

            if (!parseInt(
                    argv[++i],
                    priority)) {
                std::cerr
                    << "Invalid priority\n";
                return EXIT_FAILURE;
            }

            has_priority = true;

        } else if (arg == "--payload" &&
                   i + 1 < argc) {

            payload = argv[++i];

        } else if (arg == "--query" &&
           i + 1 < argc) {

            if (!parseInt(argv[++i],query_task_id) ||query_task_id <= 0) {

                std::cerr<< "Invalid task id\n";
                return EXIT_FAILURE;
            }
            has_query = true;

        } else if (arg == "--help" ||
                   arg == "-h") {

            printUsage(argv[0]);
            return EXIT_SUCCESS;

        } else {

            printUsage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (master_ip.empty() ||master_port == 0) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    const bool submit_mode =has_priority && !payload.empty();
    const bool query_mode =has_query;

    // 必须二选一：submit 或 query
    if (submit_mode == query_mode) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    // 当前 Client ID 暂未参与任务调度逻辑，
    // 这里使用固定值作为客户端标识。
    dts::Client client(1);

    client.setMasterAddress(
        master_ip,
        master_port
    );

    if (!client.connectMaster()) {
        std::cerr
            << "Failed to connect to Master\n";
        return EXIT_FAILURE;
    }

    if (query_mode) {
        auto status =client.queryTask(query_task_id);

        if (!status.has_value()) {
            std::cerr<< "Failed to query task\n";

            client.stop();
            return EXIT_FAILURE;
        }

        if (!status->found) {
            std::cout
                << "[Client] task_id="
                << query_task_id
                << " not found"
                << std::endl;

            client.stop();
            return EXIT_SUCCESS;
        }

        std::cout
            << "[Client] task_id="
            << status->task_id
            << ", status=";

        switch (status->status) {
            case dts::TaskStatus::PENDING:
                std::cout << "PENDING";
                break;

            case dts::TaskStatus::RUNNING:
                std::cout << "RUNNING";
                break;

            case dts::TaskStatus::FAILED:
                std::cout << "FAILED";
                break;

            case dts::TaskStatus::DONE:
                std::cout << "DONE";
                break;
        }

        std::cout << std::endl;

        if (!status->result.empty()) {
            std::cout
                << "[Client] result="
                << status->result
                << std::endl;
        }

        client.stop();
        return EXIT_SUCCESS;
    }
    
    dts::TaskSubmitInfo task;

    task.priority = priority;
    task.payload = payload;


    auto task_id =client.submitTask(task);

    if (!task_id.has_value()) {
        std::cerr<< "Failed to submit task\n";

        client.stop();
        return EXIT_FAILURE;
    }

    std::cout
        << "[Client] task submitted successfully"
        << ", task_id="
        << *task_id
        << std::endl;
    client.stop();

    return EXIT_SUCCESS;
}