#pragma once

// Game.h 是项目的公共头文件：常量、结构体、全局状态声明和函数声明都在这里。
// 各个 .cpp 模块包含它之后，就能像标准 C++ 项目一样分别编译。

#ifndef NOMINMAX
#define NOMINMAX
#endif

// Windows 桌面程序需要直接调用 Win32 API：
// windows.h 提供窗口、消息、GDI 基础类型；windowsx.h 提供 GET_X_LPARAM 等鼠标坐标宏；
// mmsystem.h 用于 timeBeginPeriod，提高定时器精度，让 240 FPS 刷新更稳定。
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cwctype>
#include <cstdlib>
#include <string>
#include <vector>

namespace BugGame
{

    // 基础窗口与画面参数

    constexpr wchar_t kWindowClassName[] = L"WindowBugClass";
    constexpr wchar_t kDefaultWindowTitle[] = L"A White Bug";
    constexpr int kWindowWidth = 860;
    constexpr int kWindowHeight = 660;
    constexpr int kBoardWidth = 560;
    constexpr int kBoardHeight = 320;
    constexpr int kStructuredBoardWidth = 590;
    constexpr int kStructuredBoardHeight = 440;
    constexpr int kBoardTop = 118;
    constexpr int kBoardCornerRadius = 34;
    constexpr int kPaneThickness = 6;
    constexpr int kWallThickness = 18;
    constexpr int kWireThickness = 10;
    constexpr int kEndpointRadius = 26;
    constexpr int kAnchorHintRadius = 8;
    constexpr int kLevelTileSize = 104;
    constexpr int kLevelTileGap = 22;
    constexpr int kLevelTileCornerRadius = 18;
    constexpr int kLevelTileShadowOffset = 4;
    constexpr int kLevelSlotCount = 9;
    constexpr float kDrawPointStep = 3.0f;
    constexpr UINT_PTR kTimerId = 1;
    constexpr UINT kTargetFrameRate = 240;
    constexpr UINT kTimerIntervalMs = 1000 / kTargetFrameRate;
    constexpr DWORD kClearReturnDelayMs = 420;
    constexpr DWORD kInvalidFadeMs = 520;

    // 只使用黑白和三原色中的红黄蓝，符合作业要求。
    // 第八关允许通过文件名得到 green，它是“改名触发隐藏内容”的 Bug 机制。
    constexpr COLORREF kWhite = RGB(255, 255, 255);
    constexpr COLORREF kBlack = RGB(0, 0, 0);
    constexpr COLORREF kRed = RGB(255, 82, 82);
    constexpr COLORREF kYellow = RGB(255, 208, 20);
    constexpr COLORREF kBlue = RGB(52, 112, 255);
    constexpr COLORREF kGreen = RGB(35, 190, 90);
    constexpr COLORREF kWallGray = RGB(226, 226, 226);

    constexpr int kBendLevelIndex = 2;
    constexpr int kGapLevelIndex = 3;
    constexpr int kResizeLevelIndex = 4;
    constexpr int kDividerLevelIndex = 5;
    constexpr int kBoxLevelIndex = 6;
    constexpr int kNameColorLevelIndex = 7;
    constexpr int kEraserLevelIndex = 8;
    // 开发调试开关：true 表示选关界面里所有已做好的关卡都可以直接打开。
    // 正式录屏版本保持 false，玩家需要逐关通关；如果之后调试关卡，可以临时改 true。
    constexpr bool kDeveloperUnlockAllLevels = false;

    // 二维坐标。使用 float 让鼠标轨迹和平滑算法保留小数，画线更自然。
    struct Vec2
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    // 一条线段由两个端点组成，同时记录它属于哪一条路径。
    struct Segment
    {
        COLORREF color = kBlack;
        Vec2 a{};
        Vec2 b{};
        // 记录线段属于哪条路径、路径中的第几小段。
        // 检测交叉时会跳过同一路径的相邻线段，避免把正常连接误判成 crossing。
        int pathIndex = -1;
        int segmentIndex = -1;
    };

    // 一条彩色线路的静态定义：连接一对同色端点。
    struct WireDefinition
    {
        COLORREF color = kBlack;
        // 锚点编号来自下面 15 个相对坐标。
        // 例如 { kRed, 5, 9 } 表示红色从 5 号点连接到 9 号点。
        int startAnchor = 0;
        int endAnchor = 0;
    };

    // 一关的全部配置：只保存真正参与玩法判定的端点连接关系。
    // 关卡名和提示文字已经删除，保持画面和数据都足够简约。
    struct LevelDefinition
    {
        std::vector<WireDefinition> wires;
    };

