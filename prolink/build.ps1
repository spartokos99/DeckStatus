param([ValidateSet('Debug','Release')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$cache=Join-Path $root 'build/prolink-deps'
$package=Join-Path $root 'build/prolink-package'
$classes=Join-Path $root 'build/prolink-classes'
foreach($directory in @($cache,$package,$classes,(Join-Path $package 'lib'),(Join-Path $package 'licenses'))){New-Item -ItemType Directory -Path $directory -Force|Out-Null}
$lock=Get-Content -Raw -Encoding UTF8 (Join-Path $PSScriptRoot 'dependencies.lock.json')|ConvertFrom-Json
foreach($artifact in @($lock.artifacts)+@($lock.runtime)) {
    $path=Join-Path $cache $artifact.name
    if(!(Test-Path -LiteralPath $path)){Invoke-WebRequest $artifact.url -UseBasicParsing -OutFile $path}
    if((Get-FileHash -LiteralPath $path).Hash -ne $artifact.sha256){throw ('Dependency checksum mismatch: '+$artifact.name)}
    if($artifact.name -ne $lock.runtime.name){
        $folder=if($artifact.name.EndsWith('-sources.jar') -or $artifact.name.EndsWith('.pom')){'licenses'}else{'lib'}
        Copy-Item -LiteralPath $path -Destination (Join-Path $package ($folder+'/'+$artifact.name)) -Force
    }
}
$runtime=Join-Path $package 'runtime'
if(!(Test-Path -LiteralPath (Join-Path $runtime 'bin/java.exe'))){
    $extracted=Join-Path $cache 'runtime-extracted'
    if(!(Test-Path -LiteralPath $extracted)){Expand-Archive -LiteralPath (Join-Path $cache $lock.runtime.name) -DestinationPath $extracted}
    $runtimeRoot=@(Get-ChildItem -LiteralPath $extracted -Directory)
    if($runtimeRoot.Count -ne 1){throw 'Unexpected runtime archive layout'}
    New-Item -ItemType Directory -Path $runtime -Force|Out-Null
    Get-ChildItem -LiteralPath $runtimeRoot[0].FullName | Copy-Item -Destination $runtime -Recurse -Force
}
$javaCompiler=(Get-Command javac -ErrorAction Stop).Source
$sources=@(Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'src') -Recurse -Filter '*.java'|ForEach-Object FullName)
& $javaCompiler --release 21 -encoding UTF-8 -cp (Join-Path $package 'lib/*') -d $classes @sources
if($LASTEXITCODE -ne 0){throw 'ProLink Java compilation failed'}
$jar=(Get-Command jar -ErrorAction Stop).Source
& $jar --create --file (Join-Path $package 'DeckStatusProLink.jar') -C $classes .
if($LASTEXITCODE -ne 0){throw 'ProLink packaging failed'}
$testClasses=Join-Path $root 'build/prolink-test-classes'
New-Item -ItemType Directory -Path $testClasses -Force|Out-Null
$testSources=@(Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'tests') -Recurse -Filter '*.java'|ForEach-Object FullName)
& $javaCompiler --release 21 -encoding UTF-8 -cp ($classes+';'+(Join-Path $package 'lib/*')) -d $testClasses @testSources
if($LASTEXITCODE -ne 0){throw 'ProLink Java test compilation failed'}
& (Join-Path $runtime 'bin/java.exe') '-Dorg.slf4j.simpleLogger.defaultLogLevel=off' -cp ($testClasses+';'+$classes+';'+(Join-Path $package 'lib/*')) com.deckstatus.prolink.ModelTest
if($LASTEXITCODE -ne 0){throw 'ProLink Java model tests failed'}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'dependencies.lock.json') -Destination $package -Force
$destination=Join-Path $root ('build/'+$Configuration+'/prolink')
New-Item -ItemType Directory -Path $destination -Force|Out-Null
Get-ChildItem -LiteralPath $package | Copy-Item -Destination $destination -Recurse -Force
Write-Output 'ProLink helper compiled; pinned dependencies and bundled runtime verified.'
