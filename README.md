<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-00599C?style=flat-square&logo=cplusplus&logoColor=white" alt="C++" />
  <img src="https://img.shields.io/badge/Win32%20API-0078D6?style=flat-square&logo=windows&logoColor=white" alt="Win32 API" />
  <img src="https://img.shields.io/badge/Windows%2010%2B-0078D6?style=flat-square&logo=windows&logoColor=white" alt="Windows" />
  <img src="https://img.shields.io/badge/MinGW-00447F?style=flat-square" alt="MinGW" />
  <img src="https://img.shields.io/badge/MSVC-5C2D91?style=flat-square&logo=visualstudio&logoColor=white" alt="MSVC" />
  <img src="https://img.shields.io/github/stars/cmdpe/LiangWenTray?style=flat-square&logo=github" alt="stars" />
  <img src="https://img.shields.io/badge/license-MIT-blue?style=flat-square" alt="license" />
</p>

# LiangWenTray - 梁文峰/梁文谷 托盘提醒

基于 C++/Win32 开发的 Windows 托盘提醒小工具（整活向）。程序根据北京时间自动判断当前处于「梁文峰」时期还是「梁文谷」时期，通过托盘图标切换与气泡通知实时提醒。无窗口、无依赖、绿色单文件，兼容 MinGW 与 MSVC 编译。

## 功能特性

### 时期判断

根据北京时间自动切换时期，判断规则如下：

| 时期 | 时间段（北京时间） | 说明 |
| ---- | ------------------ | ---- |
| 梁文峰 | 09:00 – 12:00 | 上午工作时间 |
| 梁文峰 | 14:00 – 18:00 | 下午工作时间 |
| 梁文谷 | 其余时间 | 午休 / 夜间时段 |

> 判断规则：`09:00 ≤ t < 12:00` 或 `14:00 ≤ t < 18:00` 为梁文峰时期，其余时间为梁文谷时期。

### 托盘图标

- 程序常驻系统托盘，图标随时期自动切换（`1.ico` 梁文峰 / `2.ico` 梁文谷）
- 鼠标悬停托盘图标显示当前时期名称
- 图标加载失败时自动回退到系统默认图标，不影响运行

### 气泡通知

- 首次启动时弹出当前时期气泡
- 时期切换瞬间弹出气泡，显示当前北京时间与时期（如「现在是 12:00，梁文谷时期」）
- 气泡最长展示 10 秒，后台每秒检查一次状态

### 开机自启

- 支持通过命令行参数注册 / 取消开机自启：
  - `LiangWenTray.exe --install` 注册自启
  - `LiangWenTray.exe --uninstall` 取消自启
- 自启项写入当前用户注册表 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`

### 右键菜单

- 右键托盘图标弹出菜单，支持一键退出
- 退出时自动清理托盘图标，不留后台进程

## 界面设计

- 隐藏窗口 + 托盘常驻，无主界面、无弹窗打扰
- 双图标方案：`1.ico`（梁文峰）/ `2.ico`（梁文谷）随时期切换
- 图标从 exe 同级目录加载，更换图标只需替换 `1.ico` / `2.ico` 文件

## 项目结构

```
├── main.cpp              # 主程序（Win32 托盘应用，单文件）
├── 1.ico                 # 梁文峰时期托盘图标
├── 2.ico                 # 梁文谷时期托盘图标
└── LiangWenTray.exe      # 编译产物
```

## 开发与运行

### 环境要求

- Windows 10 / 11
- MinGW-w64（g++）或 MSVC（cl）

### 编译（MinGW）

```bash
g++ main.cpp -o LiangWenTray.exe -mwindows -luser32 -lshell32 -ladvapi32
```

### 编译（MSVC）

```bat
cl main.cpp /EHsc /link user32.lib shell32.lib advapi32.lib /SUBSYSTEM:WINDOWS
```

### 运行

```bash
# 直接运行（托盘常驻）
LiangWenTray.exe

# 注册开机自启
LiangWenTray.exe --install

# 取消开机自启
LiangWenTray.exe --uninstall
```

> 注意：`1.ico` 与 `2.ico` 需与 exe 放在同一目录。

## 技术栈

- **语言**：C++（Win32 / Shell API）
- **核心 API**：`NOTIFYICONDATA` 托盘、`SetTimer` 定时器、注册表操作
- **编译**：兼容 MinGW 与 MSVC
- **平台**：Windows 10 / 11

## 版本

- v1.0.0
