#include "Game.h"

using namespace BugGame;

// WinMain.cpp 是标准 Win32 程序入口模块。
// 主要职责：
// 1. 注册窗口类、创建窗口和图标。
// 2. 处理 Windows 消息，例如鼠标、键盘、重绘、定时器和窗口缩放。
// 3. 在 WM_CREATE / WM_DESTROY 里完成游戏初始化和进度保存。
LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        // 窗口刚创建时初始化整个游戏。
        // g.defs 是全部关卡，completedLevels 和 unlockedLevels 是正式流程的进度状态。
        g.hwnd = hwnd;
        g.defs = BuildLevels();
        g.completedLevels.assign(g.defs.size(), false);
        g.unlockedLevels = kDeveloperUnlockAllLevels ? static_cast<int>(g.defs.size()) : 1;
        LoadProgress();
        const std::wstring commandLine = GetCommandLineW();
        if (kDeveloperUnlockAllLevels && commandLine.find(L"--level=3") != std::wstring::npos)
        {
            g.unlockedLevels = static_cast<int>(g.defs.size());
            LoadLevel(kBendLevelIndex);
        }
        else if (kDeveloperUnlockAllLevels && commandLine.find(L"--level=4") != std::wstring::npos)
        {
            g.unlockedLevels = static_cast<int>(g.defs.size());
            LoadLevel(kGapLevelIndex);
        }
        else if (kDeveloperUnlockAllLevels && commandLine.find(L"--level=5") != std::wstring::npos)
        {
            g.unlockedLevels = static_cast<int>(g.defs.size());
            LoadLevel(kResizeLevelIndex);
        }
        else if (kDeveloperUnlockAllLevels && commandLine.find(L"--level=6") != std::wstring::npos)
        {
            g.unlockedLevels = static_cast<int>(g.defs.size());
            LoadLevel(kDividerLevelIndex);
        }
        else if (kDeveloperUnlockAllLevels && commandLine.find(L"--level=7") != std::wstring::npos)
        {
            g.unlockedLevels = static_cast<int>(g.defs.size());
            LoadLevel(kBoxLevelIndex);
        }
        else if (kDeveloperUnlockAllLevels && commandLine.find(L"--level=8") != std::wstring::npos)
        {
            g.unlockedLevels = static_cast<int>(g.defs.size());
            LoadLevel(kNameColorLevelIndex);
        }
        else if (kDeveloperUnlockAllLevels && commandLine.find(L"--level=9") != std::wstring::npos)
        {
            g.unlockedLevels = static_cast<int>(g.defs.size());
            LoadLevel(kEraserLevelIndex);
        }
        else
        {
            LoadLevel(0);
            OpenLevelSelect(0);
        }
        // timeBeginPeriod(1) 把系统定时器精度提高到 1ms。
        // 没有它时，Windows 可能把定时器合并到更低频率，画线会显得不够跟手。
        timeBeginPeriod(1);
        // 用 240 FPS 附近的高频率重绘，提高画线时的顺滑感。
        SetTimer(hwnd, kTimerId, kTimerIntervalMs, nullptr);
        return 0;
    }

    case WM_SIZE:
    {
        // 关卡内容在加载时已经定格；缩放窗口只裁剪可见区域，不再自动平移或居中。
        if (g.hwnd == hwnd)
        {
            RefreshState();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        // 鼠标左键按下：如果点中了端点，就开始绘制该颜色的线。
        POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (g.screen == ScreenMode::LevelSelect)
        {
            int levelIndex = -1;
            if (FindLevelButtonHit(point, levelIndex))
            {
                g.levelIndex = levelIndex;
                LoadLevel(levelIndex);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        if (IsEraserLevel())
        {
            BeginEraserAction(point);
            return 0;
        }

        int wireIndex = -1;
        bool startsAtStartAnchor = true;
        if (FindEndpointHit(point, wireIndex, startsAtStartAnchor))
        {
            BeginDraw(wireIndex, startsAtStartAnchor);
        }
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        // 鼠标移动：如果正在画线，就持续追加路径点。
        POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (g.screen == ScreenMode::LevelSelect)
        {
            const int previousHover = g.hoveredLevel;
            UpdateHoverLevel(point);
            if (previousHover != g.hoveredLevel)
            {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
        else
        {
            if (IsEraserLevel())
            {
                UpdateEraserAction(point);
            }
            else
            {
                UpdateDraw(point);
            }
        }
        return 0;
    }

    case WM_LBUTTONUP:
    {
        // 鼠标松开：尝试把当前线闭合到正确的同色端点。
        POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (g.screen == ScreenMode::Playing)
        {
            if (IsEraserLevel())
            {
                EndEraserAction(point);
            }
            else
            {
                EndDraw(point);
            }
        }
        return 0;
    }

    case WM_SETCURSOR:
        // 第九关用我们自己绘制的白色橡皮预览代替系统箭头，窗口边框仍保留缩放光标。
        if (LOWORD(lParam) == HTCLIENT && IsEraserLevel())
        {
            SetCursor(nullptr);
            return TRUE;
        }
        break;

    case WM_KEYDOWN:
        // 键盘操作只保留重开、下一关和退出。
        switch (wParam)
        {
        case 'R':
            if (g.screen == ScreenMode::Playing)
            {
                RestartLevel();
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case VK_RETURN:
            NextLevel();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case VK_ESCAPE:
            if (g.screen == ScreenMode::Playing)
            {
                OpenLevelSelect(g.levelIndex);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            else
            {
                DestroyWindow(hwnd);
            }
            return 0;
        default:
            break;
        }
        break;

    case WM_CAPTURECHANGED:
        // 如果鼠标捕获意外丢失，就取消正在绘制的路径，避免状态卡住。
        CancelDraw();
        CancelEraserAction();
        return 0;

    case WM_PAINT:
        Render(hwnd);
        return 0;

    case WM_TIMER:
        if (wParam == kTimerId)
        {
            // 高频定时器负责驱动画面刷新：
            // 正在画线、正在涂白、非法线淡出、最终打字动画都会触发重绘。
            const bool fadingChanged = UpdateFadingPaths();
            UpdateAutoReturn();
            if (g.drawing || g.erasing || fadingChanged || (IsEraserLevel() && g.cleared))
            {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        break;

    case WM_ERASEBKGND:
        // 返回 1 告诉系统“背景我自己已经处理了”，配合双缓冲减少闪烁。
        return 1;

    case WM_DESTROY:
        // 用户关闭游戏时保存进度。
        // 如果已经全部通关，则重置到第一关，方便下一次演示或录屏从头开始。
        if (HasCompletedAllLevels())
        {
            ResetProgressToStart();
        }
        SaveProgress();
        KillTimer(hwnd, kTimerId);
        timeEndPeriod(1);
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

// Windows 程序入口：注册窗口类 -> 创建窗口 -> 显示窗口 -> 进入消息循环。
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    HICON largeIcon = CreateGameIcon(32);
    HICON smallIcon = CreateGameIcon(16);

    // WNDCLASSW 描述窗口类型：消息处理函数、图标、默认光标、背景等。
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = kWindowClassName;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hIcon = largeIcon != nullptr ? largeIcon : LoadIcon(nullptr, IDI_APPLICATION);
    windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));

    RegisterClassW(&windowClass);

    // 加上可缩放边框，这样把鼠标放到窗口边缘时就能直接调节宽高。
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME;
    RECT windowRect = MakeRect(0, 0, kWindowWidth, kWindowHeight);
    AdjustWindowRect(&windowRect, style, FALSE);
    const std::wstring windowTitle = WindowTitleFromExecutableName();

    HWND hwnd = CreateWindowExW(
        0,
        kWindowClassName,
        windowTitle.c_str(),
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        WidthOf(windowRect),
        HeightOf(windowRect),
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!hwnd)
    {
        if (largeIcon != nullptr)
        {
            DestroyIcon(largeIcon);
        }
        if (smallIcon != nullptr)
        {
            DestroyIcon(smallIcon);
        }
        return 0;
    }

    // 标题栏左上角的专属标识：红、蓝、黄三个小方块，对应游戏的极简三原色视觉。
    if (largeIcon != nullptr)
    {
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(largeIcon));
    }
    if (smallIcon != nullptr)
    {
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
    }

    ShowWindow(hwnd, showCommand);
    UpdateWindow(hwnd);

    MSG message{};
    // 标准 Windows 消息循环。
    // 程序大部分时间都在这里等待系统消息，然后分发给 WindowProc 处理。
    while (GetMessageW(&message, nullptr, 0, 0))
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    const int exitCode = static_cast<int>(message.wParam);
    if (largeIcon != nullptr)
    {
        DestroyIcon(largeIcon);
    }
    if (smallIcon != nullptr)
    {
        DestroyIcon(smallIcon);
    }
    return exitCode;
}
