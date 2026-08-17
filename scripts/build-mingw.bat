@echo off
setlocal
REM ============================================================================
REM 加载本地配置文件（用户自己创建，已加入 .gitignore）
REM ============================================================================
set "SCRIPT_DIR=%~dp0"
if exist "%SCRIPT_DIR%config.local.bat" (
    echo [INFO] 加载本地配置: %SCRIPT_DIR%config.local.bat
    call "%SCRIPT_DIR%config.local.bat"
)

REM ============================================================================
REM  QChartWidget —— MinGW 构建脚本（在 Windows 上运行）
REM  依赖：Qt 6.11.0 mingw_64 + Qt 自带 mingw1310_64 工具链 + Ninja
REM  路径可用环境变量覆盖：MINGW_BIN / QT_DIR / NINJA / BUILD_DIR
REM ============================================================================

if not defined MINGW_BIN set "MINGW_BIN=E:\Qt\Tools\mingw1310_64\bin"
if not defined QT_DIR    set "QT_DIR=E:\Qt\6.11.0\mingw_64"
if not defined NINJA     set "NINJA=E:\Qt\Tools\Ninja\ninja.exe"
if not defined SCRIPT_DIR set "SCRIPT_DIR=%~dp0"
if not defined ROOT_DIR set "ROOT_DIR=%SCRIPT_DIR%.."
if not defined BUILD_DIR set "BUILD_DIR=%ROOT_DIR%\build-mingw"
set "PATH=%MINGW_BIN%;%PATH%"

cmake -G Ninja -S ../ -B "%BUILD_DIR%" ^
  -DCMAKE_BUILD_TYPE=Debug ^
  -DCMAKE_C_COMPILER="%MINGW_BIN%\gcc.exe" ^
  -DCMAKE_CXX_COMPILER="%MINGW_BIN%\g++.exe" ^
  -DCMAKE_MAKE_PROGRAM="%NINJA%" ^
  -DCMAKE_PREFIX_PATH="%QT_DIR%"
if errorlevel 1 (
    echo [ERROR] CMake 配置失败
    exit /b 1
)

cmake --build "%BUILD_DIR%"
if errorlevel 1 (
    echo [ERROR] 编译失败
    exit /b 1
)

REM 构建完成后，执行 windeployqt 部署依赖
"%QT_DIR%\bin\windeployqt.exe" "%BUILD_DIR%\QChartDemo.exe"
"%QT_DIR%\bin\windeployqt.exe" "%BUILD_DIR%\QChartTests.exe"  REM 测试也需要，但一般测试在CI里跑，可以省略

echo.
echo 构建完成: %BUILD_DIR%\QChartDemo.exe   %BUILD_DIR%\QChartTests.exe
echo 运行单元测试: ctest --test-dir %BUILD_DIR% --output-on-failure
endlocal
