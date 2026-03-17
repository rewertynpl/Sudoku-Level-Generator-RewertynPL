@echo off
setlocal EnableExtensions EnableDelayedExpansion

cd /d "%~dp0"
set "REPO=%CD%"
set "EXE=%REPO%\sudoku_gen_test.exe"
set "REPORT_DIR=%REPO%\Debugs\smoke_reports"
set "SESSION_LOG=%REPORT_DIR%\all_strategies_exact_smoke_session.txt"
set "SUMMARY_REPORT=%REPORT_DIR%\all_strategies_exact_summary.txt"
set "APPVERIF_LOG=%REPORT_DIR%\all_strategies_appverif_log.txt"
set "MAX_TOTAL_TIME_DEFAULT=83"
set "MAX_TOTAL_TIME_EASY=33"
set "MAX_ATTEMPTS=0"
set "THREADS=12"

if not exist "%REPORT_DIR%" mkdir "%REPORT_DIR%"
break > "%SESSION_LOG%"
break > "%SUMMARY_REPORT%"
break > "%APPVERIF_LOG%"

net session >nul 2>&1
if errorlevel 1 (
    echo [ERROR] This smoke launcher must be run from an elevated CMD session.
    exit /b 1
)

where appverif.exe >nul 2>&1
if errorlevel 1 (
    echo [ERROR] appverif.exe was not found in PATH.
    exit /b 1
)

call "%REPO%\kompilacja.bat" --no-pause
if errorlevel 1 exit /b 1

if not exist "%EXE%" (
    echo [ERROR] Missing binary: "%EXE%"
    exit /b 1
)

echo [INFO] Enabling AppVerifier for sudoku_gen_test.exe
echo [INFO] Enabling AppVerifier for sudoku_gen_test.exe>> "%SESSION_LOG%"
(
    echo ===== APPVERIF ENABLE =====
    appverif.exe /verify "%EXE%"
    echo.
) >> "%APPVERIF_LOG%" 2>&1
appverif.exe /verify "%EXE%" >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Failed to enable AppVerifier.
    >> "%SESSION_LOG%" echo [ERROR] Failed to enable AppVerifier.
    exit /b 1
)

call :run_all_strategies
if errorlevel 1 goto cleanup_fail

goto cleanup_ok

