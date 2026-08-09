# Windows One-Click Build Scripts Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a safe PowerShell build/test/demo driver and a double-clickable CMD wrapper for the existing CMake project.

**Architecture:** `build.ps1` is the only build-logic owner: it resolves tools, validates a repository-local build directory, configures CMake, builds, tests, and optionally runs the demo. `build.cmd` is a thin Windows entry point that forwards arguments, preserves the PowerShell exit code, and pauses for double-click users. A standalone PowerShell integration test executes the real scripts and real project rather than inspecting source text.

**Tech Stack:** PowerShell 5.1-compatible syntax, Windows CMD, CMake 3.20+, Visual Studio 18 2026 generator, MSVC, CTest.

## Global Constraints

- Create only `build.ps1`, `build.cmd`, and `tests/tool/test_build_script.ps1`; modify only `README.md` for usage documentation.
- Do not modify framework C code, CMake targets, layer rules, or runtime behavior.
- Do not install tools, access the network, or run Git from either script.
- Default behavior is Debug + host port + configure + build + all tests; demo is opt-in with `-Demo`.
- `-Port none` disables tests/examples and rejects `-Demo`.
- `-Clean` may delete only the exact resolved build directory strictly below the repository root.
- All native-command failures must stop immediately and propagate a nonzero exit code.
- Tests exercise observable exit codes, files, libraries, CTest output, and demo output; they must not assert source text.

---

### Task 1: PowerShell build, test, clean, and demo driver

**Files:**
- Create: `tests/tool/test_build_script.ps1`
- Create: `build.ps1`

**Interfaces:**
- Consumes: existing CMake options `SENSOR_PORT`, `BUILD_TESTING`, `SENSOR_BUILD_EXAMPLES`; existing targets and `host_demo`.
- Produces: `build.ps1 -Configuration <Debug|Release> -BuildDir <relative> -Port <host|none> -Clean -Demo`.

- [ ] **Step 1: Write the failing real-behavior test**

Create `tests/tool/test_build_script.ps1` with these helpers and test flow:

```powershell
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$scriptPath = Join-Path $repoRoot 'build.ps1'

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

function Invoke-BuildScript {
    param([string[]]$Arguments, [string]$WorkingDirectory = $repoRoot)
    Push-Location $WorkingDirectory
    try {
        $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass `
            -File $scriptPath @Arguments 2>&1 | Out-String
        return @{ ExitCode = $LASTEXITCODE; Output = $output }
    } finally {
        Pop-Location
    }
}

Assert-True (Test-Path -LiteralPath $scriptPath) 'build.ps1 is missing'

$invalid = Invoke-BuildScript -Arguments @('-Port', 'none', '-Demo')
Assert-True ($invalid.ExitCode -ne 0) 'none port unexpectedly accepted -Demo'

$outside = Join-Path ([IO.Path]::GetTempPath()) `
    ('sensor-build-guard-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $outside | Out-Null
$sentinel = Join-Path $outside 'keep.txt'
Set-Content -LiteralPath $sentinel -Value 'keep'
try {
    $unsafe = Invoke-BuildScript -Arguments @(
        '-Port', 'none', '-BuildDir', $outside, '-Clean')
    Assert-True ($unsafe.ExitCode -ne 0) 'absolute Clean target was accepted'
    Assert-True (Test-Path -LiteralPath $sentinel) 'outside sentinel was deleted'
} finally {
    Remove-Item -LiteralPath $outside -Recurse -Force
}

$none = Invoke-BuildScript -Arguments @(
    '-Port', 'none', '-Configuration', 'Release',
    '-BuildDir', 'build-script-none', '-Clean') `
    -WorkingDirectory ([IO.Path]::GetTempPath())
Assert-True ($none.ExitCode -eq 0) "none build failed:`n$($none.Output)"
Assert-True (Test-Path (Join-Path $repoRoot `
    'build-script-none\Release\sensor_func.lib')) 'sensor_func.lib is missing'
Assert-True (-not (Test-Path (Join-Path $repoRoot `
    'build-script-none\Release\host_demo.exe'))) 'none build created host_demo'

$host = Invoke-BuildScript -Arguments @(
    '-BuildDir', 'build-script-host', '-Clean', '-Demo') `
    -WorkingDirectory ([IO.Path]::GetTempPath())
Assert-True ($host.ExitCode -eq 0) "host build failed:`n$($host.Output)"
Assert-True ($host.Output -match '100% tests passed') 'CTest success is missing'
Assert-True ($host.Output -match '\[SUMMARY\].*MQTT_pending=0') `
    'demo summary is missing'

Write-Host 'build.ps1 integration tests passed'
```