    // 运行时的一条玩家绘制路径。
    struct PathState
    {
        // points 保存玩家真实鼠标轨迹。检测用原始点，绘制时再平滑，判定会更稳定。
        std::vector<Vec2> points;
        bool connected = false;
        bool startsAtStartAnchor = true;
    };

    // 非法路径淡出动画的数据：线穿墙或交叉时，先保存轨迹，再逐帧变淡。
    struct FadingPath
    {
        std::vector<Vec2> points;
        COLORREF color = kBlack;
        DWORD startedAtTick = 0;
    };

    struct EraserCircle
    {
        // 早期第九关尝试过“画圆识别”，后来改成涂白判定。
        // 这个结构体保留了当时的迭代痕迹，也方便说明你调过不同方案。
        Vec2 center{};
        float radius = 0.0f;
        std::vector<Vec2> points;
    };

    enum class ScreenMode
    {
        LevelSelect,
        Playing,
    };

    // 游戏运行时的总状态。
    struct GameState
    {
        // 当前游戏窗口句柄。Win32 绘制、鼠标捕获和窗口尺寸查询都要用到它。
        HWND hwnd = nullptr;
        // defs 是全部关卡配置；wires / paths 是当前关卡的连接规则和玩家路径。
        std::vector<LevelDefinition> defs;
        std::vector<WireDefinition> wires;
        std::vector<PathState> paths;
        std::vector<FadingPath> fadingPaths;
        // completedLevels / unlockedLevels 共同实现“逐关解锁”和关闭后保存进度。
        std::vector<bool> completedLevels;
        int levelIndex = 0;
        int unlockedLevels = 1;
        int hoveredLevel = -1;
        // activeWire 表示正在画第几条线；activeFromStart 表示从起点还是终点开始。
        int activeWire = -1;
        bool activeFromStart = true;
        bool drawing = false;
        bool cleared = false;
        // 普通关卡通关后会短暂停留，再自动回到选关界面。
        bool pendingReturnToSelect = false;
        int crossings = 0;
        DWORD clearedAtTick = 0;
        ScreenMode screen = ScreenMode::LevelSelect;
        Vec2 activeCursor{};
        // fixedAnchors 用于实现“缩放窗口时关卡不重新居中”。
        // visualAnchors 单独保存可见端点，方便特殊关卡做显示变化。
        RECT lastBoardRect{};
        RECT fixedAnchorRect{};
        std::array<Vec2, 15> fixedAnchors{};
        std::array<Vec2, 15> visualAnchors{};
        // 第五关：上方点在窗口内，下方点初始藏在窗口下边框外。
        int resizeLevelTopY = 0;
        int resizeLevelHiddenY = 0;
        // 第七关：盒子机关的位置和两个红点坐标。
        Vec2 boxLeftDot{};
        Vec2 boxRightDot{};
        RECT boxFrameRect{};
        // 第九关：白色画笔和涂抹轨迹。
        bool erasing = false;
        Vec2 eraserCursor{};
        Vec2 eraserStart{};
        std::vector<EraserCircle> erasedCircles;
        std::vector<Vec2> erasedStroke;
        std::vector<Vec2> activeEraserStroke;
        std::vector<std::vector<Vec2>> eraserPaintStrokes;
        std::wstring status = L"Press on a colored dot and draw to its twin.";
    };

    // 15 个锚点按照“3 行 x 5 列”的方式排布在窗体内部。
    // 坐标是相对于棋盘矩形的比例值，不是固定像素；窗口变化时可以按比例换算到真实位置。
    constexpr std::array<Vec2, 15> kAnchorLayout = {{
        { 0.16f, 0.24f }, { 0.35f, 0.24f }, { 0.50f, 0.24f }, { 0.65f, 0.24f }, { 0.84f, 0.24f },
        { 0.16f, 0.50f }, { 0.35f, 0.50f }, { 0.50f, 0.50f }, { 0.65f, 0.50f }, { 0.84f, 0.50f },
        { 0.16f, 0.76f }, { 0.35f, 0.76f }, { 0.50f, 0.76f }, { 0.65f, 0.76f }, { 0.84f, 0.76f },
    }};

    // 第三关专用锚点：部分点贴近墙体缺口，逼迫玩家画出弯曲路线。
    constexpr std::array<Vec2, 15> kBendAnchorLayout = {{
        { 0.145f, 0.195f }, { 0.35f, 0.24f }, { 0.50f, 0.24f }, { 0.655f, 0.195f }, { 0.84f, 0.24f },
        { 0.16f, 0.50f }, { 0.35f, 0.50f }, { 0.405f, 0.345f }, { 0.65f, 0.50f }, { 0.84f, 0.50f },
        { 0.16f, 0.76f }, { 0.325f, 0.705f }, { 0.50f, 0.76f }, { 0.655f, 0.815f }, { 0.850f, 0.815f },
    }};

