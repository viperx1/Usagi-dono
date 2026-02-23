param (
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$TestExe
)

$envFile = Join-Path $PSScriptRoot ".github.env.ps1"
if (Test-Path $envFile) {
    . $envFile
} else {
    Write-Error "Missing .github.env.ps1"
    exit 1
}

$fullVerboseLog = "verbose.log"
$failLog       = "failed_tests.log"

"" | Out-File $fullVerboseLog
"" | Out-File $failLog

$failedTests = @()

# GitHub config
$githubToken = $env:GITHUB_TOKEN
$repo        = $env:GITHUB_REPO
$githubApi   = $env:GITHUB_API ?? "https://api.github.com"

if (-not $githubToken -or -not $repo) {
    Write-Error "GITHUB_TOKEN or GITHUB_REPO not set."
    exit 1
}

$headers = @{
    Authorization = "Bearer $githubToken"
    Accept        = "application/vnd.github+json"
    "User-Agent"  = "qt-test-runner"
}

$commitId = (& git rev-parse --short HEAD 2>$null) -replace "`n",""
if (-not $commitId) { $commitId = "unknown" }

# Detect test executables
if ($TestExe -and $TestExe.Count -gt 0) {
    $testExecutables = @()
    foreach ($exe in $TestExe) {
        if (-not (Test-Path $exe)) {
            Write-Error "Test executable '$exe' not found!"
            exit 1
        }
        $testExecutables += $exe
    }
} else {
    # Search for test executables in build directory
    # Support both simple "build/tests" and Qt Creator-style "build/Desktop_*/tests"
    # Also search in Debug/Release subdirectories which are common in Windows builds
    $testExecutables = @()
    $qtCreatorDirs = @()
    $searchedPaths = @()
    
    # Search in standard CMake build directory
    $standardPath = Join-Path $PSScriptRoot "build" "tests"
    $searchedPaths += $standardPath
    if (Test-Path $standardPath) {
        $testExecutables += Get-ChildItem -Path $standardPath -Filter "test_*.exe" -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName
        
        # Also check Debug and Release subdirectories
        foreach ($config in @("Debug", "Release", "RelWithDebInfo", "MinSizeRel")) {
            $configPath = Join-Path $standardPath $config
            $searchedPaths += $configPath
            if (Test-Path $configPath) {
                $testExecutables += Get-ChildItem -Path $configPath -Filter "test_*.exe" -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName
            }
        }
    }
    
    # Search in Qt Creator build directories (Desktop_*)
    $buildDir = Join-Path $PSScriptRoot "build"
    if (Test-Path $buildDir) {
        $qtCreatorDirs = Get-ChildItem -Path $buildDir -Directory -Filter "Desktop_*" -ErrorAction SilentlyContinue
        foreach ($dir in $qtCreatorDirs) {
            $testsPath = Join-Path $dir.FullName "tests"
            $searchedPaths += $testsPath
            if (Test-Path $testsPath) {
                # Search directly in tests directory
                $testExecutables += Get-ChildItem -Path $testsPath -Filter "test_*.exe" -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName
                
                # Also check Debug and Release subdirectories
                foreach ($config in @("Debug", "Release", "RelWithDebInfo", "MinSizeRel")) {
                    $configPath = Join-Path $testsPath $config
                    $searchedPaths += $configPath
                    if (Test-Path $configPath) {
                        $testExecutables += Get-ChildItem -Path $configPath -Filter "test_*.exe" -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName
                    }
                }
            }
        }
    }
    
    if ($testExecutables.Count -eq 0) {
        Write-Error "No test executables (test_*.exe) found in build directories. Searched: $($searchedPaths -join ', ')"
        exit 1
    }
    
    Write-Host "Found $($testExecutables.Count) test executable(s)"
}

foreach ($exe in $testExecutables) {
    $exeName = Split-Path $exe -Leaf
    Write-Host "=== $exeName ==="

    $tests = & $exe -functions | ForEach-Object {
        $_.Trim() -replace "\(\)$",""
    }

    foreach ($test in $tests) {
        $output = & $exe -v2 $test 2>&1
        $output | Out-File $fullVerboseLog -Append

        if ($output -match "FAIL!") {
            Write-Host "[FAIL] $test"

            $failedTests += [PSCustomObject]@{
                Executable = $exeName
                Test       = $test
                Output     = ($output -join "`n")
            }
        } else {
            Write-Host "[PASS] $test"
        }
    }
}

if ($failedTests.Count -eq 0) {
    Write-Host "All tests passed. No GitHub issue created."
    exit 0
}

# ===== WRITE FAILED TESTS LOG =====

"=== FAILED TESTS SUMMARY ===`n" | Out-File $failLog

foreach ($f in $failedTests) {
    "$($f.Executable) :: $($f.Test)" | Out-File $failLog -Append
}

"`n=== FAILED TEST LOGS ===`n" | Out-File $failLog -Append

foreach ($f in $failedTests) {
    "----- $($f.Executable) :: $($f.Test) -----" | Out-File $failLog -Append
    $f.Output | Out-File $failLog -Append
    "" | Out-File $failLog -Append
}

# ===== CREATE GITHUB ISSUE =====

$summaryLines = $failedTests | ForEach-Object {
    "- **$($_.Executable)** :: $($_.Test)"
}

$issueBody = @"
## Test Failures Detected

Commit: $commitId

The following tests failed during automated execution:

$($summaryLines -join "`n")

Logs for each test are posted as follow-up comments.
"@

$issuePayload = @{
    title = "[FAIL] Automated test failures ($(Get-Date -Format 'yyyy-MM-dd HH:mm'))"
    body  = $issueBody
} | ConvertTo-Json -Depth 5

$issue = Invoke-RestMethod `
    -Method POST `
    -Uri "$githubApi/repos/$repo/issues" `
    -Headers $headers `
    -Body $issuePayload

$issueNumber = $issue.number
Write-Host "Created GitHub issue #$issueNumber"

# ===== POST ONE COMMENT PER FAILED TEST =====

foreach ($f in $failedTests) {

    $commentBody = @"
### $($f.Executable) :: $($f.Test)

---- LOG START ----
$($f.Output)
---- LOG END ----
"@

    $null = Invoke-RestMethod `
    -Method POST `
    -Uri "$githubApi/repos/$repo/issues/$issueNumber/comments" `
    -Headers $headers `
    -Body (@{ body = $commentBody } | ConvertTo-Json -Depth 5)
}

Write-Host "Done. Logs written, issue populated."
