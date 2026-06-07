$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Definition
$staging = Join-Path $repoRoot '_pack_staging'
$zipName = 'fvetool.zip'
$zipPath = Join-Path $repoRoot $zipName

# Clean previous artifacts
if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }

# Create staging directories
New-Item -ItemType Directory -Path (Join-Path $staging 'x86') -Force | Out-Null

# Copy files
$files = @(
    @{ Src = 'x64\Release\fvecli.exe';   Dst = 'fvecli.exe' },
    @{ Src = 'x64\Release\fvegui.exe';   Dst = 'fvegui.exe' },
    @{ Src = 'Win32\Release\fvecli.exe';  Dst = 'x86\fvecli.exe' },
    @{ Src = 'Win32\Release\fvegui.exe';  Dst = 'x86\fvegui.exe' },
    @{ Src = 'LICENSE';                   Dst = 'LICENSE' }
)

foreach ($f in $files) {
    $src = Join-Path $repoRoot $f.Src
    if (-not (Test-Path $src)) {
        Write-Error "Missing file: $src"
        exit 1
    }
    Copy-Item $src (Join-Path $staging $f.Dst)
}

# Create zip
Compress-Archive -Path (Join-Path $staging '*') -DestinationPath $zipPath -Force

# Print hashes
$zipHash = (Get-FileHash $zipPath -Algorithm SHA256).Hash.ToLower()
$x64GuiHash  = (Get-FileHash (Join-Path $repoRoot 'x64\Release\fvegui.exe')  -Algorithm SHA256).Hash.ToLower()
$x64CliHash  = (Get-FileHash (Join-Path $repoRoot 'x64\Release\fvecli.exe')  -Algorithm SHA256).Hash.ToLower()
$x86GuiHash  = (Get-FileHash (Join-Path $repoRoot 'Win32\Release\fvegui.exe') -Algorithm SHA256).Hash.ToLower()
$x86CliHash  = (Get-FileHash (Join-Path $repoRoot 'Win32\Release\fvecli.exe') -Algorithm SHA256).Hash.ToLower()

Write-Host "$zipName SHA256: $zipHash"
Write-Host "fvegui.exe (x64) SHA256: $x64GuiHash"
Write-Host "fvecli.exe (x64) SHA256: $x64CliHash"
Write-Host "fvegui.exe (x86) SHA256: $x86GuiHash"
Write-Host "fvecli.exe (x86) SHA256: $x86CliHash"

# Clean up staging
Remove-Item $staging -Recurse -Force