The production change that makes each assertion fail is respectively: missing driver, accepting an invalid mode, deleting outside the repository, linking the host port in `none` mode, skipping CTest, or skipping the demo.

- [ ] **Step 2: Run the test and verify RED**

Run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tests\tool\test_build_script.ps1
```

Expected: nonzero exit with `build.ps1 is missing`. No configure or build command should run.

- [ ] **Step 3: Implement the minimal `build.ps1`**

Create `build.ps1` using this structure:

```powershell
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [string]$BuildDir = 'build',
    [ValidateSet('host', 'none')]
    [string]$Port = 'host',
    [switch]$Clean,
    [switch]$Demo
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path $PSScriptRoot).Path

function Resolve-BuildDirectory {
    param([string]$Root, [string]$RelativePath)
    if ([string]::IsNullOrWhiteSpace($RelativePath) -or
        [IO.Path]::IsPathRooted($RelativePath)) {
        throw 'BuildDir must be a non-empty relative path.'
    }
    $resolved = [IO.Path]::GetFullPath((Join-Path $Root $RelativePath))
    $prefix = $Root.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    if (($resolved -eq $Root) -or
        (-not $resolved.StartsWith($prefix,
            [StringComparison]::OrdinalIgnoreCase))) {
        throw 'BuildDir must resolve below the repository root.'
    }
    return $resolved
}

function Find-CMake {
    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($null -ne $command) { return $command.Source }
    $known = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    if (Test-Path -LiteralPath $known) { return $known }
    throw 'CMake was not found in PATH or Visual Studio Build Tools 18.'
}

function Find-CTest {
    param([string]$CMakePath)
    $sibling = Join-Path (Split-Path $CMakePath -Parent) 'ctest.exe'
    if (Test-Path -LiteralPath $sibling) { return $sibling }
    $command = Get-Command ctest.exe -ErrorAction SilentlyContinue
    if ($null -ne $command) { return $command.Source }
    throw 'CTest was not found next to CMake or in PATH.'
}

function Invoke-Checked {
    param([string]$FilePath, [string[]]$Arguments, [string]$Stage)
    Write-Host "`n== $Stage =="
    & $FilePath @Arguments
    $code = $LASTEXITCODE
    if ($code -ne 0) {
        Write-Error "$Stage failed with exit code $code."
        exit $code
    }
}
```

After the helpers, validate `if (($Port -eq 'none') -and $Demo) { throw ... }`, resolve the build directory, and delete it only after all safety checks when `-Clean` is present. Configure with literal arguments:

```powershell
$tests = if ($Port -eq 'host') { 'ON' } else { 'OFF' }
$examples = if ($Port -eq 'host') { 'ON' } else { 'OFF' }
$configure = @('-S', $repoRoot, '-B', $buildPath,
    '-G', 'Visual Studio 18 2026', '-A', 'x64',
    "-DSENSOR_PORT=$Port", "-DBUILD_TESTING=$tests",
    "-DSENSOR_BUILD_EXAMPLES=$examples")
Invoke-Checked $cmake $configure 'Configure'
Invoke-Checked $cmake @('--build', $buildPath, '--config', $Configuration) 'Build'
```

For host mode, locate CTest and run:

```powershell
Invoke-Checked $ctest @('--test-dir', $buildPath, '-C', $Configuration,
    '--output-on-failure') 'Test'
```

For `-Demo`, require `$buildPath\$Configuration\host_demo.exe` and invoke it through `Invoke-Checked`. Wrap validation/tool-discovery failures in `try/catch`, print `[ERROR] <message>`, and `exit 1`. Print `[OK] Build workflow completed.` only after every requested stage succeeds.

- [ ] **Step 4: Run the integration test and verify GREEN**

Run the same command from Step 2.

Expected: exit 0, `build.ps1 integration tests passed`, a successful Release `none` build, 12/12 host CTest, and a demo `[SUMMARY]` line.

- [ ] **Step 5: Run the existing project tests again**

Run:

```powershell
& '.\build-script-host\Debug\test_tool.exe'
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' `
  --test-dir build-script-host -C Debug --output-on-failure