    // 第四关专用锚点：黄点靠近水平隔板缺口，红蓝分布在不同区域。
    constexpr std::array<Vec2, 15> kGapAnchorLayout = {{
        { 0.115f, 0.205f }, { 0.35f, 0.24f }, { 0.500f, 0.305f }, { 0.65f, 0.24f }, { 0.84f, 0.24f },
        { 0.16f, 0.50f }, { 0.245f, 0.500f }, { 0.50f, 0.50f }, { 0.755f, 0.500f }, { 0.84f, 0.50f },
        { 0.16f, 0.76f }, { 0.35f, 0.76f }, { 0.500f, 0.705f }, { 0.65f, 0.76f }, { 0.895f, 0.805f },
    }};

    // 第六关专用锚点：左右两侧被中间竖直隔板隔开。
    constexpr std::array<Vec2, 15> kDividerAnchorLayout = {{
        { 0.20f, 0.34f }, { 0.35f, 0.24f }, { 0.50f, 0.24f }, { 0.65f, 0.24f }, { 0.80f, 0.34f },
        { 0.16f, 0.50f }, { 0.38f, 0.48f }, { 0.50f, 0.50f }, { 0.62f, 0.48f }, { 0.84f, 0.50f },
        { 0.20f, 0.62f }, { 0.35f, 0.76f }, { 0.50f, 0.76f }, { 0.65f, 0.76f }, { 0.80f, 0.62f },
    }};

    // 全局游戏状态。
    extern GameState g;

    // -----------------------------
    // 函数声明区
    // -----------------------------
    // 下方按模块顺序声明函数。声明集中在头文件，实现分散在对应 cpp 文件里。

