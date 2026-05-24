# kImgEdit

轻量级图像批注 / 编辑工具，基于 **C++ / Qt 6 / OpenCV / CMake**，目标平台 Windows + MSVC 2022。

> 完整开发文档见 [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md)。

## 功能

- 打开图像并在窗口中显示
- **以鼠标为中心的滚轮缩放**，右键拖动平移
- 绘图工具：直线 / 矩形 / 椭圆 / 箭头 / 铅笔
- 编辑工具：马赛克（OpenCV 像素化）、添加文字、图像裁剪
- OCR 提取文字（基于 Tesseract，可选）
- 撤销（`Ctrl+Z`，最多 30 步）
- 可选择画笔粗细与颜色
- 美化的 SVG 工具图标 + 暗色主题

## 快速开始

```powershell
# 在 PowerShell 中（不需要事先开 vcvars，脚本自动处理）
cd D:\k8\260515kelvin\p206_kImgEdit
.\build.ps1            # 编译 Release
.\run.ps1              # 运行（默认打开 D:\media\xi_an_hot\w700d1q75.jpg）
```

更多构建选项 / 命令行 / OCR 部署，请见 `docs/DEVELOPMENT.md`。
