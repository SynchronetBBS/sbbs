[CmdletBinding()]
param(
    [ValidateSet('x86', 'x64')]
    [string]$Architecture = 'x64',

    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [string]$XpdevDirectory,

    [switch]$WarningsAsErrors,

    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

if ($env:OS -ne 'Windows_NT') {
    throw 'build-msvc.ps1 must be run on Windows.'
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw 'CMake was not found on PATH.'
}

$sourceDirectory = $PSScriptRoot
$buildDirectory = Join-Path $sourceDirectory "build\msvc\$Architecture"
$cmakePlatform = if ($Architecture -eq 'x86') { 'Win32' } else { 'x64' }

if ($Clean -and (Test-Path -LiteralPath $buildDirectory)) {
    Remove-Item -LiteralPath $buildDirectory -Recurse -Force
}

$configureArguments = @(
    '-S', $sourceDirectory,
    '-B', $buildDirectory,
    '-A', $cmakePlatform
)

if ($PSBoundParameters.ContainsKey('XpdevDirectory')) {
    $configureArguments += "-DOPENDOORS_XPDEV_DIR=$XpdevDirectory"
}

if ($WarningsAsErrors) {
    $configureArguments += '-DOPENDOORS_WARNINGS_AS_ERRORS=ON'
}

Write-Host "Configuring OpenDoors for $Architecture..."
& cmake @configureArguments
if ($LASTEXITCODE -ne 0) {
    throw 'CMake configuration failed. Ensure a compatible Visual Studio generator and MSVC toolchain are installed.'
}

Write-Host "Building OpenDoors ($Configuration)..."
& cmake --build $buildDirectory --config $Configuration
if ($LASTEXITCODE -ne 0) {
    throw 'CMake build failed.'
}

Write-Host "Build complete: $buildDirectory"
