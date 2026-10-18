#ifndef __METRICS_H__
#define __METRICS_H__

#include <InfluxDbClient.h>

class Metrics {

    public:
        Metrics(QueueHandle_t core1_metrics_queue);
        void setup(const char *thingName);
        void loop();

        // Task stacks are allocated at task creation from internal RAM and can
        // never move to PSRAM, so they need watching independently of the heap.
        // Either handle may be null, in which case it is skipped.
        void setTaskHandles(TaskHandle_t monitorTask, TaskHandle_t renderTask);

    private:
        // one-shot benchmark comparing internal RAM vs PSRAM throughput
        void reportMemoryBandwidth();

    private:
        InfluxDBClient influxClient;
        Point metricsPoint;

        QueueHandle_t m_core1_metrics_queue;
        TaskHandle_t m_monitor_task = nullptr;
        TaskHandle_t m_render_task = nullptr;
};

#endif // __METRICS_H__
