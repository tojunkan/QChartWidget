@echo off
call "E:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1
echo Building QChartWidget (Debug/x64)...
"E:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" "E:\Dujia\DuRunHan\Programs\cplusplus\GoodsSystem\GoodsSystem.sln" /t:QChartWidget /p:Configuration=Debug /p:Platform=x64 /p:QtMsBuild="C:\Users\UniDu\AppData\Local\QtMsBuild" /m
echo Exit code: %ERRORLEVEL%
