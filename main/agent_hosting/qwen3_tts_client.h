/*
 * qwen3_tts_client.h — 阿里云 DashScope Qwen3-TTS 实时 API WebSocket 客户端
 *
 * 目的：把云端 LLM 的流式文本（text delta）实时合成 opus 音频，
 *       替换 xiaozhi 原有"云端整段 TTS"路径（TTFA 从 ~500-1500ms 降到 ~97ms）。
 *
 * 选型依据：docs/superpowers/plans/2026-09-10-tts-tech-decision.md
 *   - Qwen3-TTS 实时 API：TTFA ~97ms，单字符流式输入，opus @ 12kHz 输出
 *   - endpoint: wss://dashscope.aliyuncs.com/api/v1/realtime
 *   - 鉴权: HTTP Header "Authorization: Bearer <DASHSCOPE_API_KEY>"
 *
 * 对应 agentscope-cpp：
 *   - agentscope/core/event/text_chunker.h::TextChunker（文本切片）
 *   - agentscope/core/middleware/stream_first_chunk_middleware.h（切片中间件）
 *
 * 依赖：xiaozhi 现有 WebSocket 封装（esp-ml307 web_socket.h）+ cJSON。
 */
#ifndef AGENT_HOSTING_QWEN3_TTS_CLIENT_H_
#define AGENT_HOSTING_QWEN3_TTS_CLIENT_H_

#include <functional>
#include <memory>
#include <string>

#include <cJSON.h>

class WebSocket;

namespace xiaozhi {

// ---------------------------------------------------------------------------
// Qwen3TtsClient — DashScope Qwen3-TTS Realtime API 客户端
//
// 生命周期：
//   1. 应用启动 / 唤醒时 Connect() 建立长连接（复用 xiaozhi ConnectionPool 思路）
//   2. ReAct 输出流式文本时，每个句末 chunk 调 SendTextDelta()
//   3. 音频通过 on_opus_frame 回调返回（12kHz opus，交 xiaozhi 解码播放）
//   4. 一轮流式结束调 Finish()；跨轮次连接保持不断
// ---------------------------------------------------------------------------
class Qwen3TtsClient {
public:
    // 音频回调：DashScope 下行的 opus 帧（12kHz）。
    // payload 为帧数据（不含协议头），调用方负责 resample / 播放。
    using OpusFrameCallback =
        std::function<void(const uint8_t* payload, size_t len)>;

    struct Config {
        std::string api_key;             // DASHSCOPE_API_KEY
        std::string model = "qwen3-tts-12hz-0.6b-customvoice";  // 低延迟首选
        std::string voice = "Cherry";    // 内置音色
        std::string uri =
            "wss://dashscope.aliyuncs.com/api/v1/realtime";
        int connect_id = 2;              // 独立 connect_id，避开主协议(1)与TTS冲突
    };

    explicit Qwen3TtsClient(Config cfg);
    ~Qwen3TtsClient();

    // 禁拷贝（持有 WebSocket 长连接）
    Qwen3TtsClient(const Qwen3TtsClient&) = delete;
    Qwen3TtsClient& operator=(const Qwen3TtsClient&) = delete;

    // 建立长连接并发送 session.create / session.update。
    // 返回 false 表示连接或握手失败（调用方应降级到备用 TTS）。
    bool Connect();

    // 连接是否可用
    bool IsConnected() const;

    // 流式送入一个句末文本 chunk（由 StreamFirstChunkBridge 切片后调用）
    bool SendTextDelta(const std::string& text);

    // 标记本轮文本流结束，等待尾部音频
    bool Finish();

    // 关闭连接
    void Close();

    // 注册 opus 帧回调（必须在 Connect 前调用）
    void OnOpusFrame(OpusFrameCallback cb);

    // 周期性调用（application 主循环）驱动下行数据接收
    void Poll();

private:
    // 发送一个 JSON 控制帧（文本 WebSocket 帧）
    bool SendJson(const cJSON* json);

    // 处理下行 JSON 控制帧（session.created / response.audio.delta 等）
    void HandleTextFrame(const char* data, size_t len);

    // 处理下行二进制音频帧
    void HandleBinaryFrame(const char* data, size_t len);

    Config cfg_;
    std::unique_ptr<WebSocket> ws_;
    OpusFrameCallback on_opus_frame_;
    bool session_created_ = false;
    bool finished_ = true;
};

}  // namespace xiaozhi

#endif  // AGENT_HOSTING_QWEN3_TTS_CLIENT_H_
