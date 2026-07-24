/*
 * Worker.cpp
 * Worker 类的实现
 */
#include <arpa/inet.h> 
#include <cstring> 
#include"../../include/worker/Worker.h"

namespace dts{

    Worker::Worker(int worker_id):worker_id_(worker_id),worker_client_(nullptr){
        memset(&master_addr_,0,sizeof(master_addr_));
        master_addr_.sin_family=AF_INET;

    }
    void Worker::setMasterAddress(const std::string&master_ip,int master_port){
    master_addr_.sin_port=htons(master_port);
        if (inet_pton(AF_INET, master_ip.c_str(), &master_addr_.sin_addr) != 1) {
            perror("Invalid IP address");
            return ;
        }

    }
    bool Worker::connectMaster()
    {
        worker_client_=std::make_unique<dts::TCPClient>(
            inet_ntoa(master_addr_.sin_addr),
            ntohs(master_addr_.sin_port)
        );

        return worker_client_->connect();    
    }

    bool Worker::registerToMaster(std::string&data){
        Connection* conn=worker_client_->getConnection();
        return conn->send(data);
    }

    bool Worker::start(int worker_id,std::string&data,
        const std::string&master_ip,int master_port){
        Worker* worker=new Worker(worker_id);
        worker->setMasterAddress(master_ip,master_port);
        int connect_result=worker->connectMaster();
        int register_result=worker->registerToMaster(data);

        if(!connect_result ||!register_result ){
            perror("worker start failed");
            return false;
        }
        return true;
    }

}