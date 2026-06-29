<#
.SYNOPSIS
    Build kImgEdit (Qt6 + OpenCV) using CMake + Ninja + MSVC 2022.

.DESCRIPTION
    - Loads the MSVC 2022 x64 environment (vcvars64.bat).
    - Configures under <project>/build_msvc, outputs exe/DLLs to <project>/msvc_release.
    - Default config is Release. Pass -Config Debug for a debug build.

.EXAMPLE
    .\build.ps1
    .\build.ps1 -Config Debug
    .\build.ps1 -Clean
#>
[CmdletBinding()]
param(
    [ValidateSet('Release','Debug','RelWithDebInfo')]
    [string]$Config = 'Release',
    [switch]$Clean,
    [switch]$Run
)

$ErrorActionPreference = 'Continue'  # native stderr (CMake warnings) must NOT abort
$ScriptDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$CodeDir    = Split-Path -Parent $ScriptDir
$ProjectDir = Split-Path -Parent $CodeDir

function _Die($msg) {
    Write-Host "[error] $msg" -ForegroundColor Red
    exit 1
}

# --- Toolchain locations -----------------------------------------------------
$CMakeExe = 'D:\win10\cmake-4.3.2-windows-x86_64\bin\cmake.exe'
$NinjaExe = 'D:\win10\ninja.exe'
$QtDir    = 'D:\Qt6\6.9.1\msvc2022_64'
$OpenCV   = 'D:\win10\opencv500\build'
if (-not (Test-Path (Join-Path $OpenCV 'OpenCVConfig.cmake'))) {
    $fallback = 'D:\win10\opencv4130\build'
    if (Test-Path (Join-Path $fallback 'OpenCVConfig.cmake')) {
        Write-Host "[warn] OpenCV 5.0 not found at $OpenCV; using $fallback" -ForegroundColor Yellow
        $OpenCV = $fallback
    }
}

if (-not (Test-Path $CMakeExe)) { _Die "CMake not found: $CMakeExe" }
if (-not (Test-Path $NinjaExe)) { _Die "Ninja not found: $NinjaExe" }
if (-not (Test-Path (Join-Path $OpenCV 'OpenCVConfig.cmake'))) {
    _Die "OpenCV not found: $OpenCV\OpenCVConfig.cmake`nInstall OpenCV 5.0 to D:\win10\opencv500\build or adjust paths in build.ps1."
}

# --- Output directories (relative to project root) ---------------------------
$BuildDir   = Join-Path $ProjectDir 'build_msvc'
$ReleaseDir = Join-Path $ProjectDir 'msvc_release'

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "[clean] removing $BuildDir" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}
New-Item -ItemType Directory -Force -Path $BuildDir   | Out-Null
New-Item -ItemType Directory -Force -Path $ReleaseDir | Out-Null
foreach ($sub in @('models', 'input', 'output')) {
    New-Item -ItemType Directory -Force -Path (Join-Path $ReleaseDir $sub) | Out-Null
}

# --- Import MSVC env into current PowerShell --------------------------------
Write-Host "[env] loading MSVC env from vcvars64.bat" -ForegroundColor Cyan
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vsWhere)) {
    $vsWhere = "$env:ProgramFiles\Microsoft Visual Studio\Installer\vswhere.exe"
}
if (-not (Test-Path $vsWhere)) {
    _Die "vswhere.exe not found; install Visual Studio 2022 with C++ workload."
}
$vsInstall = & $vsWhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
if (-not $vsInstall) { _Die "Visual Studio with VC tools not found." }
$VcVars = Join-Path $vsInstall 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $VcVars)) { _Die "vcvars64.bat not found at $VcVars" }

$tmp = [System.IO.Path]::GetTempFileName() + ".bat"
@"
@echo off
call "$VcVars" > nul
set
"@ | Set-Content -Encoding ASCII -Path $tmp
$envLines = & cmd.exe /c $tmp
Remove-Item $tmp -Force
foreach ($line in $envLines) {
    if ($line -match '^([^=]+)=(.*)$') {
        [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2])
    }
}

# --- Configure --------------------------------------------------------------
Write-Host "[configure] $Config  (build: build_msvc, output: msvc_release)" -ForegroundColor Green
$cfgArgs = @(
    '-S', $CodeDir,
    '-B', $BuildDir,
    '-G', 'Ninja',
    "-DCMAKE_MAKE_PROGRAM=$NinjaExe",
    "-DCMAKE_BUILD_TYPE=$Config",
    "-DCMAKE_PREFIX_PATH=$QtDir",
    "-DOpenCV_DIR=$OpenCV",
    '-DCMAKE_C_COMPILER=cl',
    '-DCMAKE_CXX_COMPILER=cl'
)
& $CMakeExe @cfgArgs 2>&1 | ForEach-Object { Write-Host $_ }
if ($LASTEXITCODE -ne 0) { _Die "CMake configure failed (exit=$LASTEXITCODE)." }

# --- Build ------------------------------------------------------------------
Write-Host "[build]" -ForegroundColor Green
& $CMakeExe --build $BuildDir --parallel 2>&1 | ForEach-Object { Write-Host $_ }
if ($LASTEXITCODE -ne 0) { _Die "Build failed (exit=$LASTEXITCODE)." }

$Exe = Join-Path $ReleaseDir 'kImgEdit.exe'
Write-Host ""
Write-Host "[done] -> $Exe" -ForegroundColor Green

# Deploy bundled Tesseract OCR (if deps_sdk/tesseract was set up)
$TessSrc = Join-Path $ProjectDir 'deps_sdk\tesseract'
$TessExe = Join-Path $TessSrc 'tesseract.exe'
if (-not (Test-Path $TessExe)) {
    $hit = Get-ChildItem -Path $TessSrc -Filter 'tesseract.exe' -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($hit) { $TessExe = $hit.FullName }
}
$TessDst = Join-Path $ReleaseDir 'tesseract'
$ModelsTessdata = Join-Path $ReleaseDir 'models\tessdata'
if (Test-Path $TessExe) {
    Write-Host "[deploy] tesseract -> msvc_release\tesseract" -ForegroundColor Cyan
    if (Test-Path $TessDst) { Remove-Item -Recurse -Force $TessDst }
    Copy-Item -Path $TessSrc -Destination $TessDst -Recurse -Force
    $srcTessdata = Join-Path $TessSrc 'tessdata'
    if (Test-Path $srcTessdata) {
        New-Item -ItemType Directory -Force -Path $ModelsTessdata | Out-Null
        Copy-Item -Path (Join-Path $srcTessdata '*.traineddata') -Destination $ModelsTessdata -Force -ErrorAction SilentlyContinue
    }
} else {
    Write-Host "[hint] OCR not bundled. Run: scripts\setup_tesseract.bat" -ForegroundColor DarkYellow
}

if ($Run) {
    Write-Host "[run]" -ForegroundColor Green
    Push-Location $ReleaseDir
    try { & $Exe } finally { Pop-Location }
}
