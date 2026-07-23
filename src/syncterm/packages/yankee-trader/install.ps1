param(
    [string]$ScriptDirectory = (Join-Path $env:APPDATA "SyncTERM\scripts")
)

$ErrorActionPreference = "Stop"
$packageDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$moduleNames = @(
    "yankee_trader.wren",
    "yankee_trader_calc.wren",
    "yankee_trader_guide.wren",
    "yankee_trader_offline.wren",
    "yankee_trader_state.wren",
    "yankee_trader_tools.wren",
    "yankee_trader_ui.wren"
)

New-Item -ItemType Directory -Force -Path $ScriptDirectory | Out-Null
$menuDirectory = Join-Path (Join-Path $ScriptDirectory "auto") "menu"
New-Item -ItemType Directory -Force -Path $menuDirectory | Out-Null

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

$menuSource = Join-Path (Join-Path (Join-Path $packageDirectory "scripts") "auto\menu") "yankee_trader_menu.wren"
$menuTarget = Join-Path $menuDirectory "yankee_trader_menu.wren"
if (Test-Path $menuTarget) {
    $sourceHash = (Get-FileHash -Algorithm SHA256 $menuSource).Hash
    $targetHash = (Get-FileHash -Algorithm SHA256 $menuTarget).Hash
    if ($sourceHash -ne $targetHash) {
        Copy-Item -Force $menuTarget "$menuTarget.bak"
    }
}
Copy-Item -Force $menuSource $menuTarget

Write-Host "Installed Yankee Trader Wren package in: $ScriptDirectory"
Write-Host "Add 'yankee_trader' to the BBS entry's Wren Scripts list."
Write-Host "Alt-Y also opens the offline planner from SyncTERM's main menu."
