#### task:

```shell

基于c++,qt,cmake,的工具程序，可以打开一个图像，并显示在界面窗口上。支持按照鼠标为中心的视图缩放；
要提供以下功能，画直线，画矩形，画圆，画箭头，铅笔画，马赛克，ocr提取文字，添加文字，图像裁剪，撤销；所画图元可以选择粗细、颜色；
 

对界按钮图标、程序图标进行适当美化；

可用的工具如下：
opencv根目录是：D:\win10\opencv4130\build
默认的输入图片为：D:\media\xi_an_hot\w700d1q75.jpg
qt6目录是：D:\Qt6
halcon根目录是："C:\Users\kelvin\AppData\Local\Programs\MVTec\HALCON-24.11-Progress-Steady\bin\x64-win64"
cmake目录是：D:\win10\cmake-4.2.1-windows-x86_64\bin\cmake.exe
ninja可执行文件：D:\win10\ninja.exe
输出目录是：.\output


本机安装的有vs2022，基于msvc2022。
给出编译指令，运行指令。
输出开发文档到独立的markdown文件中。

fix:
界面添加设置tesseract.exe的选项；

添加程序图标；
```

以下无效：

```shell
halcon根目录是："D:\Program Files\MVTec\HALCON-17.12-Progress"


原始模板图像：D:\260515luokun\Photos\img_20260515_113333_327_template.png
模板文件输出到：.\output\small_tamplate.json
将形状模板中的形状的图像也输出到文件显示。
另外，还有一个测试项，即对目标图进行模板匹配；模板匹配结束时，输出图像标记显示定位的位置。
编辑的mask区域的数据按照点列的格式进行表示和保存；
模板数据包括以上的那些主要的数据，还有模板图像文件的宽高，通道数，roi区域的位置和大小，等等，保存在json文件中。
根据图片中的11x8的棋盘格对原始图像进行校正，转换到正交空间中去，并保存到图片文件中；将变换矩阵以及逆变换矩阵保存到独立的yaml中去。
给类ChessboardRectifier添加4个接口，实现对某个原始图像执行变换，以及逆变换，对某个像素点执行变换，以及逆变换。
对图像D:\260515luokun\Photos\small\image_20260514_0002.jpg执行变换，输出到image_20260514_0002_orthogonal.jpg
对图像D:\260515luokun\Photos\img_20260515_113333_327_template.png执行变换，输出到img_20260515_113333_327_template_orthogonal.jpg

界面添加设置tesseract.exe的选项；

```

#### compile:

```shell

```