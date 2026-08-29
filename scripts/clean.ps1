# clean.ps1 —— 彻底清理 QChartWidget 构建产物（含 VSCode 调试锁释放）
Write-Host "🧹 QChartWidget 构建清理工具 (含锁释放)" -ForegroundColor Cyan

$root = Split-Path -Parent $PSScriptRoot

# 1. 终止所有可能由 VSCode 启动的调试/运行进程
$processes = @(
    "QChartDemo",
    "QChartUnitTests", 
    "QChartIntegrationTests",
    "QChartBench",
    "windeployqt",
    "cppvsdbg",          # MSVC 调试器
    "vsdbg",             # 通用调试器
    "devenv"             # Visual Studio 宿主（可能）
)

foreach ($proc in $processes) {
    Get-Process -Name $proc -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
}

# 2. 稍等文件锁释放
Start-Sleep -Milliseconds 800

# 3. 强制删除所有 build-* 目录（先尝试普通删除，失败则用 cmd 的 rd /s /q 重试）
$deleted = 0
Get-ChildItem -Path $root -Directory -Filter "build-*" | ForEach-Object {
    $dir = $_.FullName
    Write-Host "  正在删除目录: $($_.Name)" -ForegroundColor Yellow
    try {
        Remove-Item -Recurse -Force $dir -ErrorAction Stop
        $deleted++
    } catch {
        Write-Warning "普通删除失败，尝试使用 cmd /c rd /s /q ..."
        # 使用 cmd 的 rd 命令，它有时能绕过某些进程占用
        cmd /c "rd /s /q `"$dir`"" 2>$null
        if (Test-Path $dir) {
            Write-Error "仍然无法删除 $($_.Name)，可能被 VSCode 或其他进程占用。"
            Write-Host "请尝试：1) 点击 VSCode 左下角的运行目标，切换到别的目标（如 '当前文件'）" -ForegroundColor Yellow
            Write-Host "       2) 按 Ctrl+Shift+P -> 'Debug: Stop' 停止所有调试" -ForegroundColor Yellow
            Write-Host "       3) 关闭 VSCode 后再运行此脚本" -ForegroundColor Yellow
        } else {
            $deleted++
        }
    }
}

# 4. 删除根目录的 compile_commands.json
$cc = Join-Path $root "compile_commands.json"
if (Test-Path $cc) {
    Remove-Item $cc -Force -ErrorAction SilentlyContinue
    $deleted++
}

if ($deleted -gt 0) {
    Write-Host "✅ 清理完成，已删除 $deleted 个构建目录/文件。" -ForegroundColor Green
    Write-Host "💡 重新配置：在 VSCode 中按 F1 -> CMake: Configure 即可重新生成。" -ForegroundColor Gray
} else {
    Write-Host "⚠️ 没有找到任何 build-* 目录，无需清理。" -ForegroundColor Yellow
}