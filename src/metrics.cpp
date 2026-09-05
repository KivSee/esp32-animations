#include "metrics.h"

#include <esp_heap_caps.h>

#include "core1_metrics.h"

#define INFLUXDB_URL "http://" INFLUXDB_IP ":" INFLUXDB_PORT
#define INFLUXDB_DB_NAME "kivsee"

Metrics::Metrics(QueueHandle_t core1_metrics_queue)
    : influxClient(INFLUXDB_URL, INFLUXDB_DB_NAME),
      metricsPoint("metrics"),
      m_core1_metrics_queue(core1_metrics_queue)
{
}

void Metrics::setTaskHandles(TaskHandle_t monitorTask, TaskHandle_t renderTask)
{
  m_monitor_task = monitorTask;
  m_render_task = renderTask;
}

// One-shot benchmark: how much slower is PSRAM than internal RAM on this
// board? Answers empirically whether moving render data to PSRAM would cost
// anything. Sequential read/write via memcpy is the best case for PSRAM
// (the cache prefetches nicely); random access on scattered small objects --
// which is what the effect list actually looks like -- is considerably worse
// than the ratio measured here.
void Metrics::reportMemoryBandwidth()
{
  const size_t BENCH_BYTES = 64 * 1024;
  const int BENCH_ITERATIONS = 8;

  uint8_t *internalBuf = (uint8_t *)heap_caps_malloc(BENCH_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  uint8_t *psramBuf = (uint8_t *)heap_caps_malloc(BENCH_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  uint8_t *scratch = (uint8_t *)heap_caps_malloc(BENCH_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

  if (internalBuf == nullptr || scratch == nullptr)
  {
    Serial.println(F("[0] bandwidth probe: could not allocate internal buffers, skipping"));
  }
  else if (psramBuf == nullptr)
  {
    Serial.println(F("[0] bandwidth probe: could not allocate PSRAM buffer, skipping"));
  }
  else
  {
    memset(internalBuf, 0xA5, BENCH_BYTES);
    memset(psramBuf, 0xA5, BENCH_BYTES);

    unsigned long start = micros();
    for (int i = 0; i < BENCH_ITERATIONS; i++)
    {
      memcpy(scratch, internalBuf, BENCH_BYTES);
    }
    unsigned long internalUs = micros() - start;

    start = micros();
    for (int i = 0; i < BENCH_ITERATIONS; i++)
    {
      memcpy(scratch, psramBuf, BENCH_BYTES);
    }
    unsigned long psramUs = micros() - start;

    const float totalKb = (float)(BENCH_BYTES * BENCH_ITERATIONS) / 1024.0f;
    Serial.printf("[0] internal read: %lu us for %.0f KB (%.1f MB/s)\n",
                  internalUs, totalKb,
                  internalUs ? totalKb / 1024.0f / ((float)internalUs / 1000000.0f) : 0.0f);
    Serial.printf("[0] psram    read: %lu us for %.0f KB (%.1f MB/s)\n",
                  psramUs, totalKb,
                  psramUs ? totalKb / 1024.0f / ((float)psramUs / 1000000.0f) : 0.0f);
    Serial.printf("[0] psram is %.2fx slower than internal (sequential)\n",
                  internalUs ? (float)psramUs / (float)internalUs : 0.0f);
  }

  heap_caps_free(internalBuf);
  heap_caps_free(psramBuf);
  heap_caps_free(scratch);
}

void Metrics::setup(const char *thingName)
{
  metricsPoint.addTag("device", thingName);

  // One-shot boot report: tells us whether PSRAM was found and mapped at all.
  // If "psram total" is 0 here, PSRAM is not active and every later psram
  // number will be 0 too -- check the board's memory_type / BOARD_HAS_PSRAM.
  Serial.println(F("[0] ---- memory at boot ----"));
  Serial.print(F("[0] internal total bytes: "));
  Serial.println(heap_caps_get_total_size(MALLOC_CAP_INTERNAL));
  Serial.print(F("[0] internal free bytes:  "));
  Serial.println(heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  Serial.print(F("[0] psram total bytes:    "));
  Serial.println(heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
  Serial.print(F("[0] psram free bytes:     "));
  Serial.println(heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  // Allocations smaller than this go to internal RAM regardless of how much
  // PSRAM is free -- the reason small per-effect objects never land in PSRAM.
  Serial.print(F("[0] malloc-always-internal threshold: "));
#ifdef CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL
  Serial.println(CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL);
#else
  Serial.println(F("n/a"));
#endif
  reportMemoryBandwidth();
  Serial.println(F("[0] -------------------------"));
}

void Metrics::loop()
{
  esp32animations::Core1Metrics core1Metrics;
  if(xQueueReceive(m_core1_metrics_queue, &core1Metrics, 0) == pdFALSE) {
    return;
  }

  // Split the heap by capability. esp_get_free_heap_size() sums internal +
  // PSRAM, which would mask exactly the thing we want to see: whether the
  // animation allocations moved off internal RAM and onto PSRAM.
  const uint32_t internalFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  const uint32_t internalMinFree = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
  const uint32_t internalLargestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  const uint32_t psramTotal = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  const uint32_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  const uint32_t psramMinFree = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);

  // Averages over the report window. framesInWindow can be 0 if the window
  // elapsed without a single frame, so guard the division.
  const unsigned int frames = core1Metrics.framesInWindow;
  const unsigned long avgRenderUs = frames ? (unsigned long)(core1Metrics.sumRenderUs / frames) : 0;
  const unsigned long avgShowUs = frames ? (unsigned long)(core1Metrics.sumShowUs / frames) : 0;
  // frames observed over the reporting interval -> frames per second
  const float fps = (float)frames * 1000.0f / (float)METRICS_REPORT_INTERVAL_MS;

  Serial.println(F("[0] ---- metrics ----"));
  Serial.printf("[0] uptime ms: %lu  rssi: %d\n", millis(), WiFi.RSSI());
  Serial.printf("[0] fps: %.1f  frames in window: %u  total frames: %u\n",
                fps, frames, core1Metrics.totalFrames);
  Serial.printf("[0] render us: avg %lu / max %lu   (effects: %u)\n",
                avgRenderUs, core1Metrics.maxRenderUs, core1Metrics.numEffectsRendered);
  Serial.printf("[0] show   us: avg %lu / max %lu\n",
                avgShowUs, core1Metrics.maxShowUs);
  Serial.printf("[0] frame  us: avg %lu (budget %lu at current fps)\n",
                avgRenderUs + avgShowUs,
                frames ? (unsigned long)(1000000.0f / fps) : 0UL);
  Serial.printf("[0] internal heap: free %u  min-ever %u  largest block %u\n",
                internalFree, internalMinFree, internalLargestBlock);

  // Smallest amount of stack headroom each task has ever had, in bytes.
  // This only ever decreases. Approaching 0 means a stack overflow (a crash,
  // not a slowdown) -- raise that task's stack size if it gets close.
  const uint32_t monitorStackFree = m_monitor_task
                                        ? uxTaskGetStackHighWaterMark(m_monitor_task) * sizeof(StackType_t)
                                        : 0;
  const uint32_t renderStackFree = m_render_task
                                       ? uxTaskGetStackHighWaterMark(m_render_task) * sizeof(StackType_t)
                                       : 0;
  Serial.printf("[0] stack headroom min-ever: monitor %u b  render %u b\n",
                monitorStackFree, renderStackFree);
  if (psramTotal > 0)
  {
    Serial.printf("[0] psram: free %u / %u  min-ever %u  (used %u)\n",
                  psramFree, psramTotal, psramMinFree, psramTotal - psramFree);
  }
  else
  {
    Serial.println(F("[0] psram: NOT AVAILABLE"));
  }
  Serial.println(F("[0] -----------------"));

  metricsPoint.clearFields();
  // Report RSSI of currently connected network
  metricsPoint.addField("rssi", WiFi.RSSI());
  metricsPoint.addField("uptime", millis());
  metricsPoint.addField("free heap", esp_get_free_heap_size());
  metricsPoint.addField("total frames", core1Metrics.totalFrames);
  metricsPoint.addField("max render ms", core1Metrics.maxFrameRenderTime);
  metricsPoint.addField("num effects rendered", core1Metrics.numEffectsRendered);
  metricsPoint.addField("fps", fps);
  metricsPoint.addField("avg render us", (unsigned long)avgRenderUs);
  metricsPoint.addField("max render us", core1Metrics.maxRenderUs);
  metricsPoint.addField("avg show us", (unsigned long)avgShowUs);
  metricsPoint.addField("max show us", core1Metrics.maxShowUs);
  metricsPoint.addField("internal free heap", internalFree);
  metricsPoint.addField("internal min free heap", internalMinFree);
  metricsPoint.addField("internal largest block", internalLargestBlock);
  metricsPoint.addField("psram free", psramFree);
  metricsPoint.addField("psram min free", psramMinFree);
  metricsPoint.addField("monitor stack free", monitorStackFree);
  metricsPoint.addField("render stack free", renderStackFree);

#ifdef METRICS_INFLUXDB_ENABLED
  if (!influxClient.writePoint(metricsPoint))
  {
    Serial.print("InfluxDB write failed: ");
    Serial.println(influxClient.getLastErrorMessage());
  }
#endif // METRICS_INFLUXDB_ENABLED
}
