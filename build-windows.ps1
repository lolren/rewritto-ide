param(
  [switch]$Clean,
  [switch]$Debug,
  [switch]$RunTests,
  [switch]$SkipPackage,
  [switch]$SkipCliDownload,
  [string]$QtPrefix = "",
  [string]$BuildDir = "",
  [string]$OutDir = "",
  [string]$Generator = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = (Resolve-Path $PSScriptRoot).Path
$sourceDir = Join-Path $scriptDir "rewritto-core\qt-native-app"
if (-not (Test-Path (Join-Path $sourceDir "CMakeLists.txt"))) {
  throw "Qt native source directory not found: $sourceDir"
}

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
  $BuildDir = Join-Path $scriptDir "build-windows"
}
if ([string]::IsNullOrWhiteSpace($OutDir)) {
  $OutDir = Join-Path $scriptDir "dist"
}

$buildType = if ($Debug.IsPresent) { "Debug" } else { "Release" }

function Resolve-Qt6DirFromPrefix {
  param(
    [string]$Prefix
  )

  if ([string]::IsNullOrWhiteSpace($Prefix)) {
    return $null
  }

  $candidates = @(
    $Prefix,
    (Join-Path $Prefix "lib\cmake\Qt6"),
    (Join-Path $Prefix "lib64\cmake\Qt6")
  )

  foreach ($candidate in $candidates) {
    if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path (Join-Path $candidate "Qt6Config.cmake"))) {
      return (Resolve-Path $candidate).Path
    }
  }

  return $null
}

function Resolve-Qt6Dir {
  param(
    [string]$ExplicitPrefix
  )

  $qt6FromEnv = @($env:Qt6_DIR, $env:QT6_DIR, $env:Qt6Dir)
  foreach ($candidate in $qt6FromEnv) {
    if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path (Join-Path $candidate "Qt6Config.cmake"))) {
      return (Resolve-Path $candidate).Path
    }
  }

  $fromPrefix = Resolve-Qt6DirFromPrefix -Prefix $ExplicitPrefix
  if ($null -ne $fromPrefix) {
    return $fromPrefix
  }

  $fromQtRoot = Resolve-Qt6DirFromPrefix -Prefix $env:QT_ROOT_DIR
  if ($null -ne $fromQtRoot) {
    return $fromQtRoot
  }

  $qmake = Get-Command qmake.exe -ErrorAction SilentlyContinue
  if ($null -eq $qmake) {
    $qmake = Get-Command qmake6.exe -ErrorAction SilentlyContinue
  }
  if ($null -ne $qmake) {
    $qtInstallPrefix = (& $qmake.Source -query QT_INSTALL_PREFIX 2>$null).Trim()
    $fromQmake = Resolve-Qt6DirFromPrefix -Prefix $qtInstallPrefix
    if ($null -ne $fromQmake) {
      return $fromQmake
    }
  }

  return $null
}

if ($Clean.IsPresent -and (Test-Path $BuildDir)) {
  Write-Host "Cleaning build directory: $BuildDir"
  Remove-Item -Path $BuildDir -Force -Recurse
}

New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

$qt6Dir = Resolve-Qt6Dir -ExplicitPrefix $QtPrefix
if ($null -eq $qt6Dir) {
  throw "Qt6 CMake package not found. Set -QtPrefix or Qt6_DIR/QT_ROOT_DIR."
}

$jobs = if ([string]::IsNullOrWhiteSpace($env:NUMBER_OF_PROCESSORS)) { 4 } else { [int]$env:NUMBER_OF_PROCESSORS }

$cmakeArgs = @(
  "-S", $sourceDir,
  "-B", $BuildDir,
  "-DCMAKE_BUILD_TYPE=$buildType",
  "-DBUILD_TESTING=$(if ($RunTests.IsPresent) { "ON" } else { "OFF" })",
  "-DQt6_DIR=$qt6Dir"
)

if (-not [string]::IsNullOrWhiteSpace($Generator)) {
  $cmakeArgs = @("-G", $Generator) + $cmakeArgs
} else {
  $ninja = Get-Command ninja.exe -ErrorAction SilentlyContinue
  if ($null -ne $ninja) {
    $cmakeArgs = @("-G", "Ninja") + $cmakeArgs
  }
}

Write-Host "======================================"
Write-Host "Rewritto-ide - Windows Build Script"
Write-Host "======================================"
Write-Host "Build Type: $buildType"
Write-Host "Source Dir: $sourceDir"
Write-Host "Build Dir : $BuildDir"
Write-Host "Out Dir   : $OutDir"
Write-Host "Qt6 Dir   : $qt6Dir"

& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) {
  throw "CMake configure failed with exit code $LASTEXITCODE"
}

& cmake --build $BuildDir --config $buildType --parallel $jobs
if ($LASTEXITCODE -ne 0) {
  throw "CMake build failed with exit code $LASTEXITCODE"
}

if ($RunTests.IsPresent) {
  & ctest --test-dir $BuildDir --output-on-failure -C $buildType
  if ($LASTEXITCODE -ne 0) {
    throw "Tests failed with exit code $LASTEXITCODE"
  }
}

if (-not $SkipPackage.IsPresent) {
  $packager = Join-Path $sourceDir "packaging\release\package-windows.ps1"
  & $packager `
    -BuildDir $BuildDir `
    -OutDir $OutDir `
    -Configuration $buildType `
    -SkipCliDownload:$SkipCliDownload
  if ($LASTEXITCODE -ne 0) {
    throw "Windows packaging failed with exit code $LASTEXITCODE"
  }
}

Write-Host ""
Write-Host "Build complete."
Write-Host "Executable candidates:"
Write-Host "  $(Join-Path $BuildDir 'rewritto-ide.exe')"
Write-Host "  $(Join-Path $BuildDir "$buildType\rewritto-ide.exe")"
if (-not $SkipPackage.IsPresent) {
  Write-Host "Package:"
  Write-Host "  $(Join-Path $OutDir 'rewritto-ide-windows-x86_64.zip')"
}
