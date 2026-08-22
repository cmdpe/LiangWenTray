// LiangWenTray.cpp
// 梁文峰/梁文谷 托盘提醒程序（C++/Win32，兼容 MinGW 和 MSVC）

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <ctime>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")

// 常量
#define WM_TRAYICON   (WM_APP + 1)
#define ID_EXIT       1001
#define TIMER_CHECK   1
#define CHECK_INTERVAL_MS  1000

// 全局变量
HINSTANCE g_hInst = nullptr;
HWND g_hWnd = nullptr;
NOTIFYICONDATAW g_nid = {};
HICON g_iconFeng = nullptr;   // 梁文峰图标
HICON g_iconGu = nullptr;     // 梁文谷图标
int g_currentState = -1;      // 0=梁文峰, 1=梁文谷
bool g_trayAdded = false;

// 函数声明
enum class Period { Feng = 0, Gu = 1 };
Period GetCurrentPeriod();
void GetBeijingTime(int &hour, int &minute);
std::wstring GetExeDir();
bool LoadIcons();
void AddTrayIcon();
void UpdateTrayIcon(Period period);
void ShowBalloon(Period period, int hour, int minute);
void RemoveTrayIcon();
void ShowContextMenu(HWND hwnd);
void InstallStartup();
void UninstallStartup();

// 窗口过程
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TIMER:
        if (wParam == TIMER_CHECK) {
            Period p = GetCurrentPeriod();
            int newState = static_cast<int>(p);
            if (newState != g_currentState) {
                g_currentState = newState;
                UpdateTrayIcon(p);
                int hour, minute;
                GetBeijingTime(hour, minute);
                ShowBalloon(p, hour, minute);
            }
        }
        return 0;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            ShowContextMenu(hwnd);
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_EXIT) {
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_DESTROY:
        RemoveTrayIcon();
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// 获取当前北京时间的时分
void GetBeijingTime(int &hour, int &minute) {
    SYSTEMTIME st;
    GetSystemTime(&st);   // UTC 时间

    // 转换为北京时间（UTC+8）
    int totalMin = st.wHour * 60 + st.wMinute + 8 * 60;
    totalMin %= 24 * 60;
    if (totalMin < 0) totalMin += 24 * 60;
    hour = totalMin / 60;
    minute = totalMin % 60;
}

// 判断当前时期
Period GetCurrentPeriod() {
    int hour, minute;
    GetBeijingTime(hour, minute);

    bool morning = (hour >= 9 && hour < 12);
    bool afternoon = (hour >= 14 && hour < 18);
    if (morning || afternoon) {
        return Period::Feng;
    }
    return Period::Gu;
}

// 获取 exe 所在目录（包括末尾反斜杠）
std::wstring GetExeDir() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring full(path);
    size_t pos = full.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        return full.substr(0, pos + 1);
    }
    return L"";
}

