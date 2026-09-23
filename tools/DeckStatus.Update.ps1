param([Parameter(Mandatory=$true)][ValidateSet('Check','Prepare','Download','Apply')][string]$Action,
      [Parameter(Mandatory=$true)][string]$Plan)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$utf8 = [Text.UTF8Encoding]::new($false)
$planFile = [IO.Path]::GetFullPath($Plan)
$job = Split-Path -Parent $planFile
$config = [IO.File]::ReadAllText($planFile) | ConvertFrom-Json
$root = [IO.Path]::GetFullPath($config.root).TrimEnd('\')
$stage = Join-Path $job 'stage'
$archive = Join-Path $job 'package.zip'
$resultFile = Join-Path $job 'result.json'
$parentExited=$false; $installStarted=$false; $applyLock=$null
$managed = @('DeckStatus.exe','DeckStatusBridge.dll','DeckStatus.Update.ps1','Start-ProLink.cmd','README.md','CHANGELOG.md','THIRD_PARTY_NOTICES.md','web','docs','prolink','vendor')
$directories = @('web','docs','prolink','vendor')
function Fail([string]$code) { throw $code }
function Save-Json([string]$path, $value) { [IO.File]::WriteAllText($path,($value | ConvertTo-Json -Depth 20 -Compress),$utf8) }
function No-Reparse([string]$path) {
    $cursor = [IO.Path]::GetFullPath($path)
    while ($cursor) {
        if (Test-Path -LiteralPath $cursor) {
            if (([IO.File]::GetAttributes($cursor) -band [IO.FileAttributes]::ReparsePoint) -ne 0) { Fail 'updateUnsafePath' }
        }
        $parent = [IO.Path]::GetDirectoryName($cursor)
        if ($parent -eq $cursor) { break }; $cursor = $parent
    }
}
function No-TreeLinks([string]$path) {
    No-Reparse $path
    if (![IO.Directory]::Exists($path)) { return }
    $pending = [Collections.Generic.Stack[string]]::new(); $pending.Push($path)
    while ($pending.Count) {
        foreach ($entry in Get-ChildItem -LiteralPath $pending.Pop() -Force) {
            if (($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { Fail 'updateUnsafePath' }
            if ($entry.PSIsContainer) { $pending.Push($entry.FullName) }
        }
    }
}
function Version([string]$value) {
    if ($value -cnotmatch '^v?(0|[1-9][0-9]{0,5})\.(0|[1-9][0-9]{0,5})\.(0|[1-9][0-9]{0,5})$') { Fail 'updateInvalidVersion' }
    return [version]($value.TrimStart('v'))
}
function Request-File([string]$url,[string]$destination,[long]$limit) {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $deadline = [DateTime]::UtcNow.AddMinutes(5)
    for ($redirect = 0; $redirect -le 5; $redirect++) {
        $uri = [Uri]$url
        if ($uri.Scheme -ne 'https' -or $uri.Port -ne 443 -or $uri.UserInfo -or
            $uri.Host -notin @('api.github.com','github.com','release-assets.githubusercontent.com','objects.githubusercontent.com')) { Fail 'updateDownloadUrl' }
        $request = [Net.HttpWebRequest]::Create($uri)
        $request.UserAgent = 'DeckStatus-Updater'; $request.Accept = 'application/vnd.github+json'
        $request.AllowAutoRedirect = $false; $request.Timeout = 10000; $request.ReadWriteTimeout = 10000
        $response = $request.GetResponse()
        try {
            if ([int]$response.StatusCode -in @(301,302,303,307,308)) { $url = [Uri]::new($uri,$response.Headers['Location']).AbsoluteUri; continue }
            if ([int]$response.StatusCode -ne 200 -or $response.ContentLength -gt $limit) { Fail 'updateDownloadFailed' }
            $input = $response.GetResponseStream(); $output = [IO.File]::Create($destination)
            try {
                $buffer = New-Object byte[] 65536; [long]$total = 0
                while (($count = $input.Read($buffer,0,$buffer.Length)) -gt 0) {
                    $total += $count
                    if ($total -gt $limit -or [DateTime]::UtcNow -gt $deadline) { Fail 'updateDownloadFailed' }
                    $output.Write($buffer,0,$count)
                }
                if ($response.ContentLength -ge 0 -and $total -ne $response.ContentLength) { Fail 'updateDownloadFailed' }
            } finally { $output.Dispose(); $input.Dispose() }
            return
        } finally { $response.Dispose() }
    }
    Fail 'updateDownloadFailed'
}
function Latest {
    $file = Join-Path $job 'release.json'
    Request-File 'https://api.github.com/repos/spartokos99/DeckStatus/releases/latest' $file 2097152
    $release = [IO.File]::ReadAllText($file) | ConvertFrom-Json
    if ($release.draft -or $release.prerelease) { Fail 'updateInvalidRelease' }
    $version = Version $release.tag_name
    $name = 'DeckStatus-'+$version.ToString(3)+'-win-x64.zip'
    $assets = @($release.assets | Where-Object { $_.name -ceq $name -and $_.state -eq 'uploaded' })
    $checksum = @($release.assets | Where-Object { $_.name -ceq ($name+'.sha256') -and $_.state -eq 'uploaded' })
    $prefix = 'https://github.com/spartokos99/DeckStatus/releases/download/'+$release.tag_name+'/'
    $download = ''; $digest = ''; $checksumUrl = ''
    if ($assets.Count -eq 1 -and $assets[0].size -gt 0 -and $assets[0].size -le 536870912 -and $assets[0].browser_download_url -ceq ($prefix+$name)) {
        $download = $assets[0].browser_download_url
        if ($assets[0].PSObject.Properties['digest'] -and $assets[0].digest -cmatch '^sha256:([a-f0-9]{64})$') { $digest = $Matches[1] }
        if ($checksum.Count -eq 1 -and $checksum[0].browser_download_url -ceq ($prefix+$name+'.sha256')) { $checksumUrl = $checksum[0].browser_download_url }
    }
    return @{ version=$version.ToString(3); available=($version -gt (Version $config.current));
        url=('https://github.com/spartokos99/DeckStatus/releases/tag/'+$release.tag_name);
        download=$download; digest=$digest; checksumUrl=$checksumUrl; name=$name;
        downloadable=([bool]$download -and ([bool]$digest -or [bool]$checksumUrl)) }
}
function Protected-Paths {
    foreach ($saved in @($config.data,$config.network)) {
        $path = [IO.Path]::GetFullPath($saved).TrimEnd('\')
        $work = Join-Path $root 'DeckStatus.update'
        if ($path.Equals($root,[StringComparison]::OrdinalIgnoreCase) -or $root.StartsWith($path+'\',[StringComparison]::OrdinalIgnoreCase) -or
            $path.Equals($work,[StringComparison]::OrdinalIgnoreCase) -or $path.StartsWith($work+'\',[StringComparison]::OrdinalIgnoreCase)) { Fail 'updateProtectedPath' }
        foreach ($name in $managed) {
            $destination = Join-Path $root $name
            No-Reparse $destination
            if ($path.Equals($destination,[StringComparison]::OrdinalIgnoreCase) -or $path.StartsWith($destination+'\',[StringComparison]::OrdinalIgnoreCase)) { Fail 'updateProtectedPath' }
        }
    }
}
function Prepare {
    Protected-Paths
    if ((Get-Item -LiteralPath $archive).Length -gt 536870912) { Fail 'updateTooLarge' }
    if (Test-Path -LiteralPath $stage) { Fail 'updateUnsafePath' }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::OpenRead($archive)
    try {
        $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase); [long]$total = 0
        if ($zip.Entries.Count -gt 10000 -or $zip.Entries.Count -lt 5) { Fail 'updateInvalidPackage' }
        foreach ($entry in $zip.Entries) {
            $name = $entry.FullName.Replace('\','/'); $parts = $name.TrimEnd('/').Split('/')
            if ($name.Length -gt 240 -or $parts[0] -notin $managed -or ($parts.Count -gt 1 -and $parts[0] -notin $directories) -or
                !$seen.Add($name.TrimEnd('/')) -or (($entry.ExternalAttributes -shr 16) -band 0xF000) -eq 0xA000) { Fail 'updateUnsafeArchive' }
            foreach ($part in $parts) {
                if (!$part -or $part -in @('.','..','DeckStatus.data','DeckStatus.network.json','portal.json') -or
                    $part -match '[<>:"|?*\x00-\x1f]' -or $part -match '[. ]$' -or
                    $part -match '^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$)') { Fail 'updateUnsafeArchive' }
            }
            $total += $entry.Length
            if ($total -gt 1073741824 -or $entry.Length -gt 536870912) { Fail 'updateTooLarge' }
        }
        [void][IO.Directory]::CreateDirectory($stage)
        foreach ($entry in $zip.Entries) {
            $destination = [IO.Path]::GetFullPath((Join-Path $stage $entry.FullName))
            if (!$destination.StartsWith($stage+'\',[StringComparison]::OrdinalIgnoreCase)) { Fail 'updateUnsafeArchive' }
            if (!$entry.Name) { [void][IO.Directory]::CreateDirectory($destination); continue }
            [void][IO.Directory]::CreateDirectory((Split-Path -Parent $destination))
            $input = $entry.Open(); $output = [IO.File]::Open($destination,[IO.FileMode]::CreateNew)
            try {
                $buffer=New-Object byte[] 65536; [long]$written=0
                while (($count=$input.Read($buffer,0,$buffer.Length)) -gt 0) {
                    $written+=$count;if ($written -gt $entry.Length) { Fail 'updateTooLarge' };$output.Write($buffer,0,$count)
                }
                if ($written -ne $entry.Length) { Fail 'updateInvalidPackage' }
            }
            finally { $output.Dispose(); $input.Dispose() }
        }
    } finally { $zip.Dispose() }
    foreach ($name in @('DeckStatus.exe','DeckStatusBridge.dll','web/index.html','web/navigation.js','web/auth.js','web/admin.html',
        'web/overlay.html','web/master-overlay.html','web/scene.html','web/waveform.html','web/locales/en.json','web/locales/de.json',
        'prolink/DeckStatusProLink.jar','prolink/dependencies.lock.json','prolink/runtime/bin/java.exe',
        'prolink/runtime/bin/java.dll','prolink/runtime/bin/server/jvm.dll','prolink/runtime/lib/modules','prolink/runtime/release')) {
        if (![IO.File]::Exists((Join-Path $stage $name))) { Fail 'updateInvalidPackage' }
    }
    $dependencies=[IO.File]::ReadAllText((Join-Path $stage 'prolink/dependencies.lock.json')) | ConvertFrom-Json
    if (@($dependencies.artifacts).Count -lt 1 -or @($dependencies.artifacts).Count -gt 100) { Fail 'updateInvalidPackage' }
    foreach ($dependency in $dependencies.artifacts) {
        if ($dependency.name -cnotmatch '^[A-Za-z0-9_.-]+\.(jar|pom)$' -or $dependency.sha256 -notmatch '^[a-f0-9]{64}$') { Fail 'updateInvalidPackage' }
        $folder=if ($dependency.name.EndsWith('-sources.jar') -or $dependency.name.EndsWith('.pom')) { 'licenses' } else { 'lib' }
        $file=Join-Path $stage ('prolink/'+$folder+'/'+$dependency.name)
        if (![IO.File]::Exists($file) -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ine $dependency.sha256) { Fail 'updateInvalidPackage' }
    }
    $exe = [Diagnostics.FileVersionInfo]::GetVersionInfo((Join-Path $stage 'DeckStatus.exe'))
    $dll = [Diagnostics.FileVersionInfo]::GetVersionInfo((Join-Path $stage 'DeckStatusBridge.dll'))
    if ($exe.ProductName -cne 'DeckStatus' -or $dll.ProductName -cne 'DeckStatus' -or $exe.OriginalFilename -cne 'DeckStatus.exe' -or
        $dll.OriginalFilename -cne 'DeckStatusBridge.dll' -or $exe.ProductVersion -cne $dll.ProductVersion) { Fail 'updateInvalidPackage' }
    $version = Version $exe.ProductVersion
    if ($version -lt (Version $config.current)) { Fail 'updateDowngrade' }
    if ($config.PSObject.Properties['expectedVersion'] -and $version.ToString(3) -cne $config.expectedVersion) { Fail 'updateInvalidPackage' }
    foreach ($binary in @('DeckStatus.exe','DeckStatusBridge.dll')) {
        $stream = [IO.File]::OpenRead((Join-Path $stage $binary)); $reader = [IO.BinaryReader]::new($stream)
        try { if ($reader.ReadUInt16() -ne 0x5A4D) { Fail 'updateInvalidPackage' }; $stream.Position=60; $pe=$reader.ReadUInt32(); $stream.Position=$pe;
            if ($reader.ReadUInt32() -ne 0x4550 -or $reader.ReadUInt16() -ne 0x8664) { Fail 'updateInvalidPackage' } }
        finally { $reader.Dispose() }
    }
    $manifest = @{}
    foreach ($file in Get-ChildItem -LiteralPath $stage -File -Recurse) { $manifest[$file.FullName.Substring($stage.Length+1)] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash }
    Save-Json (Join-Path $job 'manifest.json') $manifest
    return @{ version=$version.ToString(3); sha256=(Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant(); files=$manifest.Count }
}
function Quote-Argument([string]$value) { return '"'+[regex]::Replace([regex]::Replace($value,'(\\*)"','$1$1\"'),'(\\+)$','$1$1')+'"' }
function Restart-App {
    $arguments = @($config.arguments | ForEach-Object { Quote-Argument ([string]$_) }) -join ' '
    $parameters = @{FilePath=(Join-Path $root 'DeckStatus.exe'); WorkingDirectory=$config.workingDirectory; WindowStyle='Hidden'; PassThru=$true}
    if ($arguments) { $parameters.ArgumentList=$arguments }
    return Start-Process @parameters
}
try {
    if ((Split-Path -Leaf $job) -cnotmatch '^job-[a-f0-9]{32}$' -or (Split-Path -Parent $job) -ine (Join-Path $root 'DeckStatus.update')) { Fail 'updateUnsafePath' }
    No-Reparse $job
    if ($Action -eq 'Check') { Save-Json $resultFile @{ok=$true;latest=(Latest)}; exit 0 }
    if ($Action -eq 'Download') {
        $latest = Latest
        if (!$latest.available -or !$latest.downloadable) { Fail 'updateNoDownload' }
        $digest = $latest.digest
        if (!$digest) {
            $checksum = Join-Path $job 'checksum.txt'; Request-File $latest.checksumUrl $checksum 4096
            $line = [IO.File]::ReadAllText($checksum).Trim()
            if ($line -cnotmatch ('^([a-fA-F0-9]{64})\s+\*?'+[regex]::Escape($latest.name)+'$')) { Fail 'updateChecksum' }; $digest = $Matches[1]
        }
        Request-File $latest.download $archive 536870912
        if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ine $digest) { Fail 'updateChecksum' }
        $config | Add-Member -NotePropertyName expectedVersion -NotePropertyValue $latest.version -Force; Save-Json $planFile $config
    }
    if ($Action -in @('Prepare','Download')) { Save-Json $resultFile @{ok=$true;package=(Prepare)}; exit 0 }
    Protected-Paths; No-TreeLinks $stage
    $applyLock=[IO.File]::Open((Join-Path $root 'DeckStatus.update/update.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
    $manifest = [IO.File]::ReadAllText((Join-Path $job 'manifest.json')) | ConvertFrom-Json
    $files = @(Get-ChildItem -LiteralPath $stage -File -Recurse)
    if ($files.Count -ne @($manifest.PSObject.Properties).Count) { Fail 'updateChecksum' }
    foreach ($file in $files) {
        $name=$file.FullName.Substring($stage.Length+1)
        if (!$manifest.PSObject.Properties[$name] -or (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash -cne $manifest.$name) { Fail 'updateChecksum' }
    }
    $parent = Get-Process -Id $config.pid -ErrorAction SilentlyContinue
    if (!$parent -or $parent.StartTime.ToUniversalTime().ToFileTimeUtc().ToString() -cne $config.startTime -or $parent.Path -ine (Join-Path $root 'DeckStatus.exe')) { Fail 'updateParentChanged' }
    foreach ($other in Get-Process -Name DeckStatus -ErrorAction SilentlyContinue) {
        if ($other.Id -ne $parent.Id -and $other.Path -ieq (Join-Path $root 'DeckStatus.exe')) { Fail 'updateOtherInstance' }
    }
    Save-Json (Join-Path $job 'ready.json') @{ok=$true}
    if (!$parent.WaitForExit(120000)) { Fail 'updateShutdownTimeout' }
    $parentExited=$true
    $backup=Join-Path $job 'backup'; [void][IO.Directory]::CreateDirectory($backup)
    # The host has released its data lock. Back up complete private stores before restarting new code.
    No-TreeLinks $config.data; No-Reparse $config.network
    if (Test-Path -LiteralPath $config.data) { Copy-Item -LiteralPath $config.data -Destination (Join-Path $job 'data-backup') -Recurse }
    if (Test-Path -LiteralPath $config.network) { Copy-Item -LiteralPath $config.network -Destination (Join-Path $job 'network-backup.json') }
    $moved=[Collections.Generic.List[string]]::new(); $installed=[Collections.Generic.List[string]]::new(); $launched=$false
    try {
        $installStarted=$true
        foreach ($name in $managed) {
            $source=Join-Path $stage $name
            # Older repair packages may not contain an updater helper.
            if (!(Test-Path -LiteralPath $source)) { continue }
            $destination=Join-Path $root $name; No-Reparse $destination
            if (Test-Path -LiteralPath $destination) { Move-Item -LiteralPath $destination -Destination (Join-Path $backup $name); $moved.Add($name) }
            Move-Item -LiteralPath $source -Destination $destination; $installed.Add($name)
            Save-Json (Join-Path $job 'journal.json') @{moved=@($moved.ToArray());installed=@($installed.ToArray())}
        }
        $result=@{ok=$true;status='updateInstalled';version=([Diagnostics.FileVersionInfo]::GetVersionInfo((Join-Path $root 'DeckStatus.exe'))).ProductVersion;backup=$job}
        Save-Json (Join-Path $root 'DeckStatus.update/last-result.json') $result
        $restarted=Restart-App; $launched=$true
        if ($restarted.WaitForExit(6000)) { Fail 'updateRestartFailed' }
        Save-Json $resultFile $result
    } catch {
        $failure=$_.Exception.Message; $failed=Join-Path $job 'failed'; [void][IO.Directory]::CreateDirectory($failed)
        if ($launched -and !$restarted.HasExited) { $restarted.Kill(); $restarted.WaitForExit() }
        foreach ($name in $installed) { Move-Item -LiteralPath (Join-Path $root $name) -Destination (Join-Path $failed $name) }
        foreach ($name in $moved) { Move-Item -LiteralPath (Join-Path $backup $name) -Destination (Join-Path $root $name) }
        if ($launched) {
            if (Test-Path -LiteralPath (Join-Path $job 'data-backup')) {
                if (Test-Path -LiteralPath $config.data) { No-Reparse $config.data; Move-Item -LiteralPath $config.data -Destination (Join-Path $job 'failed-data') }
                Copy-Item -LiteralPath (Join-Path $job 'data-backup') -Destination $config.data -Recurse
            }
            if (Test-Path -LiteralPath (Join-Path $job 'network-backup.json')) { Copy-Item -LiteralPath (Join-Path $job 'network-backup.json') -Destination $config.network -Force }
        }
        Save-Json (Join-Path $root 'DeckStatus.update/last-result.json') @{ok=$false;status='updateRolledBack';error='updateOperationFailed';backup=$job}
        [void](Restart-App); Fail 'updateRolledBack'
    }
} catch {
    $code=$_.Exception.Message
    if ($code -notmatch '^update[A-Za-z]+$') { $code='updateOperationFailed' }
    if ($Action -eq 'Apply' -and $parentExited) {
        if (!$installStarted) { try { [void](Restart-App) } catch {} }
        elseif ($code -ne 'updateRolledBack') { $code='updateRecoveryNeeded' }
        Save-Json (Join-Path $root 'DeckStatus.update/last-result.json') @{ok=$false;status=$code;backup=$job}
    }
    Save-Json $resultFile @{ok=$false;error=$code}
    exit 1
} finally { if ($applyLock) { $applyLock.Dispose() } }
