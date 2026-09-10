# Copyright (c) 2026 RegAnvil contributors. SPDX-License-Identifier: MIT
<#
.SYNOPSIS
Runs local RegAnvil release checks without installing system files or publishing.
.DESCRIPTION
Creates a fresh directory under .build/verification and runs Debug, Release,
ASan/UBSan, and library-only builds. Every full CTest run includes the standalone
source and relocated installed-package consumers. Existing tools are required;
this workspace's ignored .build/env.ps1 is used if CMake is not on PATH.
For a compiler without the optional ASan/UBSan combination, explicitly pass
-SkipSanitizers. Any requested build or test failure terminates with an error.
.EXAMPLE
./scripts/verify.ps1
.EXAMPLE
./scripts/verify.ps1 -Generator 'Visual Studio 17 2022' -SkipSanitizers
#>
[CmdletBinding()]
param(
    [string]$Generator = '',
    [string]$Compiler = '',
    [switch]$SkipSanitizers
)

$ErrorActionPreference = 'Stop'
$reganvilRoot = Split-Path -Parent $PSScriptRoot
$localEnvironment = Join-Path $reganvilRoot '.build/env.ps1'
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    if (Test-Path -LiteralPath $localEnvironment) {
        . $localEnvironment
    }
}
foreach ($program in @('cmake', 'ctest')) {
    if (-not (Get-Command $program -ErrorAction SilentlyContinue)) {
        throw "RegAnvil: $program was not found. Install a local build toolchain or add it to PATH."
    }
}
if (-not $Compiler -and $env:CC) {
    $Compiler = $env:CC
}
if (-not $Generator -and (Get-Command ninja -ErrorAction SilentlyContinue) -and
    ($Compiler -or (Get-Command cl -ErrorAction SilentlyContinue))) {
    $Generator = 'Ninja'
}

$runName = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 8)
$verificationRoot = Join-Path $reganvilRoot '.build/verification'
$runRoot = Join-Path $verificationRoot $runName
$logRoot = Join-Path $runRoot 'logs'
New-Item -ItemType Directory -Path $logRoot -Force | Out-Null

function Invoke-ReganvilCheck {
    param([string]$Program, [string[]]$Arguments, [string]$LogName)
    $logPath = Join-Path $logRoot $LogName
    Write-Host ('RegAnvil: ' + $Program + ' ' + ($Arguments -join ' '))
    # Windows PowerShell 5.1 wraps native stderr in ErrorRecord objects. A
    # successful compiler warning must not bypass the explicit exit-code check.
    $previousErrorPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & $Program @Arguments 2>&1 | Tee-Object -FilePath $logPath -ErrorAction Stop
        $commandExit = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorPreference
    }
    if ($commandExit -ne 0) {
        throw "RegAnvil: command failed with exit code $commandExit. See $logPath"
    }
}

$configureArguments = @()
if ($Generator) { $configureArguments += @('-G', $Generator) }
if ($Compiler) { $configureArguments += "-DCMAKE_C_COMPILER=$Compiler" }
$completed = @()
$summary = [ordered]@{
    status = 'running'
    directory = $runRoot
    completed = @()
    sanitizers = $(if ($SkipSanitizers) { 'explicitly skipped' } else { 'requested' })
}
try {
    Invoke-ReganvilCheck -Program cmake -Arguments @('--version') -LogName 'cmake-version.log'
    foreach ($configuration in @('Debug', 'Release', 'Sanitized', 'Library')) {
        if ($configuration -eq 'Sanitized' -and $SkipSanitizers) { continue }
        $buildRoot = Join-Path $runRoot $configuration.ToLowerInvariant()
        $buildType = if ($configuration -eq 'Library') { 'Release' } elseif ($configuration -eq 'Sanitized') { 'Debug' } else { $configuration }
        # Explicit options keep the requested verification scope independent of
        # toolchain defaults. Library-only builds do not initialize CTest.
        $withTests = if ($configuration -eq 'Library') { 'OFF' } else { 'ON' }
        $withSanitizers = if ($configuration -eq 'Sanitized') { 'ON' } else { 'OFF' }
        $options = @('-S', $reganvilRoot, '-B', $buildRoot) + $configureArguments + @(
            "-DCMAKE_BUILD_TYPE=$buildType",
            "-DMXS_REGANVIL_BUILD_TESTS=$withTests",
            "-DMXS_REGANVIL_BUILD_EXAMPLES=$withTests",
            "-DMXS_REGANVIL_ENABLE_SANITIZERS=$withSanitizers")
        if ($configuration -ne 'Library') { $options += '-DBUILD_TESTING=ON' }
        Invoke-ReganvilCheck -Program cmake -Arguments $options -LogName "$configuration-configure.log"
        Invoke-ReganvilCheck -Program cmake -Arguments @('--build', $buildRoot, '--config', $buildType, '--parallel') -LogName "$configuration-build.log"
        if ($configuration -ne 'Library') {
            # UBSan recovery is disabled for this run; sanitizer reports must fail.
            $previousAsan = $env:ASAN_OPTIONS
            $previousUbsan = $env:UBSAN_OPTIONS
            try {
                if ($configuration -eq 'Sanitized') {
                    $env:ASAN_OPTIONS = 'halt_on_error=1'
                    $env:UBSAN_OPTIONS = 'halt_on_error=1:print_stacktrace=1'
                }
                Invoke-ReganvilCheck -Program ctest -Arguments @('--test-dir', $buildRoot, '-C', $buildType, '--output-on-failure', '--no-tests=error') -LogName "$configuration-tests.log"
            } finally {
                $env:ASAN_OPTIONS = $previousAsan
                $env:UBSAN_OPTIONS = $previousUbsan
            }
        }
        $completed += $configuration
    }
    $summary.status = 'passed'
    if (-not $SkipSanitizers) { $summary.sanitizers = 'passed' }
} catch {
    $summary.status = 'failed'
    $summary.error = $_.Exception.Message
    throw
} finally {
    $summary.completed = $completed
    $json = $summary | ConvertTo-Json -Depth 3
    $json | Set-Content -LiteralPath (Join-Path $runRoot 'summary.json') -Encoding UTF8
    $json | Set-Content -LiteralPath (Join-Path $verificationRoot 'latest.json') -Encoding UTF8
}
Write-Host ('RegAnvil: requested checks passed. Logs: ' + $logRoot)
if ($SkipSanitizers) { Write-Host 'RegAnvil: sanitizer checks were explicitly skipped.' }
