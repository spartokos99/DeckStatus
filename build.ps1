param([ValidateSet('Debug','Release')][string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmakeCommand) { $cmake = $cmakeCommand.Source }
else {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio mit Desktopentwicklung C++ und CMake installieren.' }
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) { throw 'Visual Studio C++-Buildtools nicht gefunden.' }
    $cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
}
& $cmake -S $root -B (Join-Path $root 'build') -A x64
if ($LASTEXITCODE) { throw 'CMake-Konfiguration fehlgeschlagen.' }
& $cmake --build (Join-Path $root 'build') --config $Configuration --parallel
if ($LASTEXITCODE) { throw 'Build fehlgeschlagen.' }
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
& $ctest --test-dir (Join-Path $root 'build') -C $Configuration --output-on-failure
if ($LASTEXITCODE) { throw 'Tests fehlgeschlagen.' }
Write-Host "Fertig: $root\build\$Configuration\DeckStatus.exe"
