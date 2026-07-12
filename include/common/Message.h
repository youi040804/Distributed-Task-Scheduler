#pragma once

#include<iostream>
#include<string>
enum class MessageType{
//Client
SUBMIT_TASK,
//Worker
REGISTER_WORKER,
HEARTBEAT,
TASK_RESULT,
//Master
TASK_ASSIGN
};

struct Message{
MessageType type;
std::string data;

};