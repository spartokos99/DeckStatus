# Disposable update packages and process ownership checks for updater_test.cjs.
param([ValidateSet('Package','Archive','Lock','Stop')][string]$Action,[string]$Config)
$ErrorActionPreference='Stop'
$settings=Get-Content -LiteralPath $Config -Raw -Encoding UTF8 | ConvertFrom-Json
$testRoot=[IO.Path]::GetFullPath($settings.testRoot)
$allowed=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../build/test-artifacts'))+'\'
if (!$testRoot.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)) { throw 'Fixture outside test artifacts' }
if ($Action -eq 'Stop') {
    foreach ($helper in Get-CimInstance Win32_Process -Filter "Name = 'powershell.exe'") {
        if ($helper.CommandLine -and $helper.CommandLine.Contains($testRoot+'\') -and $helper.CommandLine.Contains('helper.ps1') -and $helper.CommandLine.Contains(' -Plan ')) {
            Stop-Process -Id $helper.ProcessId -ErrorAction SilentlyContinue
        }
    }
    $exe=Join-Path $testRoot 'installed app/DeckStatus.exe'
    foreach ($process in Get-Process -Name DeckStatus -ErrorAction SilentlyContinue) {
        if ($process.Path -ieq $exe) { $process.Kill(); $process.WaitForExit() }
    }
    exit 0
}
if ($Action -eq 'Lock') {
    $stream=[IO.File]::Open((Join-Path $testRoot 'installed app/DeckStatusBridge.dll'),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try { [Console]::WriteLine('locked'); [Console]::Out.Flush(); [void][Console]::ReadLine() }
    finally { $stream.Dispose() }
    exit 0
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.IO.Compression
if ($Action -eq 'Package') {
    $source=[IO.Path]::GetFullPath($settings.source)
    $package=Join-Path $testRoot 'package'
    [void][IO.Directory]::CreateDirectory($package)
    foreach ($name in @('DeckStatus.exe','DeckStatusBridge.dll','DeckStatus.Update.ps1','web')) { Copy-Item -LiteralPath (Join-Path $source $name) -Destination $package -Recurse }
    Copy-Item -LiteralPath ([IO.Path]::GetFullPath($settings.prolink)) -Destination (Join-Path $package 'prolink') -Recurse
    [IO.Compression.ZipFile]::CreateFromDirectory($package,(Join-Path $testRoot 'release.zip'),[IO.Compression.CompressionLevel]::Fastest,$false)
} else {
    $file=[IO.Path]::GetFullPath($settings.archive)
    if (!$file.StartsWith($testRoot+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Archive outside fixture' }
    $zip=[IO.Compression.ZipFile]::Open($file,[IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($name in $settings.entries) {
            $entry=$zip.CreateEntry($name);$writer=[IO.StreamWriter]::new($entry.Open());$writer.Write('fixture');$writer.Dispose()
        }
    } finally { $zip.Dispose() }
}
