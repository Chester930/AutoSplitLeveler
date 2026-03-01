"""
AutoSplitLeveler - Audio Processing Script (Python 3.13 compatible)
====================================================================
使用方式：
    python process_audio.py <input_audio_file>

需要安裝：
    pip install soundfile numpy

輸出：
    <原始檔名>_processed.wav  ← 同長度，每段音量已調整，直接覆蓋原檔
    <原始檔名>_report.txt     ← 各段切點與 Gain 調整報告
"""

import sys
import os
import shutil
import numpy as np
import soundfile as sf

SILENCE_THRESH_DB   = -45.0   # 低於此值視為靜音 (dB)
MACRO_SILENCE_MS    = 1000    # 大段落分辨時長：1 秒以上靜音才視為「換段落」
MICRO_SILENCE_MS    = 150     # 小波峰分辨時長：0.15 秒以上的字間停頓視為「同段落內的不同波峰」
TARGET_PEAK_DB      = -3.0    # 每個大段落要對齊的目標最大波峰
MAX_INTRA_DROP_DB   = 6.0     # 同段落內，各小波峰距最大波峰的「最大允許落差」(例如：不得低於 -9dB)
FADE_MS             = 20      # 切點淡入淡出長度 (ms)
# ────────────────────────────────────────────────────────────

def peak_dbfs(samples: np.ndarray) -> float:
    if len(samples) == 0:
        return -100.0
    peak = np.max(np.abs(samples))
    if peak < 1e-10:
        return -100.0
    return 20.0 * np.log10(peak)


def find_segments(mono: np.ndarray, sr: int, min_silence_ms: int) -> list[tuple[int, int]]:
    thresh = 10 ** (SILENCE_THRESH_DB / 20.0)
    min_sil = int(min_silence_ms * sr / 1000)

    is_silent = np.abs(mono) < thresh
    segments = []
    in_speech = False
    speech_start = 0
    silence_count = 0

    for i, silent in enumerate(is_silent):
        if not silent:
            if not in_speech:
                in_speech = True
                speech_start = i
            silence_count = 0
        else:
            if in_speech:
                silence_count += 1
                if silence_count >= min_sil:
                    segments.append((speech_start, i - silence_count))
                    in_speech = False
                    silence_count = 0

    if in_speech:
        segments.append((speech_start, len(mono)))

    return segments


def apply_fade(seg: np.ndarray, sr: int) -> np.ndarray:
    fade_len = min(int(FADE_MS * sr / 1000), len(seg) // 4)
    if fade_len == 0:
        return seg
    result = seg.copy()
    result[:fade_len] *= np.linspace(0, 1, fade_len)
    result[-fade_len:] *= np.linspace(1, 0, fade_len)
    return result


def process(input_path: str):
    print(f"讀取：{input_path}")
    data, sr = sf.read(input_path, dtype='float32')  # shape: (samples,) or (samples, channels)

    stereo = data.ndim == 2
    if stereo:
        mono = data.mean(axis=1)
    else:
        mono = data.copy()

    print(f"取樣率：{sr} Hz | 聲道：{'立體聲' if stereo else '單聲道'} | 長度：{len(mono)/sr:.2f}s")

    macro_segments = find_segments(mono, sr, MACRO_SILENCE_MS)
    print(f"\n找到 {len(macro_segments)} 個大段落\n")

    out = data.copy()

    report_lines = [
        "AutoSplitLeveler 報告 (波峰包絡限制版)",
        f"輸入：{input_path}",
        f"大段落數：{len(macro_segments)}",
        f"目標 Peak：{TARGET_PEAK_DB:.1f} dBFS | 段內最大落差：{MAX_INTRA_DROP_DB:.1f} dB",
        "─" * 75,
        f"{'大段':>4}  {'開始':>10}  {'結束':>10}   {'原段Peak':>9}  {'大段Gain':>9}",
    ]

    for i, (m_start, m_end) in enumerate(macro_segments):
        macro_mono = mono[m_start:m_end]
        macro_peak = peak_dbfs(macro_mono)
        
        # 1. 調整整個大段落，讓最大波峰等於 TARGET_PEAK_DB (-3.0)
        macro_gain_db = TARGET_PEAK_DB - macro_peak
        macro_gain = 10 ** (macro_gain_db / 20.0)
        
        # 先把這個大段落整體放大 (粗校準)
        if stereo:
            out[m_start:m_end, 0] *= macro_gain
            out[m_start:m_end, 1] *= macro_gain
        else:
            out[m_start:m_end] *= macro_gain
            
        t_s, t_e = m_start / sr, m_end / sr
        report_lines.append(f"{i+1:>4}  {t_s:>9.3f}s  {t_e:>9.3f}s   {macro_peak:>6.1f} dB  {macro_gain_db:>+6.1f} dB")
        print(f"大段 {i+1:>3}: {t_s:7.2f}s ~ {t_e:7.2f}s | 原 Peak={macro_peak:5.1f} | 整體 Gain={macro_gain_db:+5.1f}")
        
        # 2. 尋找這個大段落裡面的「小波峰 / 字詞」(微校準)
        # 注意：我們要在 "已放大過的" 區間找微小段落
        processed_macro_mono = out[m_start:m_end, 0] if stereo else out[m_start:m_end]
        micro_segments = find_segments(processed_macro_mono, sr, MICRO_SILENCE_MS)
        
        # 目標下限：不允許任何小波峰低於 TARGET_PEAK_DB - MAX_INTRA_DROP_DB (例如 -3 - 6 = -9dBFS)
        min_allowed_peak = TARGET_PEAK_DB - MAX_INTRA_DROP_DB
        
        micro_adjusted_count = 0
        for (u_start, u_end) in micro_segments:
            # 相對於整份檔案的位置
            abs_start = m_start + u_start
            abs_end = m_start + u_end
            
            micro_mono = processed_macro_mono[u_start:u_end]
            micro_peak = peak_dbfs(micro_mono)
            
            # 如果這個詞的最高點，低於允許的最弱音量 (-9dB)，就把這個詞單獨拉上來！
            if micro_peak < min_allowed_peak and micro_peak > -90.0:
                extra_gain_db = min_allowed_peak - micro_peak
                extra_gain = 10 ** (extra_gain_db / 20.0)
                
                if stereo:
                    out[abs_start:abs_end, 0] = apply_fade(out[abs_start:abs_end, 0] * extra_gain, sr)
                    out[abs_start:abs_end, 1] = apply_fade(out[abs_start:abs_end, 1] * extra_gain, sr)
                else:
                    out[abs_start:abs_end] = apply_fade(out[abs_start:abs_end] * extra_gain, sr)
                    
                micro_adjusted_count += 1
                
        if micro_adjusted_count > 0:
            print(f"     └─ 內部拉提了 {micro_adjusted_count} 個過小的波峰，確保落差在 {MAX_INTRA_DROP_DB}dB 內")

    # clip to prevent clipping
    out = np.clip(out, -1.0, 1.0)

    base, _ = os.path.splitext(input_path)
    out_path = base + "_processed.wav"
    sf.write(out_path, out, sr)
    print(f"\n✅ 輸出：{out_path}")

    report_path = base + "_report.txt"
    with open(report_path, "w", encoding="utf-8") as f:
        f.write("\n".join(report_lines))
    print(f"📄 報告：{report_path}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("用法：python process_audio.py <音訊檔路徑>")
        sys.exit(1)
    process(sys.argv[1])
