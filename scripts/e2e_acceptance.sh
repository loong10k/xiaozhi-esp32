#!/usr/bin/env bash
# =============================================================================
# scripts/e2e_acceptance.sh — xiaozhi-esp32 端到端延迟验收（计划 §8）
#
# 前置条件：
#   1. ESP32-S3 开发板经 USB 接入（出现 /dev/cu.usb*）
#   2. xiaozhi.bin 已构建（idf.py build）
#   3. 设备端已在关键节点接入 E2EProfiler 埋点（agent_hosting/e2e_profiler.h）
#
# 执行：flash → monitor 抓取 → 提取 [E2E] 时间轴 → 判定验收门
#   验收门（2026-09-10-xiaozhi-integration-migration-plan.md §8.1）：
#     speaker_first_audio total < 500ms 为 PASS（P3 目标档）
#
# 用法：
#   bash scripts/e2e_acceptance.sh [monitor_seconds]   # 默认 60
# =============================================================================
set -u

PORT=""
MONITOR_SECONDS="${1:-60}"
GATE_MS=500          # 验收门：首声延迟 < 500ms
LOGFILE="/tmp/xiaozhi_e2e_monitor.log"
RESULTFILE="/tmp/xiaozhi_e2e_result.txt"

# ---- 0. 环境自检 ------------------------------------------------------------
if [ -z "${IDF_PATH:-}" ]; then
    export IDF_PATH="$HOME/esp/esp-idf"
fi
if [ ! -f "$IDF_PATH/export.sh" ]; then
    echo "[FAIL] ESP-IDF not found at $IDF_PATH"; exit 1
fi
# shellcheck disable=SC1091
. "$IDF_PATH/export.sh" > /dev/null 2>&1 || { echo "[FAIL] export.sh failed"; exit 1; }

# ---- 1. 等待串口设备 --------------------------------------------------------
echo "==> waiting for USB serial device (/dev/cu.usb*)..."
for i in $(seq 1 15); do
    PORT=$(ls /dev/cu.usbmodem* /dev/cu.usbserial* /dev/cu.SLAB* /dev/cu.wch* 2>/dev/null | head -1)
    [ -n "$PORT" ] && break
    sleep 2
done
if [ -z "$PORT" ]; then
    echo "[FAIL] no ESP32 serial device found — plug the board in and retry."
    exit 2
fi
echo "==> device: $PORT"

# ---- 2. 烧录 ---------------------------------------------------------------
echo "==> flashing..."
idf.py -p "$PORT" flash || { echo "[FAIL] flash failed"; exit 3; }

# ---- 3. 抓取 monitor 输出 ---------------------------------------------------
echo "==> monitoring for ${MONITOR_SECONDS}s — speak to the device now!"
# 用 idf.py monitor 的原始串口读取替代（避免交互式 TTY）：
# 直接读串口，波特率与 sdkconfig 一致（CONFIG_ESP_CONSOLE_UART_DEFAULT=115200）
BAUD=115200
timeout "$MONITOR_SECONDS" python3 - "$PORT" "$BAUD" > "$LOGFILE" <<'PYEOF' 2>/dev/null
import serial, sys
port, baud = sys.argv[1], int(sys.argv[2])
ser = serial.Serial(port, baud, timeout=1)
while True:
    line = ser.readline()
    if line:
        sys.stdout.write(line.decode('utf-8', errors='replace'))
        sys.stdout.flush()
PYEOF
# pyserial 不存在时回退：直接用 cat 读 tty（raw 模式）
if [ ! -s "$LOGFILE" ]; then
    stty -f "$PORT" "$BAUD" 2>/dev/null
    timeout "$MONITOR_SECONDS" cat "$PORT" > "$LOGFILE" 2>/dev/null
fi

# ---- 4. 判定 ----------------------------------------------------------------
echo "==> E2E timeline extracted:"
grep "\[E2E\]" "$LOGFILE" | tail -20 | tee "$RESULTFILE"

FIRST_AUDIO_MS=$(grep "speaker_first_audio" "$LOGFILE" | tail -1 \
    | grep -oE "total [0-9]+ms" | grep -oE "[0-9]+")

echo ""
if [ -z "$FIRST_AUDIO_MS" ]; then
    echo "[FAIL] no speaker_first_audio checkpoint captured —"
    echo "       check E2EProfiler wiring or speak after 'T0 reset' appears."
    exit 4
fi

echo "==> speaker_first_audio total = ${FIRST_AUDIO_MS}ms (gate: <${GATE_MS}ms)"
if [ "$FIRST_AUDIO_MS" -lt "$GATE_MS" ]; then
    echo "[PASS] E2E latency acceptance PASSED (<${GATE_MS}ms gate)"
    exit 0
else
    echo "[FAIL] E2E latency ${FIRST_AUDIO_MS}ms >= ${GATE_MS}ms gate"
    exit 5
fi
