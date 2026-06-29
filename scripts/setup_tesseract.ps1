<#
.SYNOPSIS
    Bundle Tesseract OCR from a local install into deps_sdk and msvc_release.

.DESCRIPTION
    Default source: D:\Program Files\Tesseract-OCR
    Layout:
      deps_sdk/tesseract/             - portable engine + tessdata
      msvc_release/tesseract/         - runtime copy
      msvc_release/models/tessdata/   - eng + chi_sim (project models dir)

.EXAMPLE
    code\scripts\setup_tesseract.bat
    code\scripts\setup_tesseract.ps1 -SourceDir "D:\Program Files\Tesseract-OCR"
#>
[CmdletBinding()]
param(
    [string]$SourceDir = 'D:\Program Files\Tesseract-OCR',
    [switch]$SkipDeploy
)

$ErrorActionPreference = 'Stop'
$ScriptDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$CodeDir    = Split-Path -Parent $ScriptDir
$ProjectDir = Split-Path -Parent $CodeDir

$DepsTess    = Join-Path $ProjectDir 'deps_sdk\tesseract'
$ReleaseDir  = Join-Path $ProjectDir 'msvc_release'
$ReleaseTess = Join-Path $ReleaseDir 'tesseract'
$ModelsDir   = Join-Path $ReleaseDir 'models\tessdata'

$LangFiles = @('eng.traineddata', 'chi_sim.traineddata', 'osd.traineddata')

function Ensure-Dir([string]$Path) {
    New-Item -ItemType Directory -Force -Path $Path | Out-Null
}

function Copy-EngineBundle([string]$SrcRoot, [string]$DstRoot) {
    if (-not (Test-Path (Join-Path $SrcRoot 'tesseract.exe'))) {
        throw "tesseract.exe not found under: $SrcRoot"
    }

    if (Test-Path $DstRoot) { Remove-Item -Recurse -Force $DstRoot }
    Ensure-Dir $DstRoot

    Write-Host "[copy] engine -> $DstRoot" -ForegroundColor Cyan
    Copy-Item -Path (Join-Path $SrcRoot 'tesseract.exe') -Destination $DstRoot
    Copy-Item -Path (Join-Path $SrcRoot '*.dll') -Destination $DstRoot -ErrorAction SilentlyContinue

    $srcTessdata = Join-Path $SrcRoot 'tessdata'
    $dstTessdata = Join-Path $DstRoot 'tessdata'
    Ensure-Dir $dstTessdata

    foreach ($lang in $LangFiles) {
        $src = Join-Path $srcTessdata $lang
        if (-not (Test-Path $src)) {
            throw "Missing language data: $src"
        }
        Copy-Item -Path $src -Destination $dstTessdata
    }

    $configs = Join-Path $srcTessdata 'configs'
    if (Test-Path $configs) {
        Copy-Item -Path $configs -Destination $dstTessdata -Recurse
    }
}

$src = $SourceDir.TrimEnd('\')
Write-Host "[source] $src" -ForegroundColor Green

Ensure-Dir (Split-Path -Parent $DepsTess)
Copy-EngineBundle $src $DepsTess

if (-not $SkipDeploy) {
    Ensure-Dir $ReleaseDir
    Copy-EngineBundle $DepsTess $ReleaseTess

    Write-Host "[deploy] models -> $ModelsDir" -ForegroundColor Green
    if (Test-Path $ModelsDir) { Remove-Item -Recurse -Force $ModelsDir }
    Ensure-Dir $ModelsDir
    foreach ($lang in $LangFiles) {
        Copy-Item -Path (Join-Path $DepsTess "tessdata\$lang") -Destination $ModelsDir
    }
}

Write-Host ""
Write-Host "[done] Tesseract bundled:" -ForegroundColor Green
Write-Host "  deps   : $DepsTess\tesseract.exe"
Write-Host "  runtime: $ReleaseTess\tesseract.exe"
Write-Host "  models : $ModelsDir"
Write-Host "  langs  : $($LangFiles -join ', ')"
