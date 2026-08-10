/*
 * Config.h
 * 系统配置常量
 */
#pragma once

namespace dts {

    // 心跳间隔（秒）：Worker 每隔多久发送一次心跳
    constexpr int HEARTBEAT_INTERVAL = 3;

    // 心跳检测间隔（秒）：Master 每隔多久检查一次超时
    constexpr int HEARTBEAT_CHECK_INTERVAL = 3;

    // 心跳超时阈值（秒）：超过此时间未收到心跳，视为死亡
    constexpr int HEARTBEAT_TIMEOUT = 10;
    
    //默认最大重试次数
    constexpr int MAX_TASK_RETRY = 3;

}