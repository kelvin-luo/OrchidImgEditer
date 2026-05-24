# kImgEdit 开发文档

> 基于 **C++ / Qt 6 / OpenCV / CMake** 的轻量级图像批注编辑工具。

---

## 1. 功能概览

| 分类 | 功能 | 备注 |
|------|------|------|
| 文件 | 打开 / 保存 / 另存 | 支持 PNG / JPG / BMP / TIFF / WebP |
| 视图 | **以鼠标为中心的滚轮缩放**、右键拖动平移、`Ctrl+0` 适应窗口 | 缩放范围 0.05× – 40× |
| 绘图 | 直线、矩形、椭圆、箭头、铅笔 | 实时预览，松开鼠标提交 |
| 编辑 | 马赛克（OpenCV 像素化）、添加文字、图像裁剪 | 裁剪后自动适应窗口 |
| 文本 | OCR 提取文字（基于 Tesseract，可选） | 详见 §6 |
| 撤销 | `Ctrl+Z`，最多 30 步 | 每次提交操作前快照 |
| 样式 | 自定义 SVG 工具图标 + Fusion 暗色主题 | 见 `resources/icons/*.svg` |

---

## 2. 目录结构

```
p206_kImgEdit/
├── CMakeLists.txt              # 构建脚本（Qt6 + OpenCV + Ninja + MSVC2022）
├── build.ps1                   # 一键编译（PowerShell）
├── run.ps1                     # 一键运行
├── task.md                     # 需求描述
├── docs/
│   └── DEVELOPMENT.md          # 本文档
├── resources/
│   ├── resources.qrc           # Qt 资源清单
│   └── icons/                  # SVG 图标
└── src/
    ├── main.cpp                # 程序入口
    ├── MainWindow.{h,cpp}      # 主窗口：菜单、工具栏、状态栏
    ├── ImageCanvas.{h,cpp}     # 图像画布：显示、缩放、绘图、撤销
    ├── OcrService.{h,cpp}      # OCR 服务（调用 tesseract.exe）
    └── Tools.h                 # 工具枚举
```

---

## 3. 关键设计

### 3.1 ImageCanvas（自定义 `QWidget`）

- **图像存储**：单一 `QImage`（`Format_RGBA8888`）。
- **视图变换**：维护 `m_scale`（缩放）和 `m_offset`（在 widget 坐标系的图像左上角偏移）。
  - `widgetToImage(p) = (p - offset) / scale`
  - `imageToWidget(p) = p*scale + offset`
- **以鼠标为中心缩放**：

  ```cpp
  void applyZoomAround(double newScale, const QPointF& widgetAnchor) {
      QPointF imgAnchor = widgetToImage(widgetAnchor);
      m_scale  = clamp(newScale, 0.05, 40.0);
      m_offset = widgetAnchor - imgAnchor * m_scale;   // 关键：保持锚点不动
  }
  ```
- **撤销栈**：`QStack<QImage>`，提交任何修改前 `push(m_image.copy())`，最大保留 30 个快照（`m_undoLimit`）。`QImage::copy` 受益于 Qt 隐式共享，开销可控。
- **预览/提交**：
  - 鼠标按下记录起点；移动时调用 `update()`，在 `paintEvent` 中再画一个临时预览（不修改 `m_image`）。
  - 鼠标释放时调用 `commitXxx()`，用 `QPainter(&m_image)` 真正画在图像上。
- **马赛克实现**：将 ROI 转为 `cv::Mat`，`cv::resize` 到 `1/block` 后再 `INTER_NEAREST` 还原，回写到 `m_image`：
  ```cpp
  cv::resize(roi, down, Size(roi.cols/block, roi.rows/block), 0,0, INTER_AREA);
  cv::resize(down, up,  roi.size(),                           0,0, INTER_NEAREST);
  ```

### 3.2 工具切换

`MainWindow` 中所有工具按钮加入互斥的 `QActionGroup`，每个 `QAction` 用 `setData(int(kimg::Tool::xxx))` 记录工具枚举，统一回调 `onToolTriggered(QAction*)` 切换 `ImageCanvas` 的当前工具。

### 3.3 OCR

`OcrService::recognize()` 把选区另存为临时 PNG，调用 `tesseract.exe input output -l chi_sim+eng`，再读取 `output.txt`。
查找顺序：
1. 环境变量 `KIMG_TESSERACT` 指向的可执行文件；
2. 系统 PATH；
3. `C:\Program Files\Tesseract-OCR\tesseract.exe` 等常见路径。

找不到时弹出对话框说明安装方法，不会崩溃。

### 3.4 UI 美化

- 工具栏图标：手写 SVG（位于 `resources/icons/`），统一线性灰 `#dde3ea`，重要色（黄、蓝、绿）做点缀。
- 整体样式表：自定义暗色 QSS（见 `MainWindow::applyStyle()`），并启用 Fusion 风格。
- 应用图标：`resources/icons/app.ico`（7 档尺寸，已嵌入 EXE）+ `app.svg` 备用；运行时 `QApplication::setWindowIcon` 优先加载 ICO。若需重新生成 ICO：
  ```powershell
  python tools/gen_app_icon.py   # 需要 Pillow: python -m pip install pillow
  .\build.ps1 -Clean
  ```

---

## 4. 构建环境

