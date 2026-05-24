<#
.SYNOPSIS  Launch kImgEdit (built with build.ps1).
.PARAMETER Config Release | Debug | RelWithDebInfo
.PARAMETER Image  Optional image file to open on startup.
#>
[CmdletBinding()]
param(
    [ValidateSet('Release','Debug','RelWithDebInfo')]
    [string]$Config = 'Release',
    [string]$Image  = 'D:\media\xi_an_hot\w700d1q75.jpg'
)

$ErrorActionPreference = 'Stop'
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Exe = Join-Path $ScriptDir "output\$Config\kImgEdit.exe"

if (-not (Test-Path $Exe)) {
    throw "Executable not found: $Exe`nRun .\build.ps1 -Config $Config first."
}

$args = @()
if ($Image -and (Test-Path $Image)) { $args += $Image }

& $Exe @args
