#include <cstdlib>
#include <iostream>
#include <string>

#include "client/Client.h"

namespace {

void printUsage(const char* program) {
    std::cout
        << "Usage: " << program
        << " --master <ip:port>"
        << " --priority <priority>"
        << " --payload <text>\n";
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

        } else if (arg == "--help" ||
                   arg == "-h") {

            printUsage(argv[0]);
            return EXIT_SUCCESS;

        } else {

            printUsage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (master_ip.empty() ||
        master_port == 0 ||
        !has_priority ||
        payload.empty()) {

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