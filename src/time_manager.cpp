
#include <time_manager.h>

#include <Arduino.h>
#include <time.h>

TimeManager::TimeManager(QueueHandle_t epoch_time_update_queue)
    : m_epoch_time_update_queue(epoch_time_update_queue)
{
    m_timesync.updateConfiguration(15, 1000 * 60 * 10, 250, 1000 * 60 * 2);
}

void TimeManager::begin() {
    setenv("TZ", "IST-2IDT,M3.4.4,M10.5.0", 1);
    tzset();

    IPAddress ntpServerIp;
    Serial.print("Time sync server IP: ");
    Serial.println(TIME_SERVER_IP);
    ntpServerIp.fromString(TIME_SERVER_IP);
    m_timesync.setup(ntpServerIp, 12321);
}

void TimeManager::loop()
{
    bool isTimeChanged, isFirstClockUpdate;
    m_timesync.loop(&isTimeChanged, &isFirstClockUpdate);
    if (isFirstClockUpdate)
    {
        Serial.println("TIME IS NOW VALID. the esp clock was not valid and now it is");
    }
    else if (isTimeChanged)
    {
        Serial.println("TIME CHANGED. new synced clock is available to the esp");
    }

    if (isTimeChanged || isFirstClockUpdate)
    {
        int64_t espStartTime = m_timesync.getEspStartTimeMs();

        time_t now_s = (time_t)((espStartTime + (int64_t)millis()) / 1000);
        struct tm t;
        localtime_r(&now_s, &t);
        char timebuf[32];
        strftime(timebuf, sizeof(timebuf), "%H:%M:%S", &t);
        Serial.print("Synced time: ");
        Serial.println(timebuf);

        xQueueSend(m_epoch_time_update_queue, &espStartTime, portMAX_DELAY);
    }
}