```

Expected: both commands exit 0; CTest reports 12/12 passed.

- [ ] **Step 6: Commit Task 1**

```powershell
git add -- build.ps1 tests/tool/test_build_script.ps1
git commit -m "feat: add one-click PowerShell build workflow"
```

---

### Task 2: Double-click CMD wrapper

**Files:**
- Modify: `tests/tool/test_build_script.ps1`
- Create: `build.cmd`

**Interfaces:**
- Consumes: `build.ps1` and its exact parameters/exit codes from Task 1.
- Produces: `build.cmd [build.ps1 arguments]`, with argument forwarding, visible result, pause, and preserved exit code.

- [ ] **Step 1: Extend the test with a failing wrapper invocation**

Append:

```powershell
$cmdPath = Join-Path $repoRoot 'build.cmd'
Assert-True (Test-Path -LiteralPath $cmdPath) 'build.cmd is missing'
$cmdLine = 'echo.|"{0}" -Port none -Demo' -f $cmdPath
$cmdOutput = & cmd.exe /d /c $cmdLine `
    2>&1 | Out-String
$cmdCode = $LASTEXITCODE
Assert-True ($cmdCode -ne 0) 'build.cmd lost the PowerShell failure code'
Assert-True ($cmdOutput -match '\[ERROR\].*Demo') `
    'build.cmd did not forward arguments to build.ps1'
Write-Host 'build.ps1 and build.cmd integration tests passed'
```

- [ ] **Step 2: Run the test and verify RED**

Run the Task 1 test command.

Expected: the earlier PowerShell integration checks remain green, then the run fails with `build.cmd is missing`.

- [ ] **Step 3: Implement the minimal `build.cmd`**

```bat
@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*
set "BUILD_EXIT=%ERRORLEVEL%"
echo.
if "%BUILD_EXIT%"=="0" (
  echo [OK] Build workflow completed.
) else (
  echo [ERROR] Build workflow failed with exit code %BUILD_EXIT%.
)
pause
exit /b %BUILD_EXIT%
```

- [ ] **Step 4: Run the test and verify GREEN**

Run the Task 1 test command again.

Expected: exit 0 and `build.ps1 integration tests passed`; the internal CMD probe must observe a nonzero forwarded error for the intentionally invalid mode without hanging at `pause`.

- [ ] **Step 5: Manually verify the default CMD path**

Run from `E:\project`:

```powershell
cmd.exe /d /c "echo.|build.cmd -BuildDir build-script-cmd"
```

Expected: configure/build/test succeeds, output reports 12/12, CMD prints `[OK]`, consumes the piped key for `pause`, and exits 0.

- [ ] **Step 6: Commit Task 2**

```powershell
git add -- build.cmd tests/tool/test_build_script.ps1
git commit -m "feat: add double-click build entry point"
```

---

### Task 3: README and final verification

**Files:**
- Modify: `README.md`

**Interfaces:**
- Consumes: the exact script parameters and defaults delivered by Tasks 1 and 2.
- Produces: user-facing one-click, command-line, clean, demo, Release, and MCU/no-port examples.

- [ ] **Step 1: Update the Windows build section**

Replace the primary manual CMake example with:

```powershell
.\build.ps1
.\build.ps1 -Demo
.\build.ps1 -Configuration Release -Clean
.\build.ps1 -Port none -Configuration Release -BuildDir build-mcu
```

Document that `build.cmd` can be double-clicked for the default configure/build/test flow, while advanced users should call `build.ps1` to avoid the CMD pause. Keep the manual CMake commands in a clearly labelled fallback subsection.

- [ ] **Step 2: Run whitespace and script integration checks**

```powershell
git diff --check
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tests\tool\test_build_script.ps1
```

Expected: both exit 0.

- [ ] **Step 3: Run the project test suite from the script-produced build**

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' `
  --test-dir build-script-host -C Debug --output-on-failure
```

Expected: 12/12 passed.

- [ ] **Step 4: Review destructive scope and repository status**

```powershell
git status --short
git diff -- build.ps1 build.cmd tests/tool/test_build_script.ps1 README.md
```

Expected: only intended files are changed; `.claude/` remains untouched; no build directory is tracked.

- [ ] **Step 5: Commit documentation**

```powershell
git add -- README.md
git commit -m "docs: document one-click Windows builds"
```

- [ ] **Step 6: Final commit-state verification**

```powershell
git status --short --branch
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tests\tool\test_build_script.ps1
```

Expected: only the pre-existing untracked `.claude/` remains; all script integration checks and the underlying 12 CTest targets pass.
