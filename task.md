#### task:

```shell

基于c++,qt,cmake,的工具程序，可以打开一个图像，并显示在界面窗口上。支持按照鼠标为中心的视图缩放；
要提供以下功能，画直线，画矩形，画圆，画箭头，铅笔画，马赛克，ocr提取文字，添加文字，图像裁剪，撤销；所画图元可以选择粗细、颜色；

给出编译指令，运行指令。
输出开发文档到独立的markdown文件中。

对界按钮图标、程序图标进行适当美化；

可用的工具如下：
opencv5.0目录：D:\win10\opencv500\build
默认的输入图片为：D:\media\xi_an_hot\w700d1q75.jpg
qt6目录是：D:\Qt6
halcon根目录是："C:\Users\kelvin\AppData\Local\Programs\MVTec\HALCON-24.11-Progress-Steady"
halcon根目录也可能是："D:\binWin10_D\halcon-24.11-progress-steady"
cmake目录是：D:\win10\cmake-4.3.2-windows-x86_64\bin\cmake.exe
ninja可执行文件：D:\win10\ninja.exe
tesseract目录："D:\Program Files\Tesseract-OCR"


项目目录结构如下：
源码目录为：code,即set CODE_DIR=code
程序根目录为：D:\k8\260515kelvin\p206_kImgEdit（CMake 工程在 %CODE_DIR% 子目录）
即 set PROJECT_DIR=D:\k8\260515kelvin\p206_kImgEdit
源码目录为：%PROJECT_DIR%\%CODE_DIR%
项目依赖的第三方的源码的目录为：%PROJECT_DIR%\%CODE_DIR%\thirdparty（涉及源码下载的不要下载git完整仓库，只下载新版的tag即可，以便节省体积）
项目依赖的第三方的sdk开发包的目录为：%PROJECT_DIR%\deps_sdk
脚本目录：%PROJECT_DIR%\%CODE_DIR%\scripts
文档目录：%PROJECT_DIR%\%CODE_DIR%\docs
文档媒体资源保存目录：%PROJECT_DIR%\%CODE_DIR%\docs\assets
程序编译输出的中间文件的输出目录：%PROJECT_DIR%\build_msvc
程序可执行文件和动态库输出目录：%PROJECT_DIR%\msvc_release
程序运行的模型的目录：%PROJECT_DIR%\msvc_release\models
程序运行的输入数据目录：%PROJECT_DIR%\msvc_release\input
程序运行的输出数据目录：%PROJECT_DIR%\msvc_release\output
程序安装目录为：%PROJECT_DIR%\install
注意：以上目录只是目前规划目录相对结构，程序编译、运行时应以相对路径进行，而不是绝对路径;换句话说，如果将来根目录移动到其它位置，不影响项目的编译、程序的运行；
 


fix:
界面添加设置tesseract.exe的选项；

添加程序图标；

添加支持将图像拖拽到界面上，以便打开图像；

添加支持输入一个roi(四个数字分别表示上下左右的paddding，正数内缩，负数外扩)，以便进行抠图；

图片被编辑后，在关闭程序，或者加载新图的时候，如果没有保存，弹窗提示是否保存。

Tesseract OCR engine not found.
To enable OCR:
  - Menu "Settings -> Tesseract Path..." to point at tesseract.exe, or
  - Install Tesseract (https://github.com/UB-Mannheim/tesseract/wiki),
    add it to PATH, or set environment variable KIMG_TESSERACT.
  - (Optional) Install Chinese data: chi_sim.traineddata
直接将OCR模型下载下来，放到合适的位置；

默认打开图像D:\k8\mdedia_img\深圳市宝安区南昌公园_20260620181419_216_70.jpg

Tesseract failed (exit=1).
stderr:
Error opening data file D:\k8\260515kelvin\p206_kImgEdit\msvc_release\tesseract/chi_sim.traineddata
Please make sure the TESSDATA_PREFIX environment variable is set to your "tessdata" directory.
Failed loading language 'chi_sim'
Error opening data file D:\k8\260515kelvin\p206_kImgEdit\msvc_release\tesseract/eng.traineddata
Please make sure the TESSDATA_PREFIX environment variable is set to your "tessdata" directory.
Failed loading language 'eng'
Tesseract couldn't load any languages!
Could not initialize tesseract.

需要设置到自行安装的tesseract即“D:\Program Files\Tesseract-OCR”就能识别到中文数字英文等，但是目前嵌入的Tesseract就识别不到任何东西，排查一下；

将脚本们放到code下；

PS C:\Users\kelvin> cd D:\k8\260515kelvin\p206_kImgEdit\code\scripts
PS D:\k8\260515kelvin\p206_kImgEdit\code\scripts> .\build.ps1            # 编译 Release
.\build.ps1 : 无法加载文件 D:\k8\260515kelvin\p206_kImgEdit\code\scripts\build.ps1，因为在此系统上禁止运行脚本。有关详
细信息，请参阅 https:/go.microsoft.com/fwlink/?LinkID=135170 中的 about_Execution_Policies。
所在位置 行:1 字符: 1
+ .\build.ps1            # 编译 Release
+ ~~~~~~~~~~~
    + CategoryInfo          : SecurityError: (:) []，PSSecurityException
    + FullyQualifiedErrorId : UnauthorizedAccess
PS D:\k8\260515kelvin\p206_kImgEdit\code\scripts> .\build.ps1 -Clean     # 清理后重编
.\build.ps1 : 无法加载文件 D:\k8\260515kelvin\p206_kImgEdit\code\scripts\build.ps1，因为在此系统上禁止运行脚本。有关详
细信息，请参阅 https:/go.microsoft.com/fwlink/?LinkID=135170 中的 about_Execution_Policies。
所在位置 行:1 字符: 1
+ .\build.ps1 -Clean     # 清理后重编
+ ~~~~~~~~~~~
    + CategoryInfo          : SecurityError: (:) []，PSSecurityException
    + FullyQualifiedErrorId : UnauthorizedAccess
PS D:\k8\260515kelvin\p206_kImgEdit\code\scripts>


默认情况下tesseract的路径就设置为msvc_release下的tesseract\tesseract.exe，取相对路径，保存加载都是相对路径；



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

进入脚本目录（PowerShell / CMD 均可）：

```bat
cd /d D:\k8\260515kelvin\p206_kImgEdit\code\scripts
```

**推荐：用 `.bat` 编译运行**（不受 PowerShell 执行策略限制，PS 和 CMD 都能用）：

```bat
build.bat              REM 编译 Release
build.bat -Clean       REM 清理后重编
run.bat                REM 运行
setup_tesseract.bat    REM 打包 OCR（首次）
```

**若坚持用 `.ps1`**，需先放宽当前用户策略（只需一次）：

```powershell
Set-ExecutionPolicy RemoteSigned -Scope CurrentUser
.\build.ps1
.\build.ps1 -Clean
.\run.ps1
```

或单次绕过策略（不改系统设置）：

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Clean
powershell -ExecutionPolicy Bypass -File .\run.ps1
```

**说明**：不要直接双击 `.ps1`；CMD 中也不能运行 `.\build.ps1`，请用 `build.bat`。

D:\k8\mdedia_img\深圳市宝安区南昌公园_20260620181419_216_70.jpg


