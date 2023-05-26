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
  esp32animations::Core1Metrics core1Metrics;
  if(xQueueReceive(m_core1_metrics_queue, &core1Metrics, 0) == pdFALSE) {
    return;
  }

  metricsPoint.clearFields();
  // Report RSSI of currently connected network
  metricsPoint.addField("rssi", WiFi.RSSI());
  metricsPoint.addField("uptime", millis());
  metricsPoint.addField("free heap", esp_get_free_heap_size());
  metricsPoint.addField("total frames", core1Metrics.totalFrames);
  metricsPoint.addField("max render ms", core1Metrics.maxFrameRenderTime);
  metricsPoint.addField("num effects rendered", core1Metrics.numEffectsRendered);

  // Print what are we exactly writing
  // Serial.print("Writing: ");
  // Serial.println(point.toLineProtocol());

  if (!influxClient.writePoint(metricsPoint))
  {
    Serial.print("InfluxDB write failed: ");
    Serial.println(influxClient.getLastErrorMessage());
  }
}
