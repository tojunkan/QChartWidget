# Windows resize 崩溃复盘：证据链、归因边界与 UST 页堆备用配方

> **文档性质**：复盘记录（audit / replay）。记录 2026-09-13 用户 Windows 侧「GL 后端拖动窗口边缘连续缩放时概率性崩溃」的调查过程与证据。
> **不在本文提出修复动作**（修复由 t82 承接）；本文只做证据归档、归因边界说明与**后续取证配方**。
> **状态**：根因**未完全证实** —— 受害块的属主与释放者已明确（Intel GL 驱动所有，在 `layoutGlHost` 触发的释放路径上被发现损坏），
> 但**写者（执行越界写的那条指令）尚未定位**。凡未证实处均已标注「未定论」。

---

## 1. 症状与复现方式

### 1.1 两种运行模式下的表现

| 模式 | 表现 | 记录来源 |
|---|---|---|
| **普通运行**（无调试器） | resize 时**概率性**卡住数秒后**静默退出**；WER 记为 AppCrash（共 80 条相关事件，异常码 `c0000005` / `c000041d` 交替，故障模块 `Qt6Guid.dll`、偏移恒定） | 用户侧 WER / 任务书输入 |
| **调试堆运行**（cdb + `gflags` 调试堆） | 调试堆在校验块尾时**当场 `int 3`**：`Break instruction exception - code 80000003`，命中 `ntdll!RtlpBreakPointHeap+0x16` | `build-linux/win_evidence/resize_dbg.log:279-281`、`resize_dbg4.log:173,185-186` |
| **页堆运行**（Page Heap） | **必崩**：`Access violation - code c0000005 (first chance)`，崩溃指令 `cmp qword ptr [rax+8],0`，被读地址落在 `Usage: PageHeap / State: MEM_RESERVE / Allocation Protect: PAGE_NOACCESS` 区间 | `resize_pageheap.log:171,488-489`、`dump_analysis.log:57-58,254,347-358` |
| **MinGW 构建**（Qt 6.11.0 **Release**） | **页堆下完全干净**（不崩） | 用户侧实验（Windows，未入库） |
| **MSVC + ASan** | **不崩**（布局/时序改变 ⇒ 触发窗口未撞上） | 用户侧实验（Windows，未入库） |
| **`QT_OPENGL=software`** | GL 内容不渲染（`QOpenGLFunctions_3_3_Core` 不可用）且 **resize 会卡死**（另一独立问题） | 用户侧实验（Windows，未入库） |

### 1.2 最小复现操作

1. 以 **MSVC Debug（Qt 6.11.1 Debug 套件）** 构建 `QChartDemo`，运行 `QChartDemo.exe axis gl`；
2. **用鼠标拖动窗口边缘连续缩放**（放大/缩小往复、拖到极扁/极窄再拉回），并在期间拖动绘图区视图；
3. 普通运行：约数十次缩放内出现「卡住数秒 → 静默退出」（概率性）；
   调试堆运行：出现 HEAP 校验中断（块尾被写坏）；
   页堆运行：必崩（读已释放/保留页）。

### 1.3 环境

- 平台：Windows x64；GPU：**Intel UHD 620**（驱动模块 `ig9icd64.dll`，见 `resize_dbg.log:166` 的 ModLoad 行）；
- 缩放：**DPR 1.5**；构建：**MSVC 2026 + Qt 6.11.1 Debug 套件**（对应 `Qt6Cored.dll`/`Qt6Widgetsd.dll`/`Qt6Guid.dll`）；
- 对照构建：MinGW + Qt 6.11.0 Release；MSVC + ASan（用户侧）。

---

## 2. 关键证据（原文摘录 + 出处）

### 2.1 调试堆：240 字节块被写超 24 字节（**两次运行签名一致**）

