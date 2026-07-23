param(
    [string]$ScriptDirectory = (Join-Path $env:APPDATA "SyncTERM\scripts")
)

$ErrorActionPreference = "Stop"
$packageDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$moduleNames = @(
    "yankee_trader.wren",
    "yankee_trader_calc.wren",
    "yankee_trader_state.wren",
    "yankee_trader_tools.wren"
)

New-Item -ItemType Directory -Force -Path $ScriptDirectory | Out-Null

foreach ($moduleName in $moduleNames) {
    $source = Join-Path (Join-Path $packageDirectory "scripts") $moduleName
    $target = Join-Path $ScriptDirectory $moduleName
    if (Test-Path $target) {
        $sourceHash = (Get-FileHash -Algorithm SHA256 $source).Hash
        $targetHash = (Get-FileHash -Algorithm SHA256 $target).Hash
        if ($sourceHash -ne $targetHash) {
            Copy-Item -Force $target "$target.bak"
        }
    }
    Copy-Item -Force $source $target
}

Write-Host "Installed Yankee Trader Wren package in: $ScriptDirectory"
Write-Host "Add 'yankee_trader' to the BBS entry's Wren Scripts list."