// 加载图标（优先使用 exe 目录下的 1.ico / 2.ico，失败则使用系统默认图标）
bool LoadIcons() {
    std::wstring dir = GetExeDir();

    std::wstring fengPath = dir + L"1.ico";
    std::wstring guPath = dir + L"2.ico";

    g_iconFeng = (HICON)LoadImageW(nullptr, fengPath.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
    g_iconGu = (HICON)LoadImageW(nullptr, guPath.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE);

    if (!g_iconFeng) {
        g_iconFeng = LoadIcon(nullptr, IDI_APPLICATION);
    }
    if (!g_iconGu) {
        g_iconGu = LoadIcon(nullptr, IDI_APPLICATION);
    }

    return true; // 至少系统默认图标能加载
}

// 添加托盘图标（首次启动时调用，并弹出初始气泡）
void AddTrayIcon() {
    Period p = GetCurrentPeriod();
    g_currentState = static_cast<int>(p);
    int hour, minute;
    GetBeijingTime(hour, minute);

    // 配置 NOTIFYICONDATA
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = g_hWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_INFO;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = (p == Period::Feng) ? g_iconFeng : g_iconGu;
    wcscpy(g_nid.szTip, (p == Period::Feng) ? L"梁文峰" : L"梁文谷");

    // 气泡文本
    wchar_t timeStr[16];
    wsprintfW(timeStr, L"%02d:%02d", hour, minute);
    if (p == Period::Feng) {
        wsprintfW(g_nid.szInfo, L"现在是%s，梁文峰时期", timeStr);
        wcscpy(g_nid.szInfoTitle, L"梁文峰");
    } else {
        wsprintfW(g_nid.szInfo, L"现在是%s，梁文谷时期", timeStr);
        wcscpy(g_nid.szInfoTitle, L"梁文谷");
    }
    g_nid.dwInfoFlags = NIIF_INFO;
    g_nid.uTimeout = 10000;  // 10秒

    Shell_NotifyIconW(NIM_ADD, &g_nid);
    g_trayAdded = true;
}

// 更新托盘图标和悬停提示
void UpdateTrayIcon(Period period) {
    if (!g_trayAdded) return;

    g_nid.uFlags = NIF_ICON | NIF_TIP;
    g_nid.hIcon = (period == Period::Feng) ? g_iconFeng : g_iconGu;
    wcscpy(g_nid.szTip, (period == Period::Feng) ? L"梁文峰" : L"梁文谷");
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

// 弹出气泡通知
void ShowBalloon(Period period, int hour, int minute) {
    if (!g_trayAdded) return;

    wchar_t timeStr[16];
    wsprintfW(timeStr, L"%02d:%02d", hour, minute);

    g_nid.uFlags = NIF_INFO;
    if (period == Period::Feng) {
        wsprintfW(g_nid.szInfo, L"现在是%s，梁文峰时期", timeStr);
        wcscpy(g_nid.szInfoTitle, L"梁文峰");
    } else {
        wsprintfW(g_nid.szInfo, L"现在是%s，梁文谷时期", timeStr);
        wcscpy(g_nid.szInfoTitle, L"梁文谷");
    }
    g_nid.dwInfoFlags = NIIF_INFO;
    g_nid.uTimeout = 10000;

    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

// 删除托盘图标
void RemoveTrayIcon() {
    if (g_trayAdded) {
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        g_trayAdded = false;
    }
}

// 显示右键菜单
void ShowContextMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    AppendMenuW(hMenu, MF_STRING, ID_EXIT, L"退出");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);

    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(hMenu);
}

// 注册开机自启
void InstallStartup() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
        return;
    }

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring cmd = L"\"" + std::wstring(exePath) + L"\"";

    RegSetValueExW(hKey, L"LiangWenTray", 0, REG_SZ,
                   reinterpret_cast<const BYTE*>(cmd.c_str()),
                   static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);
}

// 取消开机自启
void UninstallStartup() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
        return;
    }

    RegDeleteValueW(hKey, L"LiangWenTray");
    RegCloseKey(hKey);
}

// 主入口
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    g_hInst = hInstance;

    // 处理命令行参数（安装/卸载自启）
    const wchar_t* cmdLine = GetCommandLineW();
    if (wcsstr(cmdLine, L"--install")) {
        InstallStartup();
        MessageBoxW(nullptr, L"已添加开机自启", L"梁文峰/梁文谷", MB_OK | MB_ICONINFORMATION);
        return 0;
    }
    if (wcsstr(cmdLine, L"--uninstall")) {
        UninstallStartup();
        MessageBoxW(nullptr, L"已移除开机自启", L"梁文峰/梁文谷", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    // 加载图标
    LoadIcons();

    // 注册窗口类
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"LiangWenTrayWindow";
    wc.hIcon = g_iconFeng;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    if (!RegisterClassW(&wc)) {
        MessageBoxW(nullptr, L"窗口类注册失败", L"错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    // 创建隐藏窗口
    g_hWnd = CreateWindowExW(0, wc.lpszClassName, L"LiangWenTray",
                             0, 0, 0, 0, 0,
                             nullptr, nullptr, hInstance, nullptr);
    if (!g_hWnd) {
        MessageBoxW(nullptr, L"窗口创建失败", L"错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    // 添加托盘图标（并弹出初始气泡）
    AddTrayIcon();

    // 启动定时器（每秒检查一次）
    SetTimer(g_hWnd, TIMER_CHECK, CHECK_INTERVAL_MS, nullptr);

    // 消息循环
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // 清理
    if (g_iconFeng) DestroyIcon(g_iconFeng);
    if (g_iconGu) DestroyIcon(g_iconGu);

    return 0;
}
