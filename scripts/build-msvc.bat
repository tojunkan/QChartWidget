@echo off
setlocal
REM ============================================================================
REM  QChartWidget —— MSVC 构建脚本（在 Windows 上运行）
REM  依赖：Visual Studio 2026 (v145 工具集) + Qt 6.11.1 msvc2022_64 + Qt 自带 Ninja
REM  路径可用环境变量覆盖：VCVARS / QT_DIR / NINJA / BUILD_DIR
REM ============================================================================

if not defined VCVARS    set "VCVARS=E:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if not defined QT_DIR    set "QT_DIR=E:\Qt\6.11.1\msvc2022_64"
if not defined NINJA     set "NINJA=E:\Qt\Tools\Ninja\ninja.exe"
if not defined BUILD_DIR set "BUILD_DIR=build-msvc"

call "%VCVARS%" >nul 2>&1
if errorlevel 1 (
    echo [ERROR] vcvars64.bat 执行失败: "%VCVARS%"
    exit /b 1
)

cmake -G Ninja -S . -B "%BUILD_DIR%" ^
  -DCMAKE_BUILD_TYPE=Debug ^
  -DCMAKE_C_COMPILER=cl ^
  -DCMAKE_CXX_COMPILER=cl ^
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

echo.
echo 构建完成: %BUILD_DIR%\QChartDemo.exe   %BUILD_DIR%\QChartTests.exe
echo 运行单元测试: ctest --test-dir %BUILD_DIR% --output-on-failure
endlocal
