#ifndef __METRICS_H__
#define __METRICS_H__

#include <InfluxDbClient.h>

class Metrics {

    public:
        Metrics(QueueHandle_t core1_metrics_queue);
        void setup(const char *thingName);
        void loop();

    private:
        InfluxDBClient influxClient;
        Point metricsPoint;

        QueueHandle_t m_core1_metrics_queue;
};

#endif // __METRICS_H__
