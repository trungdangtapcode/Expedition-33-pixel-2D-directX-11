# ============================================================
# File: prepare_incremental_build.ps1
# Responsibility: Prepare and finalize the manual MSVC incremental build cache.
#
# Owns:
#   - Response files listing stale C++ sources and link object files.
#   - A lightweight build-signature file for flag/config invalidation.
#   - A header timestamp stamp used to trigger conservative full rebuilds.
#
# Lifetime:
#   Created in  -> build_src_static.bat before cl.exe is invoked.
#   Destroyed in -> Not destroyed; cache metadata remains under bin/obj.
#
# Important:
#   - This is not a build generator. The source list still lives in
#     build_src_static.bat as required by the project.
#   - Any header change triggers a full C++ rebuild because this project does
#     not use compiler-emitted dependency files yet.
# ============================================================
param(
    [ValidateSet("Prepare", "Commit")]
    [string]$Mode = "Prepare"
)

$ErrorActionPreference = "Stop"

function Get-RequiredEnvironmentValue {
    param([string]$Name)

    $value = [Environment]::GetEnvironmentVariable($Name)
    if ([string]::IsNullOrWhiteSpace($value)) {
        throw "Required environment variable '$Name' is missing."
    }
    return $value
}

function ConvertTo-ResponseLine {
    param([string]$Path)
    return '"' + $Path + '"'
}

$objectDir = Get-RequiredEnvironmentValue "OBJ_DIR"
$buildMeta = Get-RequiredEnvironmentValue "BUILD_META"
$headerStamp = Get-RequiredEnvironmentValue "HEADER_STAMP"
$buildSignature = Get-RequiredEnvironmentValue "BUILD_SIGNATURE"

if ($Mode -eq "Commit") {
    New-Item -ItemType Directory -Force -Path $objectDir | Out-Null
    Set-Content -Path $buildMeta -Value $buildSignature -Encoding ASCII
    Set-Content -Path $headerStamp -Value ([DateTime]::UtcNow.ToString("o")) -Encoding ASCII
    exit 0
}

$compileResponse = Get-RequiredEnvironmentValue "COMPILE_RSP"
$linkResponse = Get-RequiredEnvironmentValue "LINK_RSP"
$compileCountFile = Get-RequiredEnvironmentValue "COMPILE_COUNT_FILE"
$buildReasonFile = Get-RequiredEnvironmentValue "BUILD_REASON_FILE"
$sourceList = Get-RequiredEnvironmentValue "CL_SOURCES"

New-Item -ItemType Directory -Force -Path $objectDir | Out-Null

$sources = $sourceList -split "\s+" | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
if ($sources.Count -eq 0) {
    throw "No C++ sources were supplied to the incremental build script."
}

$duplicateObjectNames = $sources |
    Group-Object { [IO.Path]::GetFileNameWithoutExtension($_).ToLowerInvariant() } |
    Where-Object { $_.Count -gt 1 }
if ($duplicateObjectNames.Count -gt 0) {
    $names = ($duplicateObjectNames | ForEach-Object { $_.Name }) -join ", "
    throw "Object filename collision detected for: $names. Use unique .cpp basenames or update object naming."
}

$fullRebuild = $false
$reason = "changed source files"

if (-not (Test-Path -LiteralPath $buildMeta)) {
    $fullRebuild = $true
    $reason = "missing build cache metadata"
}
else {
    $previousSignature = Get-Content -LiteralPath $buildMeta -Raw
    if ($previousSignature.Trim() -ne $buildSignature.Trim()) {
        $fullRebuild = $true
        $reason = "build flags or configuration changed"
    }
}

if (-not $fullRebuild) {
    if (-not (Test-Path -LiteralPath $headerStamp)) {
        $fullRebuild = $true
        $reason = "missing header timestamp"
    }
    else {
        $stampTimeUtc = (Get-Item -LiteralPath $headerStamp).LastWriteTimeUtc
        $newerHeader = Get-ChildItem -Path "src" -Recurse -File -Include "*.h", "*.hpp", "*.inl" |
            Where-Object { $_.LastWriteTimeUtc -gt $stampTimeUtc } |
            Select-Object -First 1
        if ($null -ne $newerHeader) {
            $fullRebuild = $true
            $reason = "header changed: $($newerHeader.FullName)"
        }
    }
}

$staleSources = New-Object System.Collections.Generic.List[string]
$objectLines = New-Object System.Collections.Generic.List[string]

foreach ($source in $sources) {
    $sourceItem = Get-Item -LiteralPath $source
    $objectPath = Join-Path $objectDir ([IO.Path]::GetFileNameWithoutExtension($source) + ".obj")
    $objectLines.Add((ConvertTo-ResponseLine $objectPath))

    if ($fullRebuild -or -not (Test-Path -LiteralPath $objectPath)) {
        $staleSources.Add($source)
        continue
    }

    $objectItem = Get-Item -LiteralPath $objectPath
    if ($sourceItem.LastWriteTimeUtc -gt $objectItem.LastWriteTimeUtc) {
        $staleSources.Add($source)
    }
}

$compileLines = $staleSources | ForEach-Object { ConvertTo-ResponseLine $_ }
Set-Content -Path $compileResponse -Value $compileLines -Encoding ASCII
Set-Content -Path $linkResponse -Value $objectLines -Encoding ASCII
Set-Content -Path $compileCountFile -Value $staleSources.Count -Encoding ASCII

if ($staleSources.Count -eq 0) {
    Set-Content -Path $buildReasonFile -Value "object cache is current" -Encoding ASCII
}
else {
    Set-Content -Path $buildReasonFile -Value $reason -Encoding ASCII
}
