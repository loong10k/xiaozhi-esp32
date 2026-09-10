/*
 * e2e_profiler.h — 端到端延迟埋点（计划 §8 验收门的数据采集器）
 *
 * 目的：在语音链路关键节点插入 esp_timer 时间戳，串联成完整时间轴，
 *       供 scripts/e2e_acceptance.sh 抓取并判定 <500ms 验收门。
 *
 * 输出协议（串口 log，一行一个检查点）：
 *   [E2E] vad_endpoint: +48ms (total 48ms)
 *   [E2E] speaker_first_audio: +297ms (total 345ms)
 *
 * 检查点序列（对齐 deep-latency-optimization.md §8）：
 *   user_speech_end → vad_endpoint → asr_final → llm_first_token
 *   → tts_first_chunk → speaker_first_audio → full_response_complete
 *
 * 接入点（建议）：
 *   - user_speech_end       application.cc VAD 检测到说话结束处
 *   - vad_endpoint          DualThresholdVad::on_partial_endpoint 回调
 *   - asr_final             协议层收到 ASR final 处
 *   - llm_first_token       首个 TextBlockDeltaEvent 处
 *   - tts_first_chunk       Qwen3TtsClient 首个 opus 帧回调处
 *   - speaker_first_audio   AudioService 首帧起播处
 *   - full_response_complete AGENT_END 事件处
 *
 * 线程安全：使用 FreeRTOS 临界区（消费端为主任务单线程，开销可忽略）。
 */
#ifndef AGENT_HOSTING_E2E_PROFILER_H_
#define AGENT_HOSTING_E2E_PROFILER_H_

#include <cstdint>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

namespace xiaozhi {

class E2EProfiler {
public:
    static E2EProfiler& GetInstance() {
        static E2EProfiler instance;
        return instance;
    }

    // 新一轮对话起点（user_speech_end 时调用）
    void reset() {
        portENTER_CRITICAL(&mux_);
        t0_us_ = esp_timer_get_time();
        last_us_ = t0_us_;
        active_ = true;
        portEXIT_CRITICAL(&mux_);
        ESP_LOGI(TAG, "[E2E] === new turn, T0 reset ===");
    }

    // 在检查点打点（任意线程安全）
    void mark(const char* checkpoint) {
        portENTER_CRITICAL(&mux_);
        if (!active_) {
            portEXIT_CRITICAL(&mux_);
            return;  // 未开始新一轮，忽略
        }
        int64_t now = esp_timer_get_time();
        int64_t delta_us = now - last_us_;
        int64_t total_us = now - t0_us_;
        last_us_ = now;
        portEXIT_CRITICAL(&mux_);
        // 固定前缀 [E2E] 供 scripts/e2e_acceptance.sh grep
        ESP_LOGI(TAG, "[E2E] %s: +%lldms (total %lldms)",
                 checkpoint,
                 static_cast<long long>(delta_us / 1000),
                 static_cast<long long>(total_us / 1000));
    }

private:
    E2EProfiler() = default;
    static constexpr const char* TAG = "E2E";
    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
    int64_t t0_us_ = 0;
    int64_t last_us_ = 0;
    bool active_ = false;
};

}  // namespace xiaozhi

#endif  // AGENT_HOSTING_E2E_PROFILER_H_
