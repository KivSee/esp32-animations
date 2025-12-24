#include "metrics.h"

#include "core1_metrics.h"

#define INFLUXDB_URL "http://" INFLUXDB_IP ":" INFLUXDB_PORT
#define INFLUXDB_DB_NAME "kivsee"

Metrics::Metrics(QueueHandle_t core1_metrics_queue)
    : influxClient(INFLUXDB_URL, INFLUXDB_DB_NAME),
      metricsPoint("metrics"),
      m_core1_metrics_queue(core1_metrics_queue)
{
}

void Metrics::setup(const char *thingName)
{
  metricsPoint.addTag("device", thingName);
}

void Metrics::loop()
{
  // InfluxDB reporting is disabled on this build (ESP32-C3 / single-core),
  // to avoid blocking network I/O from the main loop. Metrics are still
  // collected by the renderer and placed on the queue but processing/writing
  // is intentionally no-op here.
  (void)m_core1_metrics_queue;
  return;
}
