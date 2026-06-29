<#
.SYNOPSIS  Launch kImgEdit (built with build.ps1).
.PARAMETER Config Release | Debug | RelWithDebInfo  (informational; exe is always msvc_release)
.PARAMETER Image  Optional image file to open on startup.
#>
[CmdletBinding()]
param(
    [ValidateSet('Release','Debug','RelWithDebInfo')]
    [string]$Config = 'Release',
    [string]$Image  = 'D:\media\xi_an_hot\w700d1q75.jpg'
)

$ErrorActionPreference = 'Stop'
$ScriptDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$CodeDir    = Split-Path -Parent $ScriptDir
$ProjectDir = Split-Path -Parent $CodeDir
$ReleaseDir = Join-Path $ProjectDir 'msvc_release'
$Exe        = Join-Path $ReleaseDir 'kImgEdit.exe'

if (-not (Test-Path $Exe)) {
    throw "Executable not found: $Exe`nRun scripts\build.ps1 first."
}

$args = @()
if ($Image -and (Test-Path $Image)) { $args += $Image }

Push-Location $ReleaseDir
try {
    & $Exe @args
} finally {
    Pop-Location
}