:run_all_strategies
for %%L in (
    "nakedsingle 3 3 1 p1 nofast primary 1"
    "nakedsingle 4 3 1 p1 nofast asymmetric 1"
    "hiddensingle 3 3 1 p1 nofast primary 1"
    "hiddensingle 4 3 1 p1 nofast asymmetric 1"
    "pointingpairs 3 3 1 p1 nofast primary 1"
    "pointingpairs 4 3 1 p1 nofast asymmetric 1"
    "boxlinereduction 3 3 1 p1 nofast primary 1"
    "boxlinereduction 4 3 1 p1 nofast asymmetric 1"

    "nakedpair 3 3 2 p2 nofast primary 2"
    "nakedpair 4 3 2 p2 nofast asymmetric 2"
    "hiddenpair 3 3 2 p2 nofast primary 2"
    "hiddenpair 4 3 2 p2 nofast asymmetric 2"
    "nakedtriple 3 3 2 p2 nofast primary 2"
    "nakedtriple 4 3 2 p2 nofast asymmetric 2"
    "hiddentriple 3 3 2 p2 nofast primary 2"
    "hiddentriple 4 3 2 p2 nofast asymmetric 2"

    "nakedquad 3 3 3 p3 nofast primary 3"
    "nakedquad 4 3 3 p3 nofast asymmetric 3"
    "hiddenquad 3 3 3 p3 nofast primary 3"
    "hiddenquad 4 3 3 p3 nofast asymmetric 3"
    "xwing 3 3 3 p3 nofast primary 3"
    "xwing 4 3 3 p3 nofast asymmetric 3"
    "ywing 3 3 3 p3 nofast primary 3"
    "ywing 4 3 3 p3 nofast asymmetric 3"
    "skyscraper 3 3 3 p3 nofast primary 3"
    "skyscraper 4 3 3 p3 nofast asymmetric 3"
    "twostringkite 3 3 3 p3 nofast primary 3"
    "twostringkite 4 3 3 p3 nofast asymmetric 3"
    "emptyrectangle 3 3 3 p3 nofast primary 3"
    "emptyrectangle 4 3 3 p3 nofast asymmetric 3"
    "remotepairs 3 3 3 p3 nofast primary 3"
    "remotepairs 4 3 3 p3 nofast asymmetric 3"

    "swordfish 3 3 4 p4 nofast primary 4"
    "swordfish 4 3 4 p4 nofast asymmetric 4"
    "xyzwing 3 3 4 p4 nofast primary 4"
    "xyzwing 4 3 4 p4 nofast asymmetric 4"
    "finnedxwingsashimi 3 3 4 p4 nofast primary 4"
    "finnedxwingsashimi 4 3 4 p4 nofast asymmetric 4"
    "uniquerectangle 3 3 4 p4 nofast primary 4"
    "uniquerectangle 4 3 4 p4 nofast asymmetric 4"
    "bugplusone 3 3 4 p4 nofast primary 4"
    "bugplusone 4 3 4 p4 nofast asymmetric 4"
    "wwing 3 3 4 p4 nofast primary 4"
    "wwing 4 3 4 p4 nofast asymmetric 4"
    "simplecoloring 3 3 4 p4 nofast primary 4"
    "simplecoloring 4 3 4 p4 nofast asymmetric 4"

    "jellyfish 3 3 5 p5 nofast primary 5"
    "jellyfish 4 3 5 p5 nofast asymmetric 5"
    "wxyzwing 3 3 5 p5 nofast primary 5"
    "wxyzwing 4 3 5 p5 nofast asymmetric 5"
    "finnedswordfishjellyfish 3 3 5 p5 nofast primary 5"
    "finnedswordfishjellyfish 4 3 5 p5 nofast asymmetric 5"
    "xchain 3 3 5 p5 nofast primary 5"
    "xchain 4 3 5 p5 nofast asymmetric 5"
    "xychain 3 3 5 p5 nofast primary 5"
    "xychain 4 3 5 p5 nofast asymmetric 5"
    "alsxz 3 3 5 p5 nofast primary 5"
    "alsxz 4 3 5 p5 nofast asymmetric 5"
    "uniqueloop 3 3 5 p5 nofast primary 5"
    "uniqueloop 4 3 5 p5 nofast asymmetric 5"
    "avoidablerectangle 3 3 5 p5 nofast primary 5"
    "avoidablerectangle 4 3 5 p5 nofast asymmetric 5"
    "bivalueoddagon 3 3 5 p5 nofast primary 5"
    "bivalueoddagon 4 3 5 p5 nofast asymmetric 5"
    "uniquerectangleextended 3 3 5 p5 nofast primary 5"
    "uniquerectangleextended 4 3 5 p5 nofast asymmetric 5"
    "hiddenuniquerectangle 3 3 5 p5 nofast primary 5"
    "hiddenuniquerectangle 4 3 5 p5 nofast asymmetric 5"
    "bugtype2 3 3 5 p5 nofast primary 5"
    "bugtype2 4 3 5 p5 nofast asymmetric 5"
    "bugtype3 3 3 5 p5 nofast primary 5"
    "bugtype3 4 3 5 p5 nofast asymmetric 5"
    "bugtype4 3 3 5 p5 nofast primary 5"
    "bugtype4 4 3 5 p5 nofast asymmetric 5"
    "borescoperqiudeadlypattern 3 3 5 p5 nofast primary 5"
    "borescoperqiudeadlypattern 4 3 5 p5 nofast asymmetric 5"

    "medusa3d 3 3 6 p6 nofast primary 6"
    "medusa3d 4 3 6 p6 nofast asymmetric 6"
    "aic 3 3 6 p6 nofast primary 6"
    "aic 4 3 6 p6 nofast asymmetric 6"
    "groupedaic 3 3 6 p6 nofast primary 6"
    "groupedaic 4 3 6 p6 nofast asymmetric 6"
    "groupedxcycle 3 3 6 p6 nofast primary 6"
    "groupedxcycle 4 3 6 p6 nofast asymmetric 6"
    "continuousniceloop 3 3 6 p6 nofast primary 6"
    "continuousniceloop 4 3 6 p6 nofast asymmetric 6"
    "alsxywing 3 3 6 p6 nofast primary 6"
    "alsxywing 4 3 6 p6 nofast asymmetric 6"
    "alschain 3 3 6 p6 nofast primary 6"
    "alschain 4 3 6 p6 nofast asymmetric 6"
    "alignedpairexclusion 3 3 6 p6 nofast primary 6"
    "alignedpairexclusion 4 3 6 p6 nofast asymmetric 6"
    "alignedtripleexclusion 3 3 6 p6 nofast primary 6"
    "alignedtripleexclusion 4 3 6 p6 nofast asymmetric 6"
    "alsaic 3 3 6 p6 nofast primary 6"
    "alsaic 4 3 6 p6 nofast asymmetric 6"
    "suedecoq 3 3 6 p6 nofast primary 6"
    "suedecoq 4 3 6 p6 nofast asymmetric 6"
    "deathblossom 3 3 6 p6 nofast primary 6"
    "deathblossom 4 3 6 p6 nofast asymmetric 6"
    "frankenfish 4 4 6 p6 nofast primary 6"
    "frankenfish 4 3 6 p6 nofast asymmetric 6"
    "mutantfish 4 4 6 p6 nofast primary 6"
    "mutantfish 4 3 6 p6 nofast asymmetric 6"
    "krakenfish 4 4 6 p6 nofast primary 6"
    "krakenfish 4 3 6 p6 nofast asymmetric 6"
    "squirmbag 4 4 6 p6 nofast primary 6"
    "squirmbag 4 3 6 p6 nofast asymmetric 6"

    "msls 4 4 7 p7 nofast primary 7"
    "msls 4 3 7 p7 nofast asymmetric 7"
    "exocet 4 4 7 p7 nofast primary 7"
    "exocet 4 3 7 p7 nofast asymmetric 7"
    "seniorexocet 4 4 7 p7 nofast primary 7"
    "seniorexocet 4 3 7 p7 nofast asymmetric 7"
    "skloop 4 4 7 p7 nofast primary 7"
    "skloop 4 3 7 p7 nofast asymmetric 7"
    "patternoverlaymethod 4 4 7 p7 nofast primary 7"
    "patternoverlaymethod 4 3 7 p7 nofast asymmetric 7"
    "forcingchains 3 3 7 p7 nofast primary 7"
    "forcingchains 4 3 7 p7 nofast asymmetric 7"
    "dynamicforcingchains 3 3 7 p7 nofast primary 7"
    "dynamicforcingchains 4 3 7 p7 nofast asymmetric 7"

    "backtracking 3 3 8 p8 nofast primary 8"
    "backtracking 4 3 8 p8 nofast asymmetric 8"
) do (
    call :run_strategy %%~L
    if errorlevel 1 exit /b 1
)
exit /b 0