| 依赖 | 版本 / 路径 |
|------|------|
| Qt6 | `D:\Qt6\6.9.1\msvc2022_64` |
| OpenCV | `D:\win10\opencv4130\build`（vc16，兼容 MSVC 2022） |
| CMake | `D:\win10\cmake-4.2.1-windows-x86_64\bin\cmake.exe` |
| Ninja | `D:\win10\ninja.exe` |
| MSVC | Visual Studio 2022 C++ 工具集（通过 `vswhere` 自动定位） |
| Tesseract（可选） | https://github.com/UB-Mannheim/tesseract/wiki |

---

## 5. 编译 & 运行

### 5.1 一键脚本

打开 **PowerShell**，进入项目目录：

```powershell
cd D:\k8\260515kelvin\p206_kImgEdit

# 编译（Release，输出 .\output\Release\kImgEdit.exe，自动 windeployqt + 拷贝 OpenCV DLL）
.\build.ps1

# 强制清理后编译
.\build.ps1 -Clean

# Debug 构建
.\build.ps1 -Config Debug

# 编译并立即运行
.\build.ps1 -Run

# 仅运行（带默认图片）
.\run.ps1
```

### 5.2 手动 CMake（在 “x64 Native Tools Command Prompt for VS 2022” 中）

```bat
set "QT=D:\Qt6\6.9.1\msvc2022_64"
set "OCV=D:\win10\opencv4130\build"
set "CMAKE=D:\win10\cmake-4.2.1-windows-x86_64\bin\cmake.exe"
set "NINJA=D:\win10\ninja.exe"

"%CMAKE%" -S . -B output\Release -G Ninja ^
   -DCMAKE_MAKE_PROGRAM=%NINJA% ^
   -DCMAKE_BUILD_TYPE=Release ^
   -DCMAKE_PREFIX_PATH=%QT% ^
   -DOpenCV_DIR=%OCV% ^
   -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl

"%CMAKE%" --build output\Release --parallel
output\Release\kImgEdit.exe
```

### 5.3 命令行参数

```text
kImgEdit.exe                 # 自动尝试加载 D:\media\xi_an_hot\w700d1q75.jpg
kImgEdit.exe path\to\img.jpg # 启动时打开指定图片
kImgEdit.exe --help
kImgEdit.exe --version
```

---

## 6. OCR 启用步骤

1. 安装 [UB-Mannheim Tesseract 5.x](https://github.com/UB-Mannheim/tesseract/wiki)。
2. 安装时勾选 “Additional language data → Chinese (Simplified)”；或事后把 `chi_sim.traineddata` 拷到 `tessdata` 目录。
3. **在 kImgEdit 中通过界面配置（推荐）：**
   - 菜单 **Settings → Tesseract Path...** （或工具栏齿轮按钮）选择 `tesseract.exe`；程序会先尝试 `tesseract --version` 验证并保存到 `QSettings`（Windows 注册表 `HKCU\Software\kelvin\kImgEdit\ocr\tesseract`）。
   - **Settings → Tesseract Status...** 可查看当前解析到的路径、来源、版本号。
   - **Settings → Clear Tesseract Override** 移除用户设置，恢复自动探测。
4. 自动探测顺序（`OcrService::findTesseract`）：
   1. 用户在界面里设置并保存的路径（`QSettings: ocr/tesseract`）
   2. 环境变量 `KIMG_TESSERACT`
   3. 系统 `PATH` 中的 `tesseract.exe`
   4. 常见安装目录：`C:\Program Files\Tesseract-OCR\tesseract.exe` 等
5. 找不到时，OCR 不会崩溃，只会弹窗给出上述安装/配置指引。
6. 实际使用：选 **OCR** 工具，左键拖一个矩形，松开后弹出识别结果对话框，可一键复制。

---

## 7. 操作速查

| 操作 | 快捷键 / 鼠标 |
|------|---------------|
| 打开图片 | `Ctrl+O` |
| 保存 / 另存 | `Ctrl+S` / `Ctrl+Shift+S` |
| 撤销 | `Ctrl+Z` |
| 适应窗口 | `Ctrl+0` |
| 缩放 | 滚轮（以鼠标为中心） |
| 平移 | 按住**右键**拖动 |
| 直线 / 矩形 / 圆 / 箭头 / 铅笔 | 左键按下 → 拖动 → 松开 |
| 马赛克 / 裁剪 / OCR | 左键拖出选区 |
| 添加文字 | 选 Text 工具，单击图像位置，弹窗输入文字 |
| 颜色 | 工具栏 “Color” 按钮 |
| 线宽 | 工具栏 “Width” 数字框（1–60 px） |
| 马赛克块大小 | 工具栏 “Mosaic” 数字框（2–80 px） |
| 字体大小 | 工具栏 “Font” 数字框（8–200 px） |

---

## 8. 已知约束 / 后续改进

- 文字工具目前使用 “Microsoft YaHei” 粗体，可扩展为字体选择对话框。
- OCR 依赖外部 tesseract.exe。若需完全离线一体化，可改用 `libtesseract` 静态链接。
- 撤销栈按 `QImage` 整图快照，对超大图（>8K）内存敏感；可换为 patch-based undo。
- 当前没有重做（Redo），如需加，复制 `m_undo` 思路即可。

---

## 9. 许可

仅供内部使用与学习。OpenCV 遵循 Apache-2.0；Qt 6 在本场景以 LGPL/商业方式动态链接（请确保许可证合规）。
