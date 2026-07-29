/*
 * Worker.cpp
 * Worker 类的实现，包含连接 Master、注册自身等逻辑
 */
#include <arpa/inet.h> 
#include <cstring> 
#include"../../include/worker/Worker.h"
 #include "../../include/common/Message.h"
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

    //由原来的发送字符串改为发送真正的Message
    bool Worker::registerToMaster(Message&msg){

        Connection* conn=worker_client_->getConnection();
        return conn->sendMessage(msg);
    }

    bool Worker::start(Message&msg,const std::string&master_ip,int master_port){
        this->setMasterAddress(master_ip,master_port);
        int connect_result=this->connectMaster();
        if(!connect_result) {
            perror("worker connect failed!");
            return false;
        }
        int register_result=this->registerToMaster(msg);

        if(!register_result){
            perror("worker start failed!");
            return false;
        }
        return true;
    }


}