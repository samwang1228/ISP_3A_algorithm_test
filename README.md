# isp_3a_demo

這是一個簡化版 ISP 3A（Auto-Exposure / Auto-White-Balance / Auto-Focus）示範，包含兩種模式：

1. **Simulation mode**：不讀影像，用數學模型模擬 stats（RGB mean、luma、sharpness），跑 AE/AWB/AF 控制迴圈。
2. **Image mode（需 OpenCV）**：讀入照片，做「AE-like 亮度校正 + AWB-like 白平衡校正」，並提供 **可選的銳化**（作為 AF 在單張影像上的近似）。

> 注意：AE/AWB 在單張圖上是「後處理校正」；真正的 AF 需要改鏡頭位置並重新拍攝，單張圖只能用 sharpening 近似視覺效果。

## Build

### macOS 安裝 OpenCV（Image mode 用）

如果你想用照片輸入（image mode），建議用 Homebrew：

```bash
brew install opencv
```

如果安裝後 CMake 仍找不到 OpenCV，可用 `OpenCV_DIR` 指定（Homebrew 通常是 opencv4）：

```bash
cmake -S . -B build -DOpenCV_DIR="$(brew --prefix opencv)/lib/cmake/opencv4"
cmake --build build -j
```

若只想跑 simulation mode，沒有 OpenCV 也能編譯（會顯示警告並停用 image mode）。

在專案根目錄：

```bash
cmake -S . -B build
cmake --build build -j
```

產物會在：

- `build/isp_3a_demo`

## Run

### Image mode（讀照片做 AE/AWB + 可選銳化）

單張輸入輸出：

```bash
./build/isp_3a_demo --in input/input.jpg --out output.jpg --target-luma 0.18
```

兩張以上（重複 `--in`），輸出到資料夾（輸出檔名會自動加 `_3a.jpg`）：

```bash
mkdir -p out
./build/isp_3a_demo --in input/a.jpg --in input/b.jpg --out-dir out --target-luma 0.18
```

可選銳化（unsharp mask，作為 AF 在單張圖上的近似）：

```bash
./build/isp_3a_demo --in input/input.jpg --out output.jpg --sharpen 0.6 --sharpen-sigma 1.2
```

程式預設會印出估計到的參數（WB gains、exposureScale 等），可關閉：

```bash
./build/isp_3a_demo --in input/input.jpg --out output.jpg --no-print-params
```

如果照片很暗但畫面中有少量亮點（例如夜景路燈），預設的「高光保護」可能會限制提亮幅度。你可以放寬高光保護：

```bash
./build/isp_3a_demo --in input/input.jpg --out output.jpg \
	--target-luma 0.18 \
	--clip-protect-p 0.95 \
	--clip-protect-max 0.98
```

如果你想要「暗部提亮、亮部不爆」更自然，建議啟用 tone mapping（會先允許亮部進入 HDR 值域，再壓回可顯示範圍）：

```bash
./build/isp_3a_demo --in input/input.jpg --out output.jpg \
	--target-luma 0.18 \
	--tonemap --tonemap-white 4.0 \
	--clip-protect-max 4.0
```

說明：

- `--clip-protect-p`：用哪個 luma percentile 來做保護（數值越小越「寬鬆」）
- `--clip-protect-max`：該 percentile 允許的最大 luma（越大越「寬鬆」）

### 基本

```bash
./build/isp_3a_demo
```

### 指定幀數

```bash
./build/isp_3a_demo 120
```

### 指定亂數種子（讓輸出可重現）

```bash
./build/isp_3a_demo 120 --seed 42
```

### 以 CSV 輸出（方便丟到 Excel/gnuplot）

```bash
./build/isp_3a_demo 120 --csv
```

### 不輸出表頭

```bash
./build/isp_3a_demo 120 --csv --no-header
```

### Help

```bash
./build/isp_3a_demo --help
```

## 輸出欄位說明

- `exposure`：曝光時間（相對值）
- `gain`：整體增益（相對值）
- `wb_r, wb_g, wb_b`：白平衡增益
- `focus`：鏡頭位置（0..100）
- `mean_r, mean_g, mean_b`：**pre-WB** RGB mean（0..1）
- `luma`：**post-WB + clip** 後的 luma（0..1）
- `sharpness`：清晰度指標（任意單位）

## Image mode 內部做了什麼（簡述）

- 將輸入 sRGB 轉近似 linear
- **AWB-like**：用 gray-world 估計每通道增益，使 RGB mean 更接近
- **AE-like**：根據 post-WB mean luma 計算全域曝光縮放，並用 luma percentile 做高光保護（避免過曝爆掉）
- 套用增益與曝光後，轉回 sRGB 輸出