:cleanup_fail
echo [INFO] Resetting AppVerifier for sudoku_gen_test.exe
echo [INFO] Resetting AppVerifier for sudoku_gen_test.exe>> "%SESSION_LOG%"
(
    echo ===== APPVERIF RESET (FAIL) =====
    appverif.exe /n "%EXE%"
    echo.
) >> "%APPVERIF_LOG%" 2>&1
appverif.exe /n "%EXE%" >nul 2>&1
call :write_summary
exit /b 1

:cleanup_ok
echo [INFO] Resetting AppVerifier for sudoku_gen_test.exe
echo [INFO] Resetting AppVerifier for sudoku_gen_test.exe>> "%SESSION_LOG%"
(
    echo ===== APPVERIF RESET (OK) =====
    appverif.exe /n "%EXE%"
    echo.
) >> "%APPVERIF_LOG%" 2>&1
appverif.exe /n "%EXE%" >nul 2>&1
call :write_summary

echo [DONE] Smoke sequence finished.
echo [DONE] Smoke sequence finished.>> "%SESSION_LOG%"
exit /b 0

:run_strategy
set "STRATEGY=%~1"
set "BOX_ROWS=%~2"
set "BOX_COLS=%~3"
set "DIFFICULTY=%~4"
set "MCTS_PROFILE=%~5"
set "FAST_MODE=%~6"
set "VARIANT=%~7"
set "LEVEL=%~8"
set "OUTDIR=Debugs\smoke_%STRATEGY%_%VARIANT%"
set "RUNLOG=%REPORT_DIR%\%STRATEGY%_%VARIANT%_raw.txt"
set "MAX_TOTAL_TIME=%MAX_TOTAL_TIME_DEFAULT%"
if %LEVEL% LEQ 4 set "MAX_TOTAL_TIME=%MAX_TOTAL_TIME_EASY%"