    // Layout.cpp：坐标、窗口区域、锚点和特殊关卡几何位置。
    RECT MakeRect(int left, int top, int right, int bottom);
    int WidthOf(const RECT& rect);
    int HeightOf(const RECT& rect);
    RECT GetBoardRect();
    RECT GetAnchorRect();
    RECT GetLevelGridRect();
    RECT GetLevelButtonRect(int levelIndex);
    const std::array<Vec2, 15>& AnchorLayoutForLevel(int levelIndex);
    Vec2 AnchorFromRect(const RECT& board, const std::array<Vec2, 15>& layout, int anchorId);
    void CaptureLevelGeometry(int index);
    Vec2 AnchorToClient(int anchorId);
    Vec2 EndpointVisualToClient(int anchorId);
    Vec2 ResizeLevelAnchorToClient(int anchorId);
    Vec2 LevelSelectToClient(int levelIndex);
    Vec2 ClampToBoard(Vec2 point);
    // GeometryAndCollision.cpp：距离、交叉、墙体碰撞和第九关涂白判定。
    float DistanceSquared(const Vec2& a, const Vec2& b);
    float DistancePointToSegmentSquared(const Vec2& point, const Vec2& a, const Vec2& b);
    float StrokeLength(const std::vector<Vec2>& points);
    bool StrokePassesNear(const std::vector<Vec2>& points, const Vec2& target, float radius);
    bool TryRecognizeCircle(const std::vector<Vec2>& points, EraserCircle& circle);
    bool StrokeTouchesCircle(const std::vector<Vec2>& points, const EraserCircle& circle);
    Vec2 FinalBugPointCenter();
    COLORREF FinalBugPointColor();
    float FinalBugPaintCoverage();
    bool IsFinalBugPainted();
    bool NearlyEqual(float a, float b);
    bool NearlySamePoint(const Vec2& a, const Vec2& b);
    float Cross(const Vec2& a, const Vec2& b, const Vec2& c);
    bool OnSegment(const Vec2& a, const Vec2& b, const Vec2& point);
    bool SegmentsIntersectInclusive(const Vec2& firstA, const Vec2& firstB, const Vec2& secondA, const Vec2& secondB);
    bool SegmentsIntersect(const Segment& first, const Segment& second);
    bool PointInsideRect(const Vec2& point, const RECT& rect);
    bool SegmentIntersectsRect(const Vec2& a, const Vec2& b, const RECT& rect);
    bool PointNearVisibleEndpoint(const Vec2& point);
    bool SegmentHitsWallRect(const Vec2& a, const Vec2& b, const RECT& rect);
    bool SegmentHitsWallRects(const Vec2& a, const Vec2& b, const std::vector<RECT>& walls);
    std::vector<RECT> GetBendWallRects();
    std::vector<RECT> GetGapWallRects();
    bool SegmentHitsBendWall(const Vec2& a, const Vec2& b);
    bool SegmentHitsGapWall(const Vec2& a, const Vec2& b);
    RECT GetDividerWallRect();
    bool SegmentHitsDividerWall(const Vec2& a, const Vec2& b);
    bool PointOutsideClient(const Vec2& point);
    RECT GetBoxFrameRect();
    std::array<RECT, 4> GetBoxWallRects();
    bool SegmentHitsBoxWall(const Vec2& a, const Vec2& b);
    bool SegmentHitsActiveWall(const Vec2& a, const Vec2& b);
    bool IsEraserLevel();
    bool IsEraserPuzzleSolved();
    void FinishEraserLevelIfSolved();
    // SystemProgress.cpp：读取 exe 名称、背景改色、进度保存和读取。
    std::wstring GetExecutableFileName();
    std::wstring GetExecutableStem();
    std::wstring ToLowerCopy(std::wstring text);
    COLORREF BackgroundColorFromExecutableName();
    COLORREF CurrentBackgroundColor();
    std::wstring WindowTitleFromExecutableName();
    std::wstring GetExecutableDirectory();
    std::wstring GetSaveFilePath();
    bool HasCompletedAllLevels();
    void ResetProgressToStart();
    void NormalizeSequentialProgress();
    void SaveProgress();
    bool LoadProgress();
    // Graphics.cpp：GDI 绘制工具函数和特殊关卡绘制。
    HICON CreateGameIcon(int size);
    COLORREF BlendColor(COLORREF from, COLORREF to, float amount);
    HPEN CreateRoundedPen(COLORREF color, int width);
    void FillRectColor(HDC hdc, const RECT& rect, COLORREF color);
    void DrawTextBlock(HDC hdc, const std::wstring& text, const RECT& rect, int size, int weight, UINT format);
    void DrawCircle(HDC hdc, const Vec2& center, int radius, COLORREF fill, COLORREF stroke, int strokeWidth);
    void DrawRoundedRectBlock(HDC hdc, const RECT& rect, int radius, COLORREF fill, COLORREF stroke, int strokeWidth);
    void DrawColoredStroke(HDC hdc, const std::vector<Vec2>& points, COLORREF color, int width);
    void DrawWhiteStroke(HDC hdc, const std::vector<Vec2>& points);
    void DrawFadingPaths(HDC hdc);
    void DrawBoardFrame(HDC hdc, const RECT& board);
    void DrawBendStructure(HDC hdc);
    void DrawGapStructure(HDC hdc);
    void DrawDividerStructure(HDC hdc);
    void DrawBoxStructure(HDC hdc);
    void DrawEraserLevel(HDC hdc, const RECT& clientRect);
    // Levels.cpp / Levels/*.cpp：关卡配置。
    std::vector<LevelDefinition> BuildLevels();
    // RulesAndFlow.cpp：通关规则、非法路径淡出、选关和流程切换。
    std::wstring ColorName(COLORREF color);
    std::vector<Segment> BuildSegments();
    std::vector<Vec2> BuildDisplayPath(const PathState& path, bool appendCursor);
    bool SegmentHasIllegalContact(const Segment& candidate);
    bool PathHasIllegalContact(int pathIndex);
    void AddFadingPath(const PathState& path, COLORREF color);
    void RejectActivePathWithFade();
    bool UpdateFadingPaths();
    void ShiftPaths(float dx, float dy);
    bool IsLevelUnlocked(int index);
    void OpenLevelSelect(int focusIndex);
    void RefreshState();
    void LoadLevel(int index);
    void RestartLevel();
    void NextLevel();
    // Input.cpp：鼠标输入、画线、松开判定和第九关白色画笔。
    bool FindEndpointHit(POINT point, int& wireIndex, bool& startsAtStartAnchor);
    bool FindLevelButtonHit(POINT point, int& levelIndex);
    void ResetEraserState();
    void BeginEraserAction(POINT point);
    void UpdateEraserAction(POINT point);
    void EndEraserAction(POINT point);
    void CancelEraserAction();
    void BeginDraw(int wireIndex, bool startsAtStartAnchor);
    void AppendActivePoint(Vec2 point);
    void UpdateDraw(POINT point);
    void EndDraw(POINT point);
    void CancelDraw();
    void UpdateHoverLevel(POINT point);
    void UpdateAutoReturn();
    // RenderScene.cpp：总绘制入口和双缓冲刷新。
    void DrawScene(HDC hdc, const RECT& clientRect);
    void Render(HWND hwnd);

    // 每个关卡都是一个独立 .cpp，方便讲解和维护。
    LevelDefinition BuildLevel01Boot();
    LevelDefinition BuildLevel02Sweep();
    LevelDefinition BuildLevel03Bend();
    LevelDefinition BuildLevel04Gap();
    LevelDefinition BuildLevel05Resize();
    LevelDefinition BuildLevel06Divide();
    LevelDefinition BuildLevel07Box();
    LevelDefinition BuildLevel08White();
    LevelDefinition BuildLevel09Erase();
}
