# QChartWidget

基于 **QPainter** 的轻量图表库。架构采用「五空间模型」（Data → Numeric → View Cartesian → ViewNorm → Pixel），
由 `QChartWidget`（容器）+ `QChartAxis`（轴）+ `QChartLayer`（图层）+ `QChartSeries`（系列）+ `QChartProjection`（投影）组成。

> 架构与设计细节见 [`design_notes.md`](docs/design/design_notes.md)。

## 目录结构

| 路径 | 职责 |
|---|---|
| 根目录 | 库源码（`QChartWidget` / `QChartAxis` / `QChartLayer` / `QChartSeries` / `QChartProjection` 及子类） |
| `Test/` | 演示程序。`Test/test.cpp` 统一入口，`Test/demos/` 下每个 `buildDemoXxx()` 一个独立演示 |
| `TestUnit/` | 单元测试（QtTest，`tests/` 下每类轴一个测试） |
| `scripts/` | Windows 一键构建脚本（MSVC / MinGW） |

## 依赖

- Qt 6（>= 6.2，`Core` / `Gui` / `Widgets`；测试额外需要 `Test`）
- CMake >= 3.21 + Ninja（或其它 CMake 生成器）

## 构建（Windows）

### 方式一：MSVC（Visual Studio 2026 + Qt 6.11.1 msvc2022_64）

```bat
scripts\build-msvc.bat
```

等价的手动命令：

```bat
call "E:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -G Ninja -S . -B build-msvc -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=E:\Qt\6.11.1\msvc2022_64
cmake --build build-msvc
```

### 方式二：MinGW（Qt 6.11.0 mingw_64 + mingw1310_64）

```bat
scripts\build-mingw.bat
```

等价的手动命令（先把 mingw 加入 PATH）：

```bat
set PATH=E:\Qt\Tools\mingw1310_64\bin;%PATH%
cmake -G Ninja -S . -B build-mingw -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=E:\Qt\6.11.0\mingw_64
cmake --build build-mingw
```

> 两个脚本里的工具链 / Qt 路径都可用同名环境变量覆盖，例如
> `set QT_DIR=D:\Qt\6.11.1\msvc2022_64 && scripts\build-msvc.bat`。

## 运行

- 演示：运行 `build-msvc\QChartDemo.exe`（或 `build-mingw\QChartDemo.exe`）。
  想切换演示，编辑 `Test/test.cpp` 里 `main()` 末尾的 `buildDemoXxx()` 调用（当前默认 `buildDemoSwirl`）。
- 单元测试：

```bat
ctest --test-dir build-msvc --output-on-failure
```

## 跨编译器 / 跨平台说明

- 源码为纯 Qt 编写，不含 MSVC / Win32 特有代码，**源码级可移植**：同一份 `CMakeLists.txt`
  可在 Windows（MSVC / MinGW）、Linux、macOS 上编译（后两者只需安装任意 Qt6 + CMake）。
- 注意**二进制 ABI 不跨编译器**：MinGW 编译出的 C++ 库不能直接链接进 MSVC 程序（反之亦然）。
  若日后要把本库作为 `.lib`/`.dll` 喂给某个 MSVC 应用，则本库也需用 MSVC 编译。
- 当前库目标为**静态库**（`QChartWidget.lib` / `libQChartWidget.a`）。公共类尚未加导出宏，
  如需共享库（DLL），需先给公共类加 `QCHARTWIDGET_EXPORT` 宏。
