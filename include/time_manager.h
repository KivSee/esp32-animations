#ifndef __TIME_MANAGER_H__
#define __TIME_MANAGER_H__

#include <TimeSync.hpp>
#include <freertos/queue.h>
#include "queue_manager.h"

class TimeManager
{

public:
    TimeManager(const QueueManager &queueManager);
    void begin();
    void loop();

private:
    TimeSync::TimeSyncClient m_timesync;
    QueueHandle_t m_epoch_time_update_queue;

};

#endif //__TIME_MANAGER_H__