/*
 * stream_first_chunk_bridge.h — 流式文本切片 → Qwen3-TTS 桥接器
 *
 * 目的：把 LLM 流式输出的 TextBlockDelta 按标点切片，每个句末 chunk
 *       立即送 Qwen3TtsClient 合成——实现"TTFT 与 TTS 并行"，
 *       端到端首声延迟从 ~1500ms 降到 ~350ms。
 *
 * 数据流：
 *   LLM text delta ──► agentscope::TextChunker（标点/长度切分）
 *                  ──► Qwen3TtsClient::SendTextDelta(chunk)
 *                  ──► DashScope 下行 opus（TTFA ~97ms）
 *                  ──► xiaozhi opus 解码 → DAC
 *
 * 切片策略与单元测试见 agentscope-cpp:
 *   include/agentscope/core/event/text_chunker.h
 *   tests/core/event/test_text_chunker.cc（10/10 通过）
 *
 * 对应 agentscope-cpp：
 *   include/agentscope/core/middleware/stream_first_chunk_middleware.h
 *   （本桥接器是中间件 TtsSink 回调在 xiaozhi 端的具体落地）
 */
#ifndef AGENT_HOSTING_STREAM_FIRST_CHUNK_BRIDGE_H_
#define AGENT_HOSTING_STREAM_FIRST_CHUNK_BRIDGE_H_

#include <memory>
#include <string>

#include "agentscope/core/event/text_chunker.h"
#include "qwen3_tts_client.h"

namespace xiaozhi {

class StreamFirstChunkBridge {
public:
    struct Config {
        // 单 chunk 最大字符数（超长强制切，防止单 chunk 拖慢 TTFA）
        size_t max_chunk_chars = 24;
        // 单 chunk 最小字符数（避免"啊"碎片）
        size_t min_chunk_chars = 1;
    };

    explicit StreamFirstChunkBridge(std::shared_ptr<Qwen3TtsClient> tts,
                                    Config cfg = {})
        : tts_(std::move(tts)) {
        chunker_.setMaxChunkChars(cfg.max_chunk_chars);
        chunker_.setMinChunkChars(cfg.min_chunk_chars);
    }

    // LLM 流式输出一个文本 delta 时调用。
    // 内部累积并按句末标点切片；切片完成即送 TTS（不等整段生成完）。
    void OnTextDelta(const std::string& delta) {
        if (delta.empty() || tts_ == nullptr) {
            return;
        }
        std::string chunk = chunker_.feed(delta);
        if (!chunk.empty()) {
            tts_->SendTextDelta(chunk);
        }
    }

    // LLM 一轮回复结束时调用：flush 残余文本 + 通知 TTS 文本流结束。
    void OnTurnEnd() {
        std::string remaining = chunker_.flush();
        if (!remaining.empty() && tts_ != nullptr) {
            tts_->SendTextDelta(remaining);
        }
        if (tts_ != nullptr) {
            tts_->Finish();
        }
    }

    // 用户打断（AbortSpeaking）时调用：丢弃残余文本，保留 TTS 已发内容。
    void OnInterrupt() {
        chunker_.reset();
    }

    // 供调试：当前未送出的累积字符数
    size_t PendingChars() const { return chunker_.pendingSize(); }

private:
    std::shared_ptr<Qwen3TtsClient> tts_;
    agentscope::TextChunker chunker_;
};

}  // namespace xiaozhi

#endif  // AGENT_HOSTING_STREAM_FIRST_CHUNK_BRIDGE_H_
