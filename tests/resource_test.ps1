param(
    [string]$Executable = (Join-Path $PSScriptRoot '..\build\Release\DeckStatus.exe'),
    [string]$ExpectedVersion = '1.3.2'
)
$ErrorActionPreference = 'Stop'
$target = (Resolve-Path -LiteralPath $Executable).Path
$version = (Get-Item -LiteralPath $target).VersionInfo
if ($version.ProductVersion -ne $ExpectedVersion -or $version.FileVersion -ne "$ExpectedVersion.0") {
    throw "Unexpected EXE version: $($version.ProductVersion) / $($version.FileVersion)"
}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class DeckStatusResources {
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern IntPtr LoadLibraryEx(string file, IntPtr reserved, uint flags);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern IntPtr FindResource(IntPtr module, IntPtr name, IntPtr type);
    [DllImport("kernel32.dll")] public static extern uint SizeofResource(IntPtr module, IntPtr resource);
    [DllImport("kernel32.dll")] public static extern IntPtr LoadResource(IntPtr module, IntPtr resource);
    [DllImport("kernel32.dll")] public static extern IntPtr LockResource(IntPtr resource);
    [DllImport("kernel32.dll")] public static extern bool FreeLibrary(IntPtr module);
}
'@
# Read resources as data; do not execute the application or load its dependencies.
$module = [DeckStatusResources]::LoadLibraryEx($target, [IntPtr]::Zero, 2)
if ($module -eq [IntPtr]::Zero) { throw 'Could not read executable resources.' }
try {
    $group = [DeckStatusResources]::FindResource($module, [IntPtr]101, [IntPtr]14)
    if ($group -eq [IntPtr]::Zero) { throw 'Application icon group missing.' }
    $length = [DeckStatusResources]::SizeofResource($module, $group)
    $bytes = New-Object byte[] $length
    $data = [DeckStatusResources]::LockResource([DeckStatusResources]::LoadResource($module, $group))
    [Runtime.InteropServices.Marshal]::Copy($data, $bytes, 0, $length)
    $count = [BitConverter]::ToUInt16($bytes, 4)
    if ($count -ne 9 -or $length -ne (6 + 14 * $count)) { throw 'Expected nine icon resolutions.' }
    $sizes = for ($i = 0; $i -lt $count; $i++) {
        $offset = 6 + 14 * $i
        $size = if ($bytes[$offset] -eq 0) { 256 } else { [int]$bytes[$offset] }
        $id = [BitConverter]::ToUInt16($bytes, $offset + 12)
        $resource = [DeckStatusResources]::FindResource($module, [IntPtr]([int]$id), [IntPtr]3)
        if ($resource -eq [IntPtr]::Zero -or [DeckStatusResources]::SizeofResource($module, $resource) -ne [BitConverter]::ToUInt32($bytes, $offset + 8)) {
            throw "Missing or truncated icon image: $size"
        }
        $size
    }
    if (($sizes -join ',') -ne '16,20,24,32,40,48,64,128,256') { throw 'Incorrect icon dimensions.' }
    Write-Host "EXE resources passed: DeckStatus $ExpectedVersion; embedded icons $($sizes -join ', ') px."
} finally { [void][DeckStatusResources]::FreeLibrary($module) }
