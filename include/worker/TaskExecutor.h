/*
*TaskExecutor.h
*/
#pragma once
#include"common/Protocol.h"

namespace dts{
class TaskExecutor{
private:

public:
TaskResultInfo execute(const TaskAssignInfo&task);

};
}