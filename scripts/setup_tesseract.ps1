<#
.SYNOPSIS
    Bundle Tesseract OCR from a local install into deps_sdk and msvc_release.

.DESCRIPTION
    Default source: D:\Program Files\Tesseract-OCR
    Copies tesseract.exe, all DLLs, and the complete tessdata folder.

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
    if (-not (Test-Path $srcTessdata)) {
        throw "tessdata folder not found under: $SrcRoot"
    }
    Copy-Item -Path $srcTessdata -Destination $DstRoot -Recurse -Force
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
    Copy-Item -Path (Join-Path $DepsTess 'tessdata') -Destination $ModelsDir -Recurse -Force
}

$langCount = (Get-ChildItem (Join-Path $DepsTess 'tessdata\*.traineddata') -ErrorAction SilentlyContinue).Count
Write-Host ""
Write-Host "[done] Tesseract bundled:" -ForegroundColor Green
Write-Host "  deps   : $DepsTess\tesseract.exe"
Write-Host "  runtime: $ReleaseTess\tesseract.exe"
Write-Host "  models : $ModelsDir"
Write-Host "  langs  : $langCount traineddata file(s)"
