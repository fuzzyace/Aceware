# Aceware - full build.
# Incremental: a source is compiled only when its object is missing or older.
# Run from anywhere:  powershell -ExecutionPolicy Bypass -File build.ps1

$ErrorActionPreference = 'Stop'

$np = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path) # project root (script lives in tools/)
$tc = Join-Path (Split-Path -Parent $np) 'toolchain'

$msvc    = Join-Path $tc 'msvc\VC\Tools\MSVC\14.44.35207'
$sdkBase = Join-Path $tc 'sdk_nuget\microsoft.windows.sdk.cpp\c'
$sdkX64  = Join-Path $tc 'sdk_nuget\microsoft.windows.sdk.cpp.x64\c'
$geodeInc = Join-Path $tc 'dl\sdk-extract\geode-5.11.2\loader\include'
$bindings = Join-Path $tc 'dl\bindings-extract\bindings-main\bindings'
$codegen  = Join-Path $tc 'codegen-out'
$linkDir  = Join-Path $geodeInc 'link\win64'

$env:PATH = (Join-Path $msvc 'bin\Hostx64\x64') + ';' + $env:PATH
$env:INCLUDE = (@(
    (Join-Path $msvc 'include')
    (Join-Path $sdkBase 'Include\10.0.26100.0\ucrt')
    (Join-Path $sdkBase 'Include\10.0.26100.0\shared')
    (Join-Path $sdkBase 'Include\10.0.26100.0\um')
    (Join-Path $sdkBase 'Include\10.0.26100.0\winrt')
) -join ';')
$env:LIB = (@(
    (Join-Path $msvc 'lib\x64')
    (Join-Path $sdkX64 'ucrt\x64')
    (Join-Path $sdkX64 'um\x64')
) -join ';')

$build = Join-Path $np 'build'
foreach ($d in @('obj\gen','obj\inline','obj\tools','obj\arc')) { New-Item -ItemType Directory -Force -Path (Join-Path $build $d) | Out-Null }

# ---- include switches -------------------------------------------------------
$inc = @('/nologo','/c','/std:c++latest','/EHsc','/MD','/bigobj','/utf-8','/O2','/MP')
$inc += '/I"' + $geodeInc + '"'
$inc += '/I"' + (Join-Path $geodeInc 'Geode\cocos') + '"'
$inc += '/I"' + (Join-Path $geodeInc 'Geode\fmod') + '"'
$inc += '/I"' + $codegen + '"'
$inc += '/I"' + (Join-Path $bindings 'include') + '"'
$inc += '/I"' + (Join-Path $np 'src') + '"'
Get-ChildItem (Join-Path $geodeInc 'Geode\cocos') -Directory -Recurse |
    Where-Object { $_.Name -ne 'compat' -and (Get-ChildItem $_.FullName -Filter *.h -File -ErrorAction SilentlyContinue) } |
    ForEach-Object { $inc += '/I"' + $_.FullName + '"' }
Get-ChildItem (Join-Path $tc 'deps') -Directory |
    ForEach-Object { $inc += '/I"' + (Join-Path $_.FullName 'include') + '"' }
$inc += '/FI"' + (Join-Path $np 'src\ProjectDefs.hpp') + '"'
Set-Content (Join-Path $build 'inc.rsp') ($inc -join "`n") -Encoding ASCII

# ---- compile helper ---------------------------------------------------------
$script:failed = @()
function Build-One([string]$src, [string]$objDir, [string]$extra = '') {
    $name = [IO.Path]::GetFileNameWithoutExtension($src)
    $obj  = Join-Path $objDir ($name + '.obj')
    if ((Test-Path $obj) -and ((Get-Item $obj).LastWriteTime -gt (Get-Item $src).LastWriteTime)) { return }
    $log = Join-Path $build ('log_' + $name + '.txt')
    $clArgs = @('@' + (Join-Path $build 'inc.rsp'))
    if ($extra) { $clArgs += $extra }
    $clArgs += @('"' + $src + '"', '/Fo"' + $objDir + '\\"')
    $out = & cmd /c ("cl " + ($clArgs -join ' ') + ' 2>&1')
    $out | Set-Content $log
    if ($LASTEXITCODE -ne 0) { $script:failed += $name; Write-Host ("  FAILED " + $name) -ForegroundColor Red }
}

# ---- pass A: the mod + generated bindings ----------------------------------
Write-Host '[1/5] generated bindings + mod'
# three generated sources need a concrete type before their includes resolve
Build-One (Join-Path $codegen 'Geode\source\BoomListView.cpp')        (Join-Path $build 'obj\gen') '/FI"Geode/binding/CCIndexPath.hpp"'
Build-One (Join-Path $codegen 'Geode\source\EnterEffectInstance.cpp') (Join-Path $build 'obj\gen') '/FI"Geode/binding/EnterEffectAnimValue.hpp"'
Build-One (Join-Path $codegen 'Geode\source\GJUINode.cpp')            (Join-Path $build 'obj\gen') '/FI"Geode/binding/UIButtonConfig.hpp"'

$genSources = @()
$genSources += Join-Path $np 'src\main.cpp'
$genSources += Get-ChildItem (Join-Path $codegen 'Geode') -Filter *.cpp | ForEach-Object { $_.FullName }
$genSources += Get-ChildItem (Join-Path $codegen 'Geode\source') -Filter *.cpp |
    Where-Object { $_.Name -notin @('BoomListView.cpp','EnterEffectInstance.cpp','GJUINode.cpp') } |
    ForEach-Object { $_.FullName }