echo.
echo ============================================================================
echo [SMOKE] %STRATEGY% [%VARIANT%] geom=%BOX_ROWS%x%BOX_COLS% difficulty=%DIFFICULTY% level=%LEVEL% timeout=%MAX_TOTAL_TIME%s
echo ============================================================================
>> "%SESSION_LOG%" echo.
>> "%SESSION_LOG%" echo ============================================================================
>> "%SESSION_LOG%" echo [SMOKE] %STRATEGY% [%VARIANT%] geom=%BOX_ROWS%x%BOX_COLS% difficulty=%DIFFICULTY% level=%LEVEL% timeout=%MAX_TOTAL_TIME%s
>> "%SESSION_LOG%" echo ============================================================================

if /I "%FAST_MODE%"=="nofast" (
    "%EXE%" --cli --box-rows %BOX_ROWS% --box-cols %BOX_COLS% --difficulty %DIFFICULTY% --required-strategy %STRATEGY% --seed 0 --target 10 --threads %THREADS% --max-total-time-s %MAX_TOTAL_TIME% --max-attempts %MAX_ATTEMPTS% --mcts-profile %MCTS_PROFILE% --output-folder "%OUTDIR%" --output-file generated_sudoku.txt --single-file-only --benchmark-mode --pattern-forcing --strict-canonical-strategies --no-proxy-advanced --no-fast-test > "%RUNLOG%" 2>&1
) else (
    "%EXE%" --cli --box-rows %BOX_ROWS% --box-cols %BOX_COLS% --difficulty %DIFFICULTY% --required-strategy %STRATEGY% --seed 0 --target 10 --threads %THREADS% --max-total-time-s %MAX_TOTAL_TIME% --max-attempts %MAX_ATTEMPTS% --mcts-profile %MCTS_PROFILE% --output-folder "%OUTDIR%" --output-file generated_sudoku.txt --single-file-only --benchmark-mode --pattern-forcing --strict-canonical-strategies --no-proxy-advanced --fast-test > "%RUNLOG%" 2>&1
)
set "RUN_EXIT=%ERRORLEVEL%"
type "%RUNLOG%"
type "%RUNLOG%" >> "%SESSION_LOG%"
if not "%RUN_EXIT%"=="0" (
    echo [ERROR] Strategy failed: %STRATEGY% [%VARIANT%]
    >> "%SESSION_LOG%" echo [ERROR] Strategy failed: %STRATEGY% [%VARIANT%]
    exit /b 1
)
exit /b 0

:write_summary
(
    echo ============================================================================
    echo ZBIORCZY RAPORT TESTU WSZYSTKICH STRATEGII
    echo ============================================================================
    echo Repo: %REPO%
    echo Binary: %EXE%
    echo Session log: %SESSION_LOG%
    echo AppVerifier log: %APPVERIF_LOG%
    echo.
    echo --- TEST LOGI / PODSUMOWANIE ---
    findstr /R /C:"^\[SMOKE\]" /C:"^Accepted:" /C:"^Written:" /C:"^Attempts:" /C:"^Required strategy:" /C:"^Required strategy certified exact:" /C:"^Required exact contract met:" /C:"^VIP score:" /C:"^VIP grade:" /C:"^VIP contract:" /C:"^Time:" "%SESSION_LOG%"
    echo.
    echo --- TEST PAMIECI / HEURYSTYCZNY SCAN LOGOW ---
    findstr /I /C:"heap" /C:"memory" /C:"invalid" /C:"corrupt" /C:"verifier stop" /C:"access violation" /C:"leak" /C:"out of bounds" /C:"double free" /C:"use after free" "%SESSION_LOG%" "%APPVERIF_LOG%"
    if errorlevel 1 echo [OK] Nie znaleziono oczywistych wpisow o bledach pamieci w SESSION_LOG ani APPVERIF_LOG.
    echo.
    echo --- SUROWY APPVERIF.EXE LOG ---
    type "%APPVERIF_LOG%"
) > "%SUMMARY_REPORT%"
exit /b 0