```
resize_dbg.log:279    HEAP[QChartDemo.exe]: Heap block at 00000217CA33E760 modified at 00000217CA33E868 past requested size of f0
resize_dbg.log:280    (17e0.1c74): Break instruction exception - code 80000003 (first chance)
resize_dbg.log:281    ntdll!RtlpBreakPointHeap+0x16:  00007fff`b797e39a cc  int 3
--- 第二次运行（同签名） ---
resize_dbg4.log:173   HEAP[QChartDemo.exe]: Heap block at 000001EAB774F640 modified at 000001EAB774F748 past requested size of f0
```
**偏移自校验**：`0x…E868 − 0x…E760 = 0x108`、`0x…F748 − 0x…F640 = 0x108`；请求大小 `0xf0`(240 B)
⇒ **两次都在「块起始 +0x108」处被写，即超出请求大小 24 字节**（`0x108 − 0xf0 = 0x18 = 24`）——签名可复现。

### 2.2 调试堆校验栈：损坏是**在释放时**被发现的，且释放路径来自 Intel GL 驱动

```
resize_dbg4.log:186   ntdll!RtlpBreakPointHeap+0x16            (参数含 00000000`000000f0 = 240 字节请求)
resize_dbg4.log:189   ntdll!RtlpCheckBusyBlockTail+0x232
resize_dbg4.log:190   ntdll!RtlpValidateHeapEntry+0x177
resize_dbg4.log:191   ntdll!RtlDebugFreeHeap+0xb4
resize_dbg4.log:192   ntdll!RtlpFreeHeap+0x9f7
resize_dbg4.log:193   ntdll!RtlFreeHeap+0x620
resize_dbg4.log:194   ig9icd64+0x560ee4          ← Intel GL 驱动发起释放
resize_dbg4.log:195   ig9icd64+0x8014f
…（驱动内部若干帧）…
```
调用方继续向上（同一栈的“崩溃线程栈”视图）：
```
resize_dbg4.log:225   QChartDemo!QChartAbstractWidget::layoutGlHost+0x92
resize_dbg4.log:226   QChartDemo!QChartAbstractWidget::relayout+0xef
resize_dbg4.log:227   QChartDemo!QChartAbstractWidget::paintEvent+0x5a
（再向上为 Qt6Widgetsd/Qt6Cored 帧与消息循环）
```
**读法**：调试堆的**块尾校验**只在 `RtlFreeHeap` 时执行 ⇒ 本栈给出的是「**谁在释放时发现了块尾被写坏**」，
**不是写者**（写者可能发生在更早的任意时刻）。但这条栈确定了三件关键事实：
1. 被写坏的块由**Intel GL 驱动**在释放（`ig9icd64`），即该 240 B 块**属驱动所有**；
2. 释放发生在 **`QChartAbstractWidget::layoutGlHost`**（经 `relayout` ← **`paintEvent`**）触发的调用链内；
3. 触发链经过「**在绘制回调里改 GL 子控件几何**」这条路径 —— 这是本次调查最重要的触发线索。

### 2.3 页堆：读已释放/保留页（use-after-free 型崩溃）

```
resize_pageheap.log:171   (31cc.1174): Access violation - code c0000005 (first chance)
resize_pageheap.log:444     Value: INVALID_POINTER_READ_AVRF_c0000005_Qt6Guid.dll!Unknown
resize_pageheap.log:447     Value: 0xc0000005
resize_pageheap.log:488   ExceptionAddress: 00007fff0cf2f0a5 (Qt6Guid!…+0x000000000062c21f)
resize_pageheap.log:489      ExceptionCode: c0000005 (Access violation)
resize_pageheap.log:572   FAILURE_BUCKET_ID:  INVALID_POINTER_READ_AVRF_c0000005_Qt6Guid.dll!Unknown
--- 同一 dump 的反汇编与地址属性 ---
dump_analysis.log:57      Qt6Guid!QTextCharFormat::setFontLetterSpacingType+0x62c21f:
dump_analysis.log:58      00007fff`0cf2f0a5 4883780800  cmp qword ptr [rax+8],0 ds:0000023d`31c46f48=????????????????
dump_analysis.log:119      Value: INVALID_POINTER_READ_AVRF_c0000005_Qt6Guid.dll!Unknown
dump_analysis.log:128      Value: Qt6Guid
dump_analysis.log:347   Usage:                  PageHeap
dump_analysis.log:351   State:                  00002000          MEM_RESERVE
dump_analysis.log:352   Protect:                <info not present at the target>
dump_analysis.log:358   Allocation Protect:     00000001          PAGE_NOACCESS
dump_analysis.log:359   More info:              !heap -p 0x23d152f1000
dump_analysis.log:360   More info:              !heap -p -a 0x23d31c46f48
```
**读法**：崩溃指令是 `cmp qword ptr [rax+8],0` —— 两级间接读取的**比较**指令（`[rax+8]`），被读地址
`0x23d31c46f48` 落在 **PageHeap 保留、`PAGE_NOACCESS`** 的区域 ⇒ **读的是已释放（或被页堆保护）的内存**，
即 **use-after-free / use-after-reserve** 型访问违例；崩溃点位于 `Qt6Guid.dll` 内部（偏移见下条「矛盾」）。
栈的外层仍是 GUI 线程消息循环：`dump_analysis.log:342-345` 可见 `Qt6Widgetsd!…` ← `QChartDemo!main+0x620` ←
`invoke_main` ⇒ 崩溃发生在**主线程事件循环内的绘制/布局路径**。

> **⚠️ 证据矛盾（如实列出，列为未定论）**：
> ① 任务书与 WER 记录给出的故障偏移为 **`Qt6Guid.dll + 0x68f0cb`**，而本目录 cdb dump 解析出的异常地址偏移为
> **`+0x62c21f`**（`resize_pageheap.log:488`、`dump_analysis.log:57`）。两者**不一致**，可能是不同运行/不同 Qt 构建
> （WER 事件集合含较早的运行）或 WER 与 cdb 记法差异所致，**未定论**；后续以「同一 dump 内」的偏移为准。
> ② dump 中所有 Qt6Guid 帧被解析为 `QTextCharFormat::setFontLetterSpacingType+大偏移`，这是**无 PDB 时的最近导出符号假象**，
> 只能读**模块 + 偏移**，不可据此认为与字体/排版有关。

### 2.4 交叉实验（用户侧，Windows E 盘，原始日志未入库）

| 实验 | 结果 | 含义（调查推断） |
|---|---|---|
| MinGW + Qt 6.11.0 **Release**，页堆下 resize | **完全干净** | 触发与「Debug 运行时/调试堆逐次校验」或构建差异强相关；Release 下即便写坏也不一定被发现/致崩 |
| MSVC + **ASan** resize | **不崩** | ASan 改变分配布局与时序 ⇒ 触发窗口未撞上；**不可据此判定"无缺陷"** |
| `QT_OPENGL=software` | GL 内容不渲染（`QOpenGLFunctions_3_3_Core` 不可用）+ resize 卡死 | 与本次崩溃**不同**的独立问题（软件 GL 后端能力缺失） |

> **工具性插曲（如实记录）**：`chan_test.log` 显示一次 cdb「通道自检」未能执行（
> `chan_test.log` 末行：`Cannot execute 'CHANNEL_OK; q QChartDemo.exe axis gl '`，Win32 error 0n2）——
> 该次尝试无效，后续取证请以 `run_dbg.bat` / `run_pageheap.bat` 的实际输出为准。

---

## 3. 归因与未定论项

### 3.1 已确认（有原始证据）

| 事实 | 证据 |
|---|---|
| 存在一个 **240 B（0xf0）** 的堆块被写到 **+0x108（超出 24 B）**，且**两次运行签名一致** | §2.1 |
| 该块**属 Intel GL 驱动所有**，并在 `RtlFreeHeap` 校验块尾时被发现损坏 | §2.2（`RtlDebugFreeHeap ← RtlFreeHeap ← ig9icd64`） |
| 触发链经过 **`QChartAbstractWidget::paintEvent → relayout → layoutGlHost`**（**在绘制回调内改 GL 子控件几何**） | §2.2（`resize_dbg4.log:225-227,313-315`） |
| 页堆下崩溃是**读取已释放/保护页**（`PAGE_NOACCESS`/`MEM_RESERVE`），指令为 `cmp qword ptr [rax+8],0`，位于 **`Qt6Guid.dll` 内部** | §2.3 |
| 崩溃发生在 GUI 主线程事件循环内（外层帧为 Qt6Widgetsd ← `QChartDemo!main`） | §2.3 |

### 3.2 未定论（不得写成已证实）

1. **写者未定位**：没有任何证据指出"执行越界写的那条指令"。（调试堆只报"块尾被写坏"，不含写者栈；页堆本次抓到的是**读**违例。）
2. **"我们调用方式触发 Qt/驱动生命周期错配"目前只是假设**：证据支持"受害块属驱动 + 触发链经过本库布局回调 + 页堆下呈现 use-after-free"，
   但**本库没有越界写的实证**（见 §3.3）。
3. **`0xf0` 块的语义未确定**：240 B = 15×16 B（`GLVertex` 恰 16 B，`static_assert` 已钉；或 15 个 `QPointF`；或 60 float），
   但没有任何直接证据表明是本库的哪个容器；属主已是驱动，故更可能是驱动内部的顶点/状态块。
4. **两处故障偏移不一致**（WER `+0x68f0cb` vs cdb dump `+0x62c21f`，见 §2.3 注）。
5. **Release/MinGW 干净的原因未证实**（调试堆校验缺失？时序不同？编译差异？）——只能说"未复现"，不能说明"无缺陷"。

### 3.3 本库侧的排除性证据（来自本轮 Linux 诊断，t80/t81）

| 证据 | 出处 | 说明 |
|---|---|---|
| **Linux 7 模式 × 200 轮** resize 压力（含极窄/极扁/往复/最小化/显示隐藏/CPU↔GL 后端反复重建/DPR1.5）**无运行期崩溃** | `build-linux/t80_probe/t80_diagnosis.md` §1 | 说明 resize 路径在 llvmpipe 下稳定；不等价于 Windows 驱动路径稳定 |
| **ASan（本库 + 真实 demo 源码全插桩）1200+ 轮 resize 洁净**（GL rapid/extreme/oscillate/dragresize + DPR1.5 + CPU 档，`ERROR: AddressSanitizer` 0 条） | `build-linux/t81_probe/t81_diagnosis.md` §2 | 本库被插桩的写入路径未越界 |
| **静态筛查：本库 resize/渲染路径无裸内存写**（无 `memcpy/memset/.bits()/scanLine/std::copy`；仅有界容器写入；`GLVertex` 有 16 B 断言） | 同上 §0/§3 | 本库作为"写者"的可能性较低（但**不排除**间接类型：本库把指针/索引写坏后由 Qt/驱动使用） |
| 页堆下崩溃点为"读已释放页"、且外层栈在 Qt6Guid ⇒ 与本库无直接符号关联 | 本报告 §2.3 | 与"受害块属驱动"一致 |

**归因表述（建议口径）**：> 现有证据表明：**受害内存属 Intel GL 驱动；损坏在由 `layoutGlHost`（经 `relayout ← paintEvent`）触发的释放路径上被发现；页堆下表现为 Qt6Guid 内的 use-after-free 读崩溃。**
> 因此**假设**为"本库的调用方式（在绘制回调内变更 GL 子控件几何）触发了 Qt/驱动的生命周期错配"，
> 但**写者尚未定位，该假设未经证实**；Linux 侧的 ASan/压力结论不能替代 Windows 侧取证。

---

## 4. 与 t80 §5「上下文生命周期项」的关系（**独立缺陷，勿混为一谈**）

| 项 | 现象 | 已证程度 | 与本次 resize 崩溃的关系 |
|---|---|---|---|
| **A. 退出期 SIGSEGV**（t80 §2） | GL 控件**存活到 `QApplication` 析构**时 100% 崩（栈：`QObject::thread ← QOffscreenSurface::create ← libQt6OpenGL ← QThreadStorageData::finish ← ~QApplicationPrivate`）；退出前 `delete` 控件则干净；**裸 `QOpenGLWidget` 对照干净** | **已归因到范围**：需要"控件/上下文存活到应用析构"；**与程序缓存无关**（`makeCurrent+releasePrograms()` 仍崩） | **独立**：发生在**进程退出**，与 resize 期间的堆损坏无共同证据；本文 §2 的崩溃发生在**运行期** |
| **B. `QChartGL::program()` 上下文键缓存无失效机制**（`src/core/QChartGL.cpp:260-292`；`registerHost/unregisterHost` 全仓无人调用） | 若新上下文复用已销毁上下文的地址，`program()` 会返回属于死上下文的程序 ⇒ 崩溃/静默不出图 | **客观缺陷已确认存在**；但在 Linux 上**未触发**（t80 `multirender` 6 个连续控件 FBO 墨迹均 843、`churn/backend` 140+ 次上下文重建无崩溃） | **独立**：属"缓存生命周期"问题，**没有任何证据显示它参与了本次 resize 堆损坏**（受害块属驱动、且发现于驱动释放路径） |

> **结论**：A、B 是 t80 已定位/已记录的两个**独立**缺陷；本次 Windows resize 崩溃（本文 §2）**不应**用 A 或 B 解释。
> 三者的唯一共性是"GL 资源生命周期管理薄弱"这一**主题**，但**代码部位、触发时机、证据链各自独立**。

---

## 5. 备用配方：**页堆 + 用户栈回溯（UST）**抓「谁释放的 / 谁写的」

> 目标：把「块尾被写坏（无写者栈）」升级为「**在写者指令处当场 AV**」或「**读到 freed-by 栈**」。

### 5.1 安装与开启（二选一，推荐 A）

**方案 A：gflags（最省事，自动写注册表）**
```bat
:: 管理员 cmd。先确认 gflags 可用（Windows SDK / Debugging Tools for Windows）
gflags /p /enable QChartDemo.exe /full        :: 全页堆（每次分配独占页 + 尾部保护页）
gflags /i QChartDemo.exe +ust                 :: 记录用户栈（UST）
gflags /i QChartDemo.exe                      :: 复核：应显示 ust 与相关 heap 标志
gflags /p                                     :: 复核：应显示 QChartDemo.exe: page heap enabled with flags (full)
:: 重要：UST 数据库大小（否则栈可能不记录）
reg add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options" ^
    /v StackTraceDatabaseSizeInMB /t REG_DWORD /d 128 /f
```
**方案 B：注册表直填**（等价于 A；**数值以 `gflags /i` 的显示为准，不要盲目抄位**）
```bat
:: 期望：PageHeapFlags = "full"；GlobalFlag 含 FLG_HEAP_PAGE_ALLOCS(0x02000000) 与 UST 位
reg add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\QChartDemo.exe" ^
    /v PageHeapFlags /t REG_SZ /d "full" /f
reg add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\QChartDemo.exe" ^
    /v GlobalFlag /t REG_SZ /d 0x02101000 /f
```
**方案 C：Application Verifier（可选，与 A/B 互斥使用）**
```bat
appverif.exe /enable Heaps QChartDemo.exe
appverif.exe /enable Handles QChartDemo.exe
:: 复现后务必关闭：
appverif.exe /disable * QChartDemo.exe
```
**关闭页堆（每次取数后必须执行，否则该 exe 之后极慢）**
```bat
gflags /p /disable QChartDemo.exe
gflags /i QChartDemo.exe -ust
```

### 5.2 抓取步骤（复用既有 `run_pageheap.bat` 的模式）

```bat
:: ① 以 cdb 启动并保存完整日志与转储（沿用现有脚本结构：先写命令文件再 $$>< 执行）
cd /d E:\Dujia\DuRunHan\Programs\cplusplus\QChartWidget\build-msvc\Test\demos
cdb -o -g -G -c ".logopen resize_pageheap_ust.log; g; .ecxr; r; kb; !analyze -v; !heap -p -a <被读地址>; .dump /ma resize_crash_ust.dmp; q" QChartDemo.exe axis gl
:: ② 复现操作：拖动窗口边缘连续缩放（放大/缩小往复、极扁/极窄、与视图拖动交替）
:: ③ 结束后：gflags /p /disable QChartDemo.exe
```
要点：
- `-o`（忽略首次异常继续跑）便于观察；**若首次 AV 即写者，去掉 `-o` 直接停在写者处**（两种都跑一遍）。
- 关键命令：`!analyze -v`、`kb`、`!heap -p -a <地址>`、`.dump /ma`。

### 5.3 判读要点

| 观察 | 含义 | 下一步 |
|---|---|---|
| `!heap -p -a <地址>` 输出 **"freed by" 栈**（UST 生效时含用户帧） | 该地址**已被释放**；栈即**释放者**（若含 `ig9icd64` ⇒ 驱动释放；含 `QChartWidget!` ⇒ 本库释放） | 与 §2.2 的释放路径对照，确认"释放后仍被读/写"的时序 |
| `!heap -p -a <地址>` 输出 **"allocated by" 栈** | 该块的**分配者**（用于判定 0xf0 块的语义） | 若分配者是 `ig9icd64` ⇒ 驱动内部块（与 §2.2 一致） |
| AV 的**故障指令是写**（`mov [..],..`）且地址落在保护页 | **写者当场落网** —— 这就是本次缺失的关键证据 | 记录 `kb` 全文；写者栈落在 `QChartWidget!` ⇒ 本库越界；落在 `Qt6Guid/ig9icd64` ⇒ 非本库写者 |
| AV 的**故障指令是读**（如 `cmp qword ptr [rax+8],0`） | use-after-free 的**读者**（§2.3 即为该情形） | 结合 freed-by 栈确定"谁提前释放" |
| 页堆下**不再复现** | 触发依赖特定分配布局/时序（自我保护） | 回到调试堆 + **写监视点**（见 5.4） |

### 5.4 调试堆路径的进阶配方：给 0xf0 块的**尾部**下写监视点（可直接抓写者栈）

思路：调试堆报的是「块尾被写坏」，但块地址每次不同 ⇒ 在**分配时**捕获该块，再对**尾部**下硬件写断点：
```bat
:: 1) 在调试堆下启动（沿用 run_dbg.bat），并在分配侧下断（按 0xF0 过滤）
cdb -o -g -c "bp ntdll!RtlDebugAllocateHeap \".if (@rdx==0xf0) { .echo FOUND_0xF0_BLOCK; r; kb; .echo 记录返回的块地址 } .else { gc }\"; g" QChartDemo.exe axis gl
:: 2) 拿到块地址 base 后，对"块尾之后 24B 区间"下写断点（示例：base=0x…F640 ⇒ 尾区 0x…F730~0x…F748）
ba w 4 0x000001EAB774F740
ba w 4 0x000001EAB774F744
:: 3) g 继续 → 硬件写断点命中处即**写者栈**（这就是本报告缺失的那条栈）
```
> 注意：① 需要**块地址在当次运行内有效**（地址每次运行不同，故必须先捕获分配）；② 硬件断点数量有限（x64 通常 4 个），
> 建议只在尾部下 1–2 个；③ 若驱动使用向量化写（16/32 B），可把监视点前移到 `base+0xf0` 起 16 B 对齐处。

---

## 6. 对后续的启示（给批次五的**测试设计**输入，不含修复动作）

1. **契约断言：几何变更不得发生在绘制回调内。**
   本次证据链的关键一环是 `QChartAbstractWidget::paintEvent → relayout → layoutGlHost`（§2.2）。
   建议在测试约定中显式禁止"绘制回调内同步修改子控件几何"，并给出可断言的形态：
   - 单元/集成测试中安装**绘制期重入哨兵**：在 `paintEvent` 处理期间（事件过滤器可判定）若发生
     `QWidget::setGeometry/resize/move`（或 `layoutGlHost` 之类）则断言失败并打印调用栈；
   - 或反向断言：几何/布局变更只允许出现在事件循环的下一次迭代（可用 `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` 计数验证）。
2. **"自强迫 API/工具会改变时序"必须写进测试约定。**
   本轮已有三个实例：ASan 下不崩、MinGW/Release 下干净、`QT_OPENGL=software` 行为完全不同。
   ⇒ 约定：「某工具/配置下未复现」**不能**作为"无缺陷"的证据，至少需要**一份带插桩 + 一份不带插桩**的运行结果并列归档。
3. **重入/时序类缺陷的守护方式（测试侧）**：
   - **resize 风暴用例**保留（连续 resize、极扁/极窄、与视图拖动交替）——本次 Linux 侧已就绪（t80 `stress` 模式、t81 `resize` 驱动可复用）；
   - 断言要点从"是否崩溃"升级为"**期间是否出现被禁止的重入/同步几何变更**"+ "帧间不变量保持"；
   - 记录 p95/max 帧耗时与"卡住"阈值（普通运行表现为"卡住数秒"），作为可回归的软指标。
4. **平台取证能力要固化**：把 §5 的 UST 页堆配方与 `run_pageheap.bat` / `run_dbg.bat` 一起作为仓库外脚本清单保留（见 §7），
   并在测试约定中写明"Windows 驱动路径问题以页堆 + UST 为准"。

---

## 7. 产物清单（脚本与日志位置）

### 7.1 已复制入库（`build-linux/win_evidence/`）

| 文件 | 内容 | 本文引用 |
|---|---|---|
| `resize_dbg.log` | 首次调试堆运行（含 HEAP 消息与中断） | §2.1（:279-281）、§1.3（:166） |
| `resize_dbg4.log` | 调试堆 + 完整栈采集（**主证据**：释放路径栈与调用方栈） | §2.1（:173）、§2.2（:185-200,225-227,313-315） |
| `resize_pageheap.log` | 页堆运行（AV、异常码、`!analyze` 摘要） | §2.3（:171,444,447,488-489,572） |
| `dump_analysis.log` | 转储分析（反汇编、`!analyze -v`、`!address` 属性、栈） | §2.3（:57-58,119,128,254,288,340-360） |
| `dump_out.txt` | 转储分析原始输出（与上同源的完整版） | §2.3（:347-355） |
| `chan_test.log` | 一次 cdb「通道自检」失败记录（工具性插曲） | §2.4 注 |

### 7.2 Windows 侧（E 盘，**未入库**，由用户持有）

```
E:\Dujia\DuRunHan\Programs\cplusplus\QChartWidget\build-msvc\Test\demos\
  run_dbg.bat            调试堆运行脚本（cdb + 命令文件）
  run_pageheap.bat       页堆运行脚本（本次 §5.2 沿用它）
  exp1_software.bat      QT_OPENGL=software 对照实验
  exp2_mingw.bat         MinGW/Release 对照实验
  build-msvc-release.bat MSVC Release 构建
  build-msvc-asan.bat    MSVC + ASan 构建
  run_asan.bat           ASan 运行
  resize_*.log           各次运行日志（普通/调试堆/页堆）
  resize_crash.dmp       cdb 保存的崩溃转储（§2.3 分析对象）
```
> 另有 Linux 侧本轮诊断报告可作为对照证据：`build-linux/t80_probe/t80_diagnosis.md`（resize 压力 + 退出期崩溃归因）、
> `build-linux/t81_probe/t81_diagnosis.md`（ASan 1200 轮 + 静态筛查）。本文与其结论**互补而不重叠**。

---

## 8. 一句话小结

**已证实**：一个 240 B 的**驱动所有**堆块被写到 +0x108（超出 24 B，两次运行签名一致），损坏在 `ig9icd64` 的释放路径上被调试堆发现；
触发链经过 `paintEvent → relayout → layoutGlHost`（绘制回调内改 GL 子控件几何）；页堆下崩溃为 `Qt6Guid.dll` 内的 **use-after-free 读**（`cmp qword ptr [rax+8],0`，地址落在 `PAGE_NOACCESS`）。
**未证实**：**写者指令**未定位；"本库调用方式触发 Qt/驱动生命周期错配"仍是**假设**。
**下一步决定性动作**：按 §5 开启 **页堆 + UST**（必要时叠加 §5.4 的尾区写监视点），取**写者栈 / freed-by 栈**。
**与 t80 的两项关系**：退出期崩溃（A）与 `QChartGL::program()` 缓存无失效（B）都是**独立缺陷**，不参与本次堆损坏的解释。
