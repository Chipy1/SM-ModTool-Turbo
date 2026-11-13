# Install SM ModTool Turbo into a Mod Tool Release folder.
# Usage: .\install.ps1 -Target "C:\path\to\Scrap Mechanic Mod Tool\Release"
param(
    [Parameter(Mandatory=$true)][string]$Target
)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not (Test-Path (Join-Path $Target 'ModTool.exe'))) {
    throw "$Target\ModTool.exe not found"
}
$dll = Join-Path $here 'dist\d3d11.dll'
$ini = Join-Path $here 'dist\ModToolTurbo.ini'
if (-not (Test-Path $dll)) { throw 'dist\d3d11.dll missing; run build.sh first' }
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$targetDll = Join-Path $Target 'd3d11.dll'
$targetIni = Join-Path $Target 'ModToolTurbo.ini'
if (Test-Path $targetDll) { Copy-Item $targetDll "$targetDll.bak.$stamp" }
$oldVersion = Join-Path $Target 'version.dll'
if (Test-Path $oldVersion) { Write-Warning 'old version.dll is present; v1.0.0 used that name and Proton bypasses it. Remove it if it is the old ModTool Turbo proxy.' }
if (Test-Path $targetIni) { Copy-Item $targetIni "$targetIni.bak.$stamp" }
Copy-Item $dll $targetDll -Force
Copy-Item $ini $targetIni -Force
Write-Host "installed into $Target"
