<#
.SYNOPSIS
    Build kImgEdit (Qt6 + OpenCV) using CMake + Ninja + MSVC 2022.

.DESCRIPTION
    - Loads the MSVC 2022 x64 environment (vcvars64.bat).
    - Configures and builds an out-of-source build under .\output\<config>.
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
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

function _Die($msg) {
    Write-Host "[error] $msg" -ForegroundColor Red
    exit 1
}

# --- Toolchain locations -----------------------------------------------------
$CMakeExe = 'D:\win10\cmake-4.2.1-windows-x86_64\bin\cmake.exe'
$NinjaExe = 'D:\win10\ninja.exe'
$QtDir    = 'D:\Qt6\6.9.1\msvc2022_64'
$OpenCV   = 'D:\win10\opencv4130\build'

if (-not (Test-Path $CMakeExe)) { _Die "CMake not found: $CMakeExe" }
if (-not (Test-Path $NinjaExe)) { _Die "Ninja not found: $NinjaExe" }

# Locate vcvars64.bat for MSVC 2022
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

# --- Output directories ------------------------------------------------------
$OutRoot  = Join-Path $ScriptDir 'output'
$BuildDir = Join-Path $OutRoot   $Config
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "[clean] removing $BuildDir" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

# --- Import MSVC env into current PowerShell --------------------------------
Write-Host "[env] loading MSVC env from vcvars64.bat" -ForegroundColor Cyan
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
Write-Host "[configure] $Config" -ForegroundColor Green
$cfgArgs = @(
    '-S', $ScriptDir,
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

$Exe = Join-Path $BuildDir 'kImgEdit.exe'
Write-Host ""
Write-Host "[done] -> $Exe" -ForegroundColor Green

if ($Run) {
    Write-Host "[run]" -ForegroundColor Green
    & $Exe
}
