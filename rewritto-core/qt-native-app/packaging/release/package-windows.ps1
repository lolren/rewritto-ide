param(
  [string]$BuildDir = "",
  [string]$OutDir = "",
  [string]$Configuration = "Release",
  [string]$WindeployqtPath = "",
  [switch]$SkipCliDownload
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$rootDir = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
  $BuildDir = Join-Path $rootDir "build"
}
if ([string]::IsNullOrWhiteSpace($OutDir)) {
  $OutDir = Join-Path $rootDir "dist"
}

function Resolve-WindeployqtPath {
  param(
    [string]$ExplicitPath
  )

  if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
    if (Test-Path $ExplicitPath) {
      return (Resolve-Path $ExplicitPath).Path
    }
    throw "windeployqt path does not exist: $ExplicitPath"
  }

  $cmd = Get-Command windeployqt.exe -ErrorAction SilentlyContinue
  if ($null -ne $cmd) {
    return $cmd.Source
  }

  if (-not [string]::IsNullOrWhiteSpace($env:QT_ROOT_DIR)) {
    $candidate = Join-Path $env:QT_ROOT_DIR "bin\windeployqt.exe"
    if (Test-Path $candidate) {
      return $candidate
    }
  }

  throw "windeployqt.exe not found in PATH and QT_ROOT_DIR/bin."
}

function Resolve-AppBinaryPath {
  param(
    [string]$BuildDirPath,
    [string]$BuildConfig
  )

  $candidates = @(
    (Join-Path $BuildDirPath "rewritto-ide.exe"),
    (Join-Path $BuildDirPath "$BuildConfig\rewritto-ide.exe")
  )

  foreach ($candidate in $candidates) {
    if (Test-Path $candidate) {
      return (Resolve-Path $candidate).Path
    }
  }

  throw "Native binary not found. Expected one of: $($candidates -join ', ')"
}

function Resolve-ArduinoCliPath {
  param(
    [string]$BuildDirPath,
    [string]$RootDir,
    [switch]$SkipDownload
  )

  $candidates = @(
    (Join-Path $BuildDirPath "arduino-cli.exe"),
    (Join-Path $RootDir ".tools\windows\arduino-cli\arduino-cli.exe"),
    (Join-Path $RootDir ".tools\appimage\arduino-cli\arduino-cli.exe")
  )

  foreach ($candidate in $candidates) {
    if (Test-Path $candidate) {
      return (Resolve-Path $candidate).Path
    }
  }

  $cliFromPath = Get-Command arduino-cli.exe -ErrorAction SilentlyContinue
  if ($null -ne $cliFromPath) {
    return $cliFromPath.Source
  }

  if ($SkipDownload.IsPresent) {
    return $null
  }

  $toolsDir = Join-Path $RootDir ".tools\windows\arduino-cli"
  $archivePath = Join-Path $toolsDir "arduino-cli.zip"
  $extractDir = Join-Path $toolsDir "extract"
  $downloadedCli = Join-Path $toolsDir "arduino-cli.exe"
  $downloadUrl = if (-not [string]::IsNullOrWhiteSpace($env:ARDUINO_CLI_URL)) {
    $env:ARDUINO_CLI_URL
  } else {
    "https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Windows_64bit.zip"
  }

  New-Item -ItemType Directory -Path $toolsDir -Force | Out-Null
  if (Test-Path $extractDir) {
    Remove-Item -Path $extractDir -Force -Recurse
  }

  Write-Host "Downloading arduino-cli for Windows from $downloadUrl"
  Invoke-WebRequest -Uri $downloadUrl -OutFile $archivePath
  Expand-Archive -Path $archivePath -DestinationPath $extractDir -Force

  $cli = Get-ChildItem -Path $extractDir -Filter "arduino-cli.exe" -File -Recurse |
    Select-Object -First 1
  if ($null -eq $cli) {
    return $null
  }

  Copy-Item -Path $cli.FullName -Destination $downloadedCli -Force
  return (Resolve-Path $downloadedCli).Path
}

$buildDirPath = (Resolve-Path $BuildDir).Path
$outDirPath = $OutDir
New-Item -ItemType Directory -Path $outDirPath -Force | Out-Null
$outDirPath = (Resolve-Path $outDirPath).Path

$appBinary = Resolve-AppBinaryPath -BuildDirPath $buildDirPath -BuildConfig $Configuration
$windeployqt = Resolve-WindeployqtPath -ExplicitPath $WindeployqtPath

$packageName = "rewritto-ide-windows-x86_64"
$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("rewritto-ide-" + [Guid]::NewGuid().ToString("N"))
$packageDir = Join-Path $tempRoot $packageName

New-Item -ItemType Directory -Path $packageDir -Force | Out-Null

try {
  Copy-Item -Path $appBinary -Destination (Join-Path $packageDir "rewritto-ide.exe") -Force

  $licensePath = Join-Path $rootDir "LICENSE.txt"
  if (Test-Path $licensePath) {
    Copy-Item -Path $licensePath -Destination (Join-Path $packageDir "LICENSE.txt") -Force
  }

  & $windeployqt `
    --release `
    --force `
    --compiler-runtime `
    --no-translations `
    (Join-Path $packageDir "rewritto-ide.exe")
  if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE"
  }

  $cliPath = Resolve-ArduinoCliPath -BuildDirPath $buildDirPath -RootDir $rootDir -SkipDownload:$SkipCliDownload
  if ($null -ne $cliPath) {
    Copy-Item -Path $cliPath -Destination (Join-Path $packageDir "arduino-cli.exe") -Force
  } else {
    Write-Warning "arduino-cli.exe not packaged. Runtime will fall back to PATH."
  }

  @"
Rewritto-ide (Windows native package)

Run:
  rewritto-ide.exe

Notes:
- Use this package on Windows x86_64.
- If included, arduino-cli.exe is auto-detected by rewritto-ide.
"@ | Set-Content -Path (Join-Path $packageDir "README.txt") -Encoding utf8

  $archivePath = Join-Path $outDirPath "$packageName.zip"
  if (Test-Path $archivePath) {
    Remove-Item -Path $archivePath -Force
  }
  Compress-Archive -Path (Join-Path $packageDir "*") -DestinationPath $archivePath -Force

  Write-Host "Windows package created:"
  Write-Host "  $archivePath"
} finally {
  if (Test-Path $tempRoot) {
    Remove-Item -Path $tempRoot -Force -Recurse
  }
}