foreach ($s in $genSources) { Build-One $s (Join-Path $build 'obj\gen') }

# ---- pass B: inline bindings ------------------------------------------------
Write-Host '[2/5] inline bindings'
# these two use the old _ftime64 family; force the header in
Build-One (Join-Path $bindings '2.2081\inline\DialogLayer.cpp')      (Join-Path $build 'obj\inline') '/FI"sys/timeb.h"'
Build-One (Join-Path $bindings '2.2081\inline\GameStatsManager.cpp') (Join-Path $build 'obj\inline') '/FI"sys/timeb.h"'
Get-ChildItem (Join-Path $bindings '2.2081\inline') -Filter *.cpp |
    Where-Object { $_.Name -notin @('DialogLayer.cpp','GameStatsManager.cpp') } |
    ForEach-Object { Build-One $_.FullName (Join-Path $build 'obj\inline') }

# ---- pass C: things the import library does not provide ---------------------
Write-Host '[3/5] support objects'
Build-One (Join-Path $tc 'deps\asp2\src\Log.cpp')              (Join-Path $build 'obj\tools')
Build-One (Join-Path $tc 'deps\asp2\src\time\Instant.cpp')     (Join-Path $build 'obj\tools')
Build-One (Join-Path $tc 'deps\asp2\src\time\Duration.cpp')    (Join-Path $build 'obj\tools')
Build-One (Join-Path $tc 'deps\fmt\src\format.cc')             (Join-Path $build 'obj\tools') '/DFMT_STATIC'
Build-One (Join-Path $tc 'deps\fmt\src\os.cc')                 (Join-Path $build 'obj\tools') '/DFMT_STATIC'
# Geode's own mod entry point: defines geodeImplicitEntry + geode::getMod.
Build-One (Join-Path $tc 'dl\sdk-extract\geode-5.11.2\entry.cpp') (Join-Path $build 'obj\tools')
# asp pieces the entry and the arc runtime need.
Build-One (Join-Path $tc 'deps\asp2\src\platform\windows\Notify.cpp') (Join-Path $build 'obj\tools')
Build-One (Join-Path $tc 'deps\asp2\src\thread\Thread.cpp')           (Join-Path $build 'obj\tools')

# ---- pass D: the arc runtime the entry initialises -------------------------
Write-Host '[4/5] arc runtime'
foreach ($rel in @('src\Util.cpp','src\future\Context.cpp','src\future\Pollable.cpp',
                   'src\runtime\Runtime.cpp','src\sync\Notify.cpp','src\sync\Semaphore.cpp',
                   'src\task\Task.cpp','src\task\Waker.cpp')) {
    Build-One (Join-Path $tc ('deps\arc\' + $rel)) (Join-Path $build 'obj\arc')
}

if ($script:failed.Count -gt 0) {
    Write-Host ('BUILD FAILED - ' + $script:failed.Count + ' object(s): ' + ($script:failed -join ', ')) -ForegroundColor Red
    exit 1
}

# ---- link -------------------------------------------------------------------
Write-Host '[5/5] linking'
$outDll = Join-Path $build 'ace.aceware.dll'
$link = @('/nologo','/DLL',('/OUT:"' + $outDll + '"'))
$link += Get-ChildItem (Join-Path $build 'obj\gen')    -Filter *.obj | ForEach-Object { '"' + $_.FullName + '"' }
$link += Get-ChildItem (Join-Path $build 'obj\inline') -Filter *.obj | ForEach-Object { '"' + $_.FullName + '"' }
$link += Get-ChildItem (Join-Path $build 'obj\tools')  -Filter *.obj | ForEach-Object { '"' + $_.FullName + '"' }
$link += Get-ChildItem (Join-Path $build 'obj\arc')    -Filter *.obj | ForEach-Object { '"' + $_.FullName + '"' }
$link += @(
    '"' + (Join-Path $tc 'geode-lib\Geode.lib') + '"'
    '"' + (Join-Path $linkDir 'libcocos2d.lib') + '"'
    '"' + (Join-Path $linkDir 'libExtensions.lib') + '"'
    '"' + (Join-Path $linkDir 'fmod.lib') + '"'
    '"' + (Join-Path $linkDir 'glew32.lib') + '"'
    '"' + (Join-Path $linkDir 'gd-libcurl.lib') + '"'
    'opengl32.lib','user32.lib','gdi32.lib','winmm.lib','shell32.lib','advapi32.lib','ws2_32.lib','ole32.lib','version.lib'
)
Set-Content (Join-Path $build 'link.rsp') ($link -join "`n") -Encoding ASCII
& cmd /c ('link @' + (Join-Path $build 'link.rsp') + ' > ' + (Join-Path $build 'link.log') + ' 2>&1')
if ($LASTEXITCODE -ne 0) {
    Get-Content (Join-Path $build 'link.log') | Select-String 'error LNK' | Select-Object -First 15 | ForEach-Object { $_.Line }
    Write-Host 'LINK FAILED' -ForegroundColor Red
    exit 1
}

$dll = Get-Item (Join-Path $build 'ace.aceware.dll')
Write-Host ('done: ' + $dll.FullName + '  (' + [math]::Round($dll.Length / 1MB, 2) + ' MB)') -ForegroundColor Green
