# AutoSplitLeveler

一個用於 Cubase 的 VST3 自動音量整平外掛，只需在「直達離線處理 (DOP)」套用，即可自動把人聲音軌各字、各句的音量平滑整平至目標響度。

---

## 🎯 功能說明

- **自動增益控制 (AGC)**：以 50ms 滑動視窗即時分析 RMS 響度，自動對齊至目標音量 **-18 dBFS**。
- **無聲保護**：偵測到低於 -45 dBFS 的靜音段時，停止出增益，避免放大底噪或呼吸聲。
- **平滑壓縮**：10ms 快速 Attack 處理突發大音量，200ms 慢速 Release 避免語句間產生抽吸感。
- **雙向整平**：太大聲的字被壓下來，太小聲的字被捧起來，整體對話音量更一致。
- **離線處理**：採用 VST3 Direct Offline Processing (DOP)，對音檔本體不可逆地套用增益，不佔用 CPU 即時資源。

---

## 📦 安裝方式

### Windows

1. 下載 `AutoSplitLeveler_Windows.zip`（從本頁 Releases 或 Actions 建置成品）。
2. 解壓縮，取得 `AutoSplitLeveler.vst3` 資料夾。
3. 將整個資料夾複製至：

   ```
   C:\Program Files\Common Files\VST3\
   ```

4. 重啟 Cubase，在外掛管理員掃描後即可使用。

### macOS

1. 從本 Repo 的 **Actions** 頁面，選最新一筆成功的 `Build macOS VST3` Job，在 Artifacts 裡下載 `AutoSplitLeveler_macOS.zip`。
2. 解壓縮，取得 `AutoSplitLeveler.vst3` 資料夾。
3. 將整個資料夾複製至：

   ```
   /Library/Audio/Plug-Ins/VST3/
   ```

4. 重啟 Cubase，在外掛管理員掃描後即可使用。

> **注意**：macOS 版本為 Universal Binary，同時支援 Intel 及 Apple Silicon (M1/M2/M3)。

---

## 🚀 使用方式（Cubase）

1. 在音軌上選取要整平的**人聲音訊片段**。
2. 按 `F7` 打開「**直達離線處理 (Direct Offline Processing)**」視窗。
3. 點「**+ 插件**」，找到並載入 `AutoSplitLeveler`（在 Dynamics 類別下）。
4. 確認上方「**自動應用**」已勾選，外掛載入後即自動執行並完成整平。
5. 聽聽看波形是否被自動壓縮/提升，確認效果後關閉 DOP 視窗即可。

---

## 🔨 從原始碼自行建置

### Windows（Visual Studio 2022 + CMake）

```powershell
git clone --recurse-submodules https://github.com/Chester930/AutoSplitLeveler.git
cd AutoSplitLeveler
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target AutoSplitLeveler
```

編譯完成後，輸出路徑為：

```
build\VST3\Release\AutoSplitLeveler.vst3\
```

### macOS（CI 自動化）

只要推送程式碼至 `master` 分支，GitHub Actions 會自動在 Mac 雲端伺服器上建置 Universal Binary，並上傳為 Artifact 供下載。

---

## 📋 技術規格

| 項目 | 規格 |
|------|------|
| 格式 | VST3 |
| 類別 | Fx / Dynamics |
| 目標響度 | -18 dBFS RMS |
| 靜音閾值 | -45 dBFS |
| RMS 視窗 | 50 ms |
| Attack Time | 10 ms |
| Release Time | 200 ms |
| 最大增益 | ±12 dB |
| 支援平台 | Windows x64, macOS Universal (arm64 + x86_64) |
| 最低 macOS 版本 | 11.0 (Big Sur) |

---

## 📁 專案結構

```
AutoSplitLeveler/
├── .github/workflows/
│   └── build_mac.yml        # macOS 自動建置腳本
├── source/
│   ├── ARA/                 # 主要外掛邏輯
│   │   ├── VST3/            # VST3 音訊處理核心 (TestVST3Processor.cpp)
│   │   └── TestPlugInConfig.h  # 外掛名稱與 ID 設定
│   ├── AutoSplitLevelerAlgorithm.cpp  # 核心演算法
│   └── ...
├── ARA_SDK/                 # ARA SDK (Git Submodule)
├── vst3sdk/                 # VST3 SDK (Git Submodule)
├── AutoSplitLeveler_Windows/ # 預編譯 Windows 版
└── CMakeLists.txt
```

---

## 📄 授權

本專案基於 Apache License 2.0 開源 SDK 建置，僅供個人學習與內部使用。
