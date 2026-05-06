// 防止 windows.h 里的 min / max 宏污染 C++ 标准库。
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>

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

// 匿名命名空间中的内容只在当前 cpp 文件内可见，适合放全局常量和辅助函数。
namespace
{
    // 窗口与棋盘尺寸。整体依旧保持“小白窗”气质。
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
    constexpr UINT kTimerIntervalMs = 4;
    constexpr DWORD kClearReturnDelayMs = 420;

    // 只使用黑白和三原色中的红黄蓝，符合作业要求。
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
    // 开发调试开关：true 表示选关界面里所有已做好的关卡都可以直接打开。
    // 如果之后想恢复正式流程，把这里改成 false 即可回到逐关解锁。
    constexpr bool kDeveloperUnlockAllLevels = true;

    // 最基础的二维坐标类型，用来表示点的位置。
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
        int pathIndex = -1;
        int segmentIndex = -1;
    };

    // 一条彩色线路的静态定义：连接一对同色端点。
    struct WireDefinition
    {
        COLORREF color = kBlack;
        int startAnchor = 0;
        int endAnchor = 0;
    };

    // 一关的全部配置：名字、提示文字、端点连接关系。
    struct LevelDefinition
    {
        const wchar_t* name = L"";
        const wchar_t* hint = L"";
        std::vector<WireDefinition> wires;
    };

    // 运行时的一条玩家绘制路径。
    struct PathState
    {
        std::vector<Vec2> points;
        bool connected = false;
        bool startsAtStartAnchor = true;
    };

    enum class ScreenMode
    {
        LevelSelect,
        Playing,
    };

    // 游戏运行时的总状态。
    struct GameState
    {
        HWND hwnd = nullptr;
        std::vector<LevelDefinition> defs;
        std::vector<WireDefinition> wires;
        std::vector<PathState> paths;
        std::vector<bool> completedLevels;
        int levelIndex = 0;
        int unlockedLevels = 1;
        int hoveredLevel = -1;
        int activeWire = -1;
        bool activeFromStart = true;
        bool drawing = false;
        bool cleared = false;
        bool pendingReturnToSelect = false;
        int crossings = 0;
        DWORD clearedAtTick = 0;
        ScreenMode screen = ScreenMode::LevelSelect;
        Vec2 activeCursor{};
        RECT lastBoardRect{};
        RECT fixedAnchorRect{};
        std::array<Vec2, 15> fixedAnchors{};
        std::array<Vec2, 15> visualAnchors{};
        int resizeLevelTopY = 0;
        int resizeLevelHiddenY = 0;
        Vec2 boxLeftDot{};
        Vec2 boxRightDot{};
        RECT boxFrameRect{};
        std::wstring levelName;
        std::wstring hint;
        std::wstring status = L"Press on a colored dot and draw to its twin.";
    };

    // 15 个锚点按照“3 行 x 5 列”的方式排布在窗体内部。
    constexpr std::array<Vec2, 15> kAnchorLayout = {{
        { 0.16f, 0.24f }, { 0.35f, 0.24f }, { 0.50f, 0.24f }, { 0.65f, 0.24f }, { 0.84f, 0.24f },
        { 0.16f, 0.50f }, { 0.35f, 0.50f }, { 0.50f, 0.50f }, { 0.65f, 0.50f }, { 0.84f, 0.50f },
        { 0.16f, 0.76f }, { 0.35f, 0.76f }, { 0.50f, 0.76f }, { 0.65f, 0.76f }, { 0.84f, 0.76f },
    }};

    constexpr std::array<Vec2, 15> kBendAnchorLayout = {{
        { 0.145f, 0.195f }, { 0.35f, 0.24f }, { 0.50f, 0.24f }, { 0.655f, 0.195f }, { 0.84f, 0.24f },
        { 0.16f, 0.50f }, { 0.35f, 0.50f }, { 0.405f, 0.345f }, { 0.65f, 0.50f }, { 0.84f, 0.50f },
        { 0.16f, 0.76f }, { 0.325f, 0.705f }, { 0.50f, 0.76f }, { 0.655f, 0.815f }, { 0.850f, 0.815f },
    }};

    constexpr std::array<Vec2, 15> kGapAnchorLayout = {{
        { 0.115f, 0.205f }, { 0.35f, 0.24f }, { 0.500f, 0.305f }, { 0.65f, 0.24f }, { 0.84f, 0.24f },
        { 0.16f, 0.50f }, { 0.245f, 0.500f }, { 0.50f, 0.50f }, { 0.755f, 0.500f }, { 0.84f, 0.50f },
        { 0.16f, 0.76f }, { 0.35f, 0.76f }, { 0.500f, 0.705f }, { 0.65f, 0.76f }, { 0.895f, 0.805f },
    }};

    constexpr std::array<Vec2, 15> kDividerAnchorLayout = {{
        { 0.20f, 0.34f }, { 0.35f, 0.24f }, { 0.50f, 0.24f }, { 0.65f, 0.24f }, { 0.80f, 0.34f },
        { 0.16f, 0.50f }, { 0.38f, 0.48f }, { 0.50f, 0.50f }, { 0.62f, 0.48f }, { 0.84f, 0.50f },
        { 0.20f, 0.62f }, { 0.35f, 0.76f }, { 0.50f, 0.76f }, { 0.65f, 0.76f }, { 0.80f, 0.62f },
    }};

    // 全局游戏状态。
    GameState g;

    // 下面是前置声明，便于把主要逻辑按功能分块书写。
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
    float DistanceSquared(const Vec2& a, const Vec2& b);
    bool NearlyEqual(float a, float b);
    bool NearlySamePoint(const Vec2& a, const Vec2& b);
    float Cross(const Vec2& a, const Vec2& b, const Vec2& c);
    bool OnSegment(const Vec2& a, const Vec2& b, const Vec2& point);
    bool SegmentsIntersectInclusive(const Vec2& firstA, const Vec2& firstB, const Vec2& secondA, const Vec2& secondB);
    bool SegmentsIntersect(const Segment& first, const Segment& second);
    bool PointInsideRect(const Vec2& point, const RECT& rect);
    bool SegmentIntersectsRect(const Vec2& a, const Vec2& b, const RECT& rect);
    RECT GetDividerWallRect();
    bool SegmentHitsDividerWall(const Vec2& a, const Vec2& b);
    bool PointOutsideClient(const Vec2& point);
    RECT GetBoxFrameRect();
    std::array<RECT, 4> GetBoxWallRects();
    bool SegmentHitsBoxWall(const Vec2& a, const Vec2& b);
    bool SegmentHitsActiveWall(const Vec2& a, const Vec2& b);
    std::wstring GetExecutableFileName();
    std::wstring GetExecutableStem();
    std::wstring ToLowerCopy(std::wstring text);
    COLORREF BackgroundColorFromExecutableName();
    COLORREF CurrentBackgroundColor();
    std::wstring WindowTitleFromExecutableName();
    std::wstring GetExecutableDirectory();
    std::wstring GetSaveFilePath();
    void SaveProgress();
    bool LoadProgress();
    HPEN CreateRoundedPen(COLORREF color, int width);
    void FillRectColor(HDC hdc, const RECT& rect, COLORREF color);
    void DrawTextBlock(HDC hdc, const std::wstring& text, const RECT& rect, int size, int weight, UINT format);
    void DrawCircle(HDC hdc, const Vec2& center, int radius, COLORREF fill, COLORREF stroke, int strokeWidth);
    void DrawRoundedRectBlock(HDC hdc, const RECT& rect, int radius, COLORREF fill, COLORREF stroke, int strokeWidth);
    void DrawBoardFrame(HDC hdc, const RECT& board);
    void DrawBendStructure(HDC hdc);
    void DrawGapStructure(HDC hdc);
    void DrawDividerStructure(HDC hdc);
    void DrawBoxStructure(HDC hdc);
    std::vector<LevelDefinition> BuildLevels();
    std::wstring ColorName(COLORREF color);
    std::vector<Segment> BuildSegments();
    std::vector<Vec2> BuildDisplayPath(const PathState& path, bool appendCursor);
    void ShiftPaths(float dx, float dy);
    bool IsLevelUnlocked(int index);
    void OpenLevelSelect(int focusIndex);
    void RefreshState();
    void LoadLevel(int index);
    void RestartLevel();
    void NextLevel();
    bool FindEndpointHit(POINT point, int& wireIndex, bool& startsAtStartAnchor);
    bool FindLevelButtonHit(POINT point, int& levelIndex);
    void BeginDraw(int wireIndex, bool startsAtStartAnchor);
    void AppendActivePoint(Vec2 point);
    void UpdateDraw(POINT point);
    void EndDraw(POINT point);
    void CancelDraw();
    void UpdateHoverLevel(POINT point);
    void UpdateAutoReturn();
    void DrawScene(HDC hdc, const RECT& clientRect);
    void Render(HWND hwnd);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand);

namespace
{
    // 生成一个 Windows 常用的矩形结构 RECT。
    RECT MakeRect(int left, int top, int right, int bottom)
    {
        RECT rect{};
        rect.left = left;
        rect.top = top;
        rect.right = right;
        rect.bottom = bottom;
        return rect;
    }

    int WidthOf(const RECT& rect) { return rect.right - rect.left; }
    int HeightOf(const RECT& rect) { return rect.bottom - rect.top; }

    // 计算白色主区域内部那块“谜题面板”的实际位置。
    RECT GetBoardRect()
    {
        RECT client{};
        GetClientRect(g.hwnd, &client);
        const int left = (WidthOf(client) - kBoardWidth) / 2;
        const int top = (HeightOf(client) - kBoardHeight) / 2;
        return MakeRect(left, top, left + kBoardWidth, top + kBoardHeight);
    }

    RECT GetAnchorRect()
    {
        if (g.screen == ScreenMode::Playing &&
            (g.levelIndex == kBendLevelIndex || g.levelIndex == kGapLevelIndex))
        {
            RECT client{};
            GetClientRect(g.hwnd, &client);

            const int width = std::min(kStructuredBoardWidth, std::max(320, WidthOf(client) - 160));
            const int height = std::min(kStructuredBoardHeight, std::max(240, HeightOf(client) - 170));
            const int left = (WidthOf(client) - width) / 2;
            const int top = (HeightOf(client) - height) / 2 + 4;
            return MakeRect(left, top, left + width, top + height);
        }

        return GetBoardRect();
    }

    RECT GetLevelGridRect()
    {
        RECT client{};
        GetClientRect(g.hwnd, &client);

        const int totalSize = kLevelTileSize * 3 + kLevelTileGap * 2;
        const int left = (WidthOf(client) - totalSize) / 2;
        const int centeredTop = (HeightOf(client) - totalSize) / 2 - 16;
        const int top = std::max(54, centeredTop);
        return MakeRect(left, top, left + totalSize, top + totalSize);
    }

    RECT GetLevelButtonRect(int levelIndex)
    {
        const RECT grid = GetLevelGridRect();
        const int column = levelIndex % 3;
        const int row = levelIndex / 3;
        const int left = grid.left + column * (kLevelTileSize + kLevelTileGap);
        const int top = grid.top + row * (kLevelTileSize + kLevelTileGap);
        return MakeRect(left, top, left + kLevelTileSize, top + kLevelTileSize);
    }

    const std::array<Vec2, 15>& AnchorLayoutForLevel(int levelIndex)
    {
        if (levelIndex == kBendLevelIndex)
        {
            return kBendAnchorLayout;
        }
        if (levelIndex == kGapLevelIndex)
        {
            return kGapAnchorLayout;
        }
        if (levelIndex == kDividerLevelIndex)
        {
            return kDividerAnchorLayout;
        }
        return kAnchorLayout;
    }

    Vec2 AnchorFromRect(const RECT& board, const std::array<Vec2, 15>& layout, int anchorId)
    {
        const Vec2 anchor = layout[static_cast<size_t>(anchorId)];
        return Vec2{
            board.left + anchor.x * static_cast<float>(WidthOf(board)),
            board.top + anchor.y * static_cast<float>(HeightOf(board))
        };
    }

    // 把锚点编号转换成窗口客户区中的真实像素坐标。
    Vec2 AnchorToClient(int anchorId)
    {
        if (g.screen == ScreenMode::Playing)
        {
            return g.fixedAnchors[static_cast<size_t>(anchorId)];
        }

        const RECT board = GetAnchorRect();
        return AnchorFromRect(board, kAnchorLayout, anchorId);
    }

    Vec2 EndpointVisualToClient(int anchorId)
    {
        if (g.screen == ScreenMode::Playing)
        {
            return g.visualAnchors[static_cast<size_t>(anchorId)];
        }

        return AnchorToClient(anchorId);
    }

    Vec2 ResizeLevelAnchorToClient(int anchorId)
    {
        RECT client{};
        GetClientRect(g.hwnd, &client);

        // 第五关的谜题点不依赖普通棋盘，而是依赖窗口尺寸本身。
        // 上方三个点一直留在窗口里，下方三个点被放在初始下边框外，
        // 玩家必须把窗口下边框往下拖大，才能看到并连接它们。
        const std::array<float, 3> xRel = { 0.22f, 0.50f, 0.78f };
        int column = 0;
        bool hiddenBelowWindow = false;
        switch (anchorId)
        {
        case 0:
            column = 0;
            break;
        case 1:
            column = 1;
            break;
        case 2:
            column = 2;
            break;
        case 10:
            column = 0;
            hiddenBelowWindow = true;
            break;
        case 11:
            column = 1;
            hiddenBelowWindow = true;
            break;
        case 12:
            column = 2;
            hiddenBelowWindow = true;
            break;
        default:
            break;
        }

        const float x = client.left + xRel[static_cast<size_t>(column)] * static_cast<float>(WidthOf(client));
        const float y = static_cast<float>(hiddenBelowWindow ? g.resizeLevelHiddenY : g.resizeLevelTopY);
        return Vec2{ x, y };
    }

    void CaptureLevelGeometry(int index)
    {
        // 进入关卡时把所有点和机关的基础矩形“拍照”保存。
        // 之后调整窗口大小只会裁剪可见区域，不再把关卡内容重新居中。
        g.fixedAnchorRect = GetAnchorRect();
        const std::array<Vec2, 15>& layout = AnchorLayoutForLevel(index);
        for (int anchorId = 0; anchorId < static_cast<int>(g.fixedAnchors.size()); ++anchorId)
        {
            if (index == kResizeLevelIndex)
            {
                g.fixedAnchors[static_cast<size_t>(anchorId)] = ResizeLevelAnchorToClient(anchorId);
            }
            else
            {
                g.fixedAnchors[static_cast<size_t>(anchorId)] = AnchorFromRect(g.fixedAnchorRect, layout, anchorId);
            }
        }
        g.visualAnchors = g.fixedAnchors;

        if (index == kBoxLevelIndex)
        {
            const RECT board = GetBoardRect();
            const int boxSize = 170;
            const Vec2 leftDot{
                board.left + 0.24f * static_cast<float>(WidthOf(board)),
                board.top + 0.50f * static_cast<float>(HeightOf(board))
            };
            const Vec2 rightDot{
                board.left + 0.62f * static_cast<float>(WidthOf(board)),
                board.top + 0.50f * static_cast<float>(HeightOf(board))
            };

            g.boxLeftDot = leftDot;
            g.boxRightDot = rightDot;
            g.fixedAnchors[0] = leftDot;
            g.fixedAnchors[8] = rightDot;
            g.visualAnchors[0] = leftDot;
            g.visualAnchors[8] = rightDot;
            g.boxFrameRect = MakeRect(
                static_cast<int>(std::round(rightDot.x)) - boxSize / 2,
                static_cast<int>(std::round(rightDot.y)) - boxSize / 2,
                static_cast<int>(std::round(rightDot.x)) + boxSize / 2,
                static_cast<int>(std::round(rightDot.y)) + boxSize / 2);
        }
        else
        {
            g.boxLeftDot = {};
            g.boxRightDot = {};
            g.boxFrameRect = {};
        }
    }

    Vec2 LevelSelectToClient(int levelIndex)
    {
        const RECT dot = GetLevelButtonRect(levelIndex);
        return Vec2{
            dot.left + WidthOf(dot) * 0.5f,
            dot.top + HeightOf(dot) * 0.5f
        };
    }

    // 绘制时不允许路径点跑出棋盘，所以这里做边界裁剪。
    // 线条现在允许覆盖整个窗口客户区，只在最外侧保留极小留边。
    Vec2 ClampToBoard(Vec2 point)
    {
        RECT client{};
        GetClientRect(g.hwnd, &client);
        constexpr int kClientPadding = 6;
        point.x = std::clamp(point.x, static_cast<float>(client.left + kClientPadding), static_cast<float>(client.right - kClientPadding));
        point.y = std::clamp(point.y, static_cast<float>(client.top + kClientPadding), static_cast<float>(client.bottom - kClientPadding));
        return point;
    }

    // 比较距离时使用平方距离，避免频繁开方。
    float DistanceSquared(const Vec2& a, const Vec2& b)
    {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        return dx * dx + dy * dy;
    }

    bool NearlyEqual(float a, float b)
    {
        return std::fabs(a - b) <= 0.001f;
    }

    bool NearlySamePoint(const Vec2& a, const Vec2& b)
    {
        return NearlyEqual(a.x, b.x) && NearlyEqual(a.y, b.y);
    }

    // 向量叉积，用来判断三点的相对朝向，是线段相交判定的核心。
    float Cross(const Vec2& a, const Vec2& b, const Vec2& c)
    {
        return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    }

    // 判断一个点是否在线段 ab 上。
    bool OnSegment(const Vec2& a, const Vec2& b, const Vec2& point)
    {
        if (!NearlyEqual(Cross(a, b, point), 0.0f))
        {
            return false;
        }

        return point.x >= std::min(a.x, b.x) - 0.001f && point.x <= std::max(a.x, b.x) + 0.001f &&
            point.y >= std::min(a.y, b.y) - 0.001f && point.y <= std::max(a.y, b.y) + 0.001f;
    }

    // 判断两条线段是否相交。
    // 共享端点是允许的，因为它们可能只是正常接到同一个节点。
    bool SegmentsIntersectInclusive(const Vec2& firstA, const Vec2& firstB, const Vec2& secondA, const Vec2& secondB)
    {
        const float d1 = Cross(firstA, firstB, secondA);
        const float d2 = Cross(firstA, firstB, secondB);
        const float d3 = Cross(secondA, secondB, firstA);
        const float d4 = Cross(secondA, secondB, firstB);

        const bool proper = ((d1 > 0.0f && d2 < 0.0f) || (d1 < 0.0f && d2 > 0.0f)) &&
            ((d3 > 0.0f && d4 < 0.0f) || (d3 < 0.0f && d4 > 0.0f));
        if (proper)
        {
            return true;
        }

        return OnSegment(firstA, firstB, secondA) ||
            OnSegment(firstA, firstB, secondB) ||
            OnSegment(secondA, secondB, firstA) ||
            OnSegment(secondA, secondB, firstB);
    }

    bool SegmentsIntersect(const Segment& first, const Segment& second)
    {
        if (NearlySamePoint(first.a, second.a) || NearlySamePoint(first.a, second.b) ||
            NearlySamePoint(first.b, second.a) || NearlySamePoint(first.b, second.b))
        {
            return false;
        }

        const float d1 = Cross(first.a, first.b, second.a);
        const float d2 = Cross(first.a, first.b, second.b);
        const float d3 = Cross(second.a, second.b, first.a);
        const float d4 = Cross(second.a, second.b, first.b);

        const bool proper = ((d1 > 0.0f && d2 < 0.0f) || (d1 < 0.0f && d2 > 0.0f)) &&
            ((d3 > 0.0f && d4 < 0.0f) || (d3 < 0.0f && d4 > 0.0f));
        if (proper)
        {
            return true;
        }

        return OnSegment(first.a, first.b, second.a) || OnSegment(first.a, first.b, second.b) ||
            OnSegment(second.a, second.b, first.a) || OnSegment(second.a, second.b, first.b);
    }

    // 创建圆头、圆角连接的画笔，让玩家画出来的线更接近参考游戏的柔和视觉。
    bool PointInsideRect(const Vec2& point, const RECT& rect)
    {
        return point.x >= static_cast<float>(rect.left) &&
            point.x <= static_cast<float>(rect.right) &&
            point.y >= static_cast<float>(rect.top) &&
            point.y <= static_cast<float>(rect.bottom);
    }

    bool SegmentIntersectsRect(const Vec2& a, const Vec2& b, const RECT& rect)
    {
        if (PointInsideRect(a, rect) || PointInsideRect(b, rect))
        {
            return true;
        }

        const Vec2 topLeft{ static_cast<float>(rect.left), static_cast<float>(rect.top) };
        const Vec2 topRight{ static_cast<float>(rect.right), static_cast<float>(rect.top) };
        const Vec2 bottomRight{ static_cast<float>(rect.right), static_cast<float>(rect.bottom) };
        const Vec2 bottomLeft{ static_cast<float>(rect.left), static_cast<float>(rect.bottom) };

        return SegmentsIntersectInclusive(a, b, topLeft, topRight) ||
            SegmentsIntersectInclusive(a, b, topRight, bottomRight) ||
            SegmentsIntersectInclusive(a, b, bottomRight, bottomLeft) ||
            SegmentsIntersectInclusive(a, b, bottomLeft, topLeft);
    }

    RECT GetDividerWallRect()
    {
        RECT client{};
        GetClientRect(g.hwnd, &client);

        const RECT board = g.fixedAnchorRect;
        const int thickness = 10;
        const int x = board.left + WidthOf(board) / 2;
        return MakeRect(x - thickness / 2, client.top, x + thickness / 2, client.bottom);
    }

    bool SegmentHitsDividerWall(const Vec2& a, const Vec2& b)
    {
        return g.screen == ScreenMode::Playing &&
            g.levelIndex == kDividerLevelIndex &&
            SegmentIntersectsRect(a, b, GetDividerWallRect());
    }

    bool PointOutsideClient(const Vec2& point)
    {
        RECT client{};
        GetClientRect(g.hwnd, &client);
        return point.x < static_cast<float>(client.left) ||
            point.x > static_cast<float>(client.right) ||
            point.y < static_cast<float>(client.top) ||
            point.y > static_cast<float>(client.bottom);
    }

    RECT GetBoxFrameRect()
    {
        return g.boxFrameRect;
    }

    std::array<RECT, 4> GetBoxWallRects()
    {
        const RECT frame = GetBoxFrameRect();
        const int thickness = 10;
        return {{
            MakeRect(frame.left, frame.top, frame.right, frame.top + thickness),
            MakeRect(frame.right - thickness, frame.top, frame.right, frame.bottom),
            MakeRect(frame.left, frame.bottom - thickness, frame.right, frame.bottom),
            MakeRect(frame.left, frame.top, frame.left + thickness, frame.bottom),
        }};
    }

    bool SegmentHitsBoxWall(const Vec2& a, const Vec2& b)
    {
        if (g.screen != ScreenMode::Playing || g.levelIndex != kBoxLevelIndex)
        {
            return false;
        }

        RECT client{};
        GetClientRect(g.hwnd, &client);
        for (const RECT& wall : GetBoxWallRects())
        {
            // 第七关的机关核心是“窗口遮住的墙不再存在”：
            // 因此碰撞只检查当前客户区内真正可见的墙体部分。
            RECT visibleWall{};
            if (!IntersectRect(&visibleWall, &wall, &client))
            {
                continue;
            }

            if (SegmentIntersectsRect(a, b, visibleWall))
            {
                return true;
            }
        }
        return false;
    }

    bool SegmentHitsActiveWall(const Vec2& a, const Vec2& b)
    {
        return SegmentHitsDividerWall(a, b) || SegmentHitsBoxWall(a, b);
    }

    std::wstring GetExecutableFileName()
    {
        wchar_t path[MAX_PATH]{};
        GetModuleFileNameW(nullptr, path, MAX_PATH);

        const std::wstring fullPath = path;
        const size_t slash = fullPath.find_last_of(L"\\/");
        if (slash == std::wstring::npos)
        {
            return fullPath;
        }
        return fullPath.substr(slash + 1);
    }

    std::wstring GetExecutableStem()
    {
        std::wstring fileName = GetExecutableFileName();
        const size_t dot = fileName.find_last_of(L'.');
        if (dot != std::wstring::npos)
        {
            fileName.resize(dot);
        }
        return fileName;
    }

    std::wstring ToLowerCopy(std::wstring text)
    {
        for (wchar_t& ch : text)
        {
            ch = static_cast<wchar_t>(std::towlower(ch));
        }
        return text;
    }

    COLORREF BackgroundColorFromExecutableName()
    {
        const std::wstring name = ToLowerCopy(GetExecutableStem());
        if (name.find(L"blue") != std::wstring::npos)
        {
            return kBlue;
        }
        if (name.find(L"yellow") != std::wstring::npos)
        {
            return kYellow;
        }
        if (name.find(L"green") != std::wstring::npos)
        {
            return kGreen;
        }
        if (name.find(L"red") != std::wstring::npos)
        {
            return kRed;
        }
        if (name.find(L"black") != std::wstring::npos)
        {
            return kBlack;
        }
        return kWhite;
    }

    COLORREF CurrentBackgroundColor()
    {
        if (g.screen == ScreenMode::Playing && g.levelIndex == kNameColorLevelIndex)
        {
            return BackgroundColorFromExecutableName();
        }
        return kWhite;
    }

    std::wstring WindowTitleFromExecutableName()
    {
        const std::wstring stem = GetExecutableStem();
        if (stem.empty() || ToLowerCopy(stem) == L"buggame")
        {
            return kDefaultWindowTitle;
        }
        return stem;
    }

    std::wstring GetExecutableDirectory()
    {
        wchar_t path[MAX_PATH]{};
        GetModuleFileNameW(nullptr, path, MAX_PATH);

        const std::wstring fullPath = path;
        const size_t slash = fullPath.find_last_of(L"\\/");
        if (slash == std::wstring::npos)
        {
            return L".";
        }
        return fullPath.substr(0, slash);
    }

    std::wstring GetSaveFilePath()
    {
        const std::wstring directory = GetExecutableDirectory();
        if (directory.empty() || directory == L".")
        {
            return L"bug_progress.txt";
        }
        return directory + L"\\bug_progress.txt";
    }

    void SaveProgress()
    {
        FILE* file = nullptr;
        if (_wfopen_s(&file, GetSaveFilePath().c_str(), L"w") != 0 || file == nullptr)
        {
            return;
        }

        fwprintf(file, L"WINDOW_BUG_PROGRESS 1\n");
        fwprintf(file, L"unlocked %d\n", g.unlockedLevels);
        fwprintf(file, L"completed ");
        for (bool completed : g.completedLevels)
        {
            fwprintf(file, completed ? L"1" : L"0");
        }
        fwprintf(file, L"\n");
        fclose(file);
    }

    bool LoadProgress()
    {
        FILE* file = nullptr;
        if (_wfopen_s(&file, GetSaveFilePath().c_str(), L"r") != 0 || file == nullptr)
        {
            return false;
        }

        wchar_t line[256]{};
        if (fgetws(line, static_cast<int>(sizeof(line) / sizeof(line[0])), file) == nullptr)
        {
            fclose(file);
            return false;
        }

        const std::wstring header = line;
        if (header.find(L"WINDOW_BUG_PROGRESS") == std::wstring::npos)
        {
            fclose(file);
            return false;
        }

        int savedUnlocked = g.unlockedLevels;
        std::vector<bool> savedCompleted(g.completedLevels.size(), false);
        while (fgetws(line, static_cast<int>(sizeof(line) / sizeof(line[0])), file) != nullptr)
        {
            const std::wstring text = line;
            if (text.rfind(L"unlocked ", 0) == 0)
            {
                savedUnlocked = _wtoi(text.c_str() + 9);
            }
            else if (text.rfind(L"completed ", 0) == 0)
            {
                constexpr size_t kCompletedPrefixLength = 10;
                for (size_t i = 0; i < savedCompleted.size() && kCompletedPrefixLength + i < text.size(); ++i)
                {
                    savedCompleted[i] = text[kCompletedPrefixLength + i] == L'1';
                }
            }
        }
        fclose(file);

        const int levelCount = static_cast<int>(g.defs.size());
        g.completedLevels = savedCompleted;
        g.unlockedLevels = std::clamp(savedUnlocked, 1, std::max(1, levelCount));
        for (size_t i = 0; i < g.completedLevels.size(); ++i)
        {
            if (g.completedLevels[i])
            {
                g.unlockedLevels = std::max(g.unlockedLevels, std::min(levelCount, static_cast<int>(i) + 2));
            }
        }
        return true;
    }

    HPEN CreateRoundedPen(COLORREF color, int width)
    {
        LOGBRUSH brush{};
        brush.lbStyle = BS_SOLID;
        brush.lbColor = color;
        return ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND, width, &brush, 0, nullptr);
    }

    // 用纯色填充矩形。
    void FillRectColor(HDC hdc, const RECT& rect, COLORREF color)
    {
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);
    }

    // 统一的文字绘制函数。
    void DrawTextBlock(HDC hdc, const std::wstring& text, const RECT& rect, int size, int weight, UINT format)
    {
        HFONT font = CreateFontW(size, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
        HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
        SetTextColor(hdc, kBlack);
        SetBkMode(hdc, TRANSPARENT);
        RECT drawRect = rect;
        DrawTextW(hdc, text.c_str(), -1, &drawRect, format);
        SelectObject(hdc, oldFont);
        DeleteObject(font);
    }

    // 画圆点，用于线路端点与目标高亮提示。
    void DrawCircle(HDC hdc, const Vec2& center, int radius, COLORREF fill, COLORREF stroke, int strokeWidth)
    {
        HBRUSH brush = CreateSolidBrush(fill);
        HPEN pen = CreatePen(PS_SOLID, strokeWidth, stroke);
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, brush));
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
        Ellipse(hdc,
            static_cast<int>(center.x - radius),
            static_cast<int>(center.y - radius),
            static_cast<int>(center.x + radius),
            static_cast<int>(center.y + radius));
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(brush);
        DeleteObject(pen);
    }

    // 绘制圆角外框和内部窗格分割线。
    void DrawRoundedRectBlock(HDC hdc, const RECT& rect, int radius, COLORREF fill, COLORREF stroke, int strokeWidth)
    {
        HBRUSH brush = CreateSolidBrush(fill);
        HPEN pen = CreatePen(PS_SOLID, strokeWidth, stroke);
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, brush));
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
        RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(brush);
        DeleteObject(pen);
    }

    void DrawBoardFrame(HDC hdc, const RECT& board)
    {
        HPEN borderPen = CreateRoundedPen(kBlack, kPaneThickness);
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(WHITE_BRUSH)));

        RoundRect(hdc, board.left, board.top, board.right, board.bottom, kBoardCornerRadius, kBoardCornerRadius);

        const int inset = kBoardCornerRadius / 2;
        const int midX = board.left + WidthOf(board) / 2;
        const int midY = board.top + HeightOf(board) / 2;

        MoveToEx(hdc, board.left + inset, midY, nullptr);
        LineTo(hdc, board.right - inset, midY);

        MoveToEx(hdc, midX, board.top + inset, nullptr);
        LineTo(hdc, midX, board.bottom - inset);

        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(borderPen);
    }

    // 关卡配置。现在玩法改成玩家直接画线，所以每关只需要描述端点配对。
    void DrawBendStructure(HDC hdc)
    {
        const RECT board = g.fixedAnchorRect;
        const int thickness = kWallThickness;

        auto xAt = [&](float rel)
        {
            return board.left + static_cast<int>(std::round(rel * WidthOf(board)));
        };
        auto yAt = [&](float rel)
        {
            return board.top + static_cast<int>(std::round(rel * HeightOf(board)));
        };
        auto wall = [&](int left, int top, int right, int bottom)
        {
            FillRectColor(hdc, MakeRect(left, top, right, bottom), kWallGray);
        };
        auto horizontal = [&](float leftRel, float rightRel, float centerYRel)
        {
            const int y = yAt(centerYRel);
            wall(xAt(leftRel), y - thickness / 2, xAt(rightRel), y + thickness / 2);
        };
        auto vertical = [&](float centerXRel, float topRel, float bottomRel)
        {
            const int x = xAt(centerXRel);
            wall(x - thickness / 2, yAt(topRel), x + thickness / 2, yAt(bottomRel));
        };

        horizontal(0.0f, 1.0f, 0.0f);
        horizontal(0.0f, 1.0f, 1.0f);
        vertical(0.0f, 0.0f, 1.0f);
        vertical(1.0f, 0.0f, 1.0f);
        horizontal(0.0f, 0.145f, 0.195f);
        vertical(0.145f, 0.0f, 0.195f);
        vertical(0.655f, 0.0f, 0.195f);
        vertical(0.655f, 0.815f, 1.0f);
        horizontal(0.850f, 1.0f, 0.815f);
        vertical(0.850f, 0.815f, 1.0f);
    }

    void DrawGapStructure(HDC hdc)
    {
        const RECT board = g.fixedAnchorRect;
        const int thickness = kWallThickness;

        auto xAt = [&](float rel)
        {
            return board.left + static_cast<int>(std::round(rel * WidthOf(board)));
        };
        auto yAt = [&](float rel)
        {
            return board.top + static_cast<int>(std::round(rel * HeightOf(board)));
        };
        auto wall = [&](int left, int top, int right, int bottom)
        {
            FillRectColor(hdc, MakeRect(left, top, right, bottom), kWallGray);
        };
        auto horizontal = [&](float leftRel, float rightRel, float centerYRel)
        {
            const int y = yAt(centerYRel);
            wall(xAt(leftRel), y - thickness / 2, xAt(rightRel), y + thickness / 2);
        };
        auto vertical = [&](float centerXRel, float topRel, float bottomRel)
        {
            const int x = xAt(centerXRel);
            wall(x - thickness / 2, yAt(topRel), x + thickness / 2, yAt(bottomRel));
        };

        horizontal(0.0f, 1.0f, 0.0f);
        horizontal(0.0f, 1.0f, 1.0f);
        vertical(0.0f, 0.0f, 1.0f);
        vertical(1.0f, 0.0f, 1.0f);

        // 第四关参考原版的“窗格”结构：中间竖隔板保留，
        // 横隔板只画需要的几段，让圆点附近自然形成缺口。
        vertical(0.500f, 0.0f, 0.305f);
        vertical(0.500f, 0.705f, 1.0f);
        horizontal(0.0f, 0.115f, 0.500f);
        horizontal(0.145f, 0.245f, 0.500f);
        horizontal(0.755f, 1.0f, 0.500f);
    }

    void DrawDividerStructure(HDC hdc)
    {
        FillRectColor(hdc, GetDividerWallRect(), kWallGray);
    }

    void DrawBoxStructure(HDC hdc)
    {
        for (const RECT& wall : GetBoxWallRects())
        {
            FillRectColor(hdc, wall, kWallGray);
        }
    }

    std::vector<LevelDefinition> BuildLevels()
    {
        return {
            {
                L"BOOT",
                L"Hold the dot and draw to the same color.",
                {
                    { kRed, 0, 14 },
                    { kYellow, 10, 4 },
                }
            },
            {
                L"SWEEP",
                L"Three colors. Keep every path apart.",
                {
                    { kRed, 0, 9 },
                    { kYellow, 5, 14 },
                    { kBlue, 10, 4 },
                }
            },
            {
                L"BEND",
                L"Use the corners. Straight lines will collide.",
                {
                    { kRed, 0, 14 },
                    { kYellow, 7, 13 },
                    { kBlue, 3, 11 },
                }
            },
            {
                L"GAP",
                L"One color should cut through the middle. The others should not.",
                {
                    { kRed, 0, 14 },
                    { kYellow, 6, 8 },
                    { kBlue, 2, 12 },
                }
            },
            {
                L"RESIZE",
                L"The matching dots are below the window. Pull the lower border down.",
                {
                    { kYellow, 0, 10 },
                    { kBlue, 1, 11 },
                    { kRed, 2, 12 },
                }
            },
            {
                L"DIVIDE",
                L"The pane is split in two. Match each pair without crossings.",
                {
                    { kYellow, 0, 10 },
                    { kRed, 6, 8 },
                    { kBlue, 4, 14 },
                }
            },
            {
                L"BOX",
                L"The dot is boxed in. Leave the window and come back inside.",
                {
                    { kRed, 0, 8 },
                }
            },
            {
                L"WHITE",
                L"Rename A White Bug.exe to A Blue, Yellow, or Green Bug.exe.",
                {
                    { kWhite, 0, 14 },
                }
            },
        };
    }

    // 把颜色转换成文字，方便状态栏提示玩家当前正在画哪种颜色。
    std::wstring ColorName(COLORREF color)
    {
        if (color == kWhite)
        {
            return L"WHITE";
        }
        if (color == kRed)
        {
            return L"RED";
        }
        if (color == kYellow)
        {
            return L"YELLOW";
        }
        if (color == kGreen)
        {
            return L"GREEN";
        }
        return L"BLUE";
    }

    // 把玩家实际画出来的折线展开成若干线段，用于交叉检测。
    std::vector<Segment> BuildSegments()
    {
        std::vector<Segment> segments;
        for (size_t pathIndex = 0; pathIndex < g.paths.size(); ++pathIndex)
        {
            const PathState& path = g.paths[pathIndex];
            if (path.points.size() < 2)
            {
                continue;
            }

            for (size_t i = 0; i + 1 < path.points.size(); ++i)
            {
                segments.push_back({
                    g.wires[pathIndex].color,
                    path.points[i],
                    path.points[i + 1],
                    static_cast<int>(pathIndex),
                    static_cast<int>(i)
                });
            }
        }
        return segments;
    }

    // 仅用于显示的平滑路径。
    // 检测仍然使用原始折线，保证判定规则稳定；渲染时再把折线做圆滑处理。
    std::vector<Vec2> BuildDisplayPath(const PathState& path, bool appendCursor)
    {
        std::vector<Vec2> points = path.points;
        if (appendCursor && !points.empty() && !NearlySamePoint(points.back(), g.activeCursor))
        {
            points.push_back(g.activeCursor);
        }

        if (points.size() < 3)
        {
            return points;
        }

        // 使用两轮 Chaikin corner cutting，让折线更接近手绘曲线。
        for (int iteration = 0; iteration < 2; ++iteration)
        {
            if (points.size() < 3)
            {
                break;
            }

            std::vector<Vec2> smooth;
            smooth.reserve(points.size() * 2);
            smooth.push_back(points.front());

            for (size_t i = 0; i + 1 < points.size(); ++i)
            {
                const Vec2 a = points[i];
                const Vec2 b = points[i + 1];
                smooth.push_back({
                    a.x * 0.75f + b.x * 0.25f,
                    a.y * 0.75f + b.y * 0.25f
                });
                smooth.push_back({
                    a.x * 0.25f + b.x * 0.75f,
                    a.y * 0.25f + b.y * 0.75f
                });
            }

            smooth.push_back(points.back());
            points.swap(smooth);
        }

        return points;
    }

    // 当窗口大小变化时，棋盘仍然保持固定尺寸并居中。
    // 因此只需要把已有路径整体平移到新的棋盘中心位置即可。
    void ShiftPaths(float dx, float dy)
    {
        if (NearlyEqual(dx, 0.0f) && NearlyEqual(dy, 0.0f))
        {
            return;
        }

        for (PathState& path : g.paths)
        {
            for (Vec2& point : path.points)
            {
                point.x += dx;
                point.y += dy;
            }
        }

        if (g.drawing)
        {
            g.activeCursor.x += dx;
            g.activeCursor.y += dy;
        }
    }

    bool IsLevelUnlocked(int index)
    {
        if (index < 0 || index >= static_cast<int>(g.defs.size()))
        {
            return false;
        }

        if (kDeveloperUnlockAllLevels)
        {
            return true;
        }

        return index < g.unlockedLevels;
    }

    void OpenLevelSelect(int focusIndex)
    {
        if (g.defs.empty())
        {
            return;
        }

        g.screen = ScreenMode::LevelSelect;
        g.levelIndex = std::clamp(focusIndex, 0, static_cast<int>(g.defs.size()) - 1);
        g.hoveredLevel = -1;
        g.pendingReturnToSelect = false;
        g.cleared = false;
        g.drawing = false;
        g.activeWire = -1;
        ReleaseCapture();
        g.lastBoardRect = GetAnchorRect();
    }

    // 每次绘制或鼠标移动后，实时重新计算交叉数和通关状态。
    void RefreshState()
    {
        if (g.screen != ScreenMode::Playing)
        {
            g.crossings = 0;
            g.cleared = false;
            return;
        }

        const std::vector<Segment> segments = BuildSegments();
        g.crossings = 0;

        for (size_t i = 0; i < segments.size(); ++i)
        {
            for (size_t j = i + 1; j < segments.size(); ++j)
            {
                // 现在只把“不同颜色”的相交当成错误。
                // 同色线段即使互相覆盖或回绕，也不记为 crossing。
                if (segments[i].color == segments[j].color)
                {
                    continue;
                }

                // 同一路径里相邻的线段共享顶点，这种情况不算交叉。
                if (segments[i].pathIndex == segments[j].pathIndex &&
                    std::abs(segments[i].segmentIndex - segments[j].segmentIndex) <= 1)
                {
                    continue;
                }

                if (SegmentsIntersect(segments[i], segments[j]))
                {
                    ++g.crossings;
                }
            }
        }

        if (g.levelIndex == kDividerLevelIndex)
        {
            for (const Segment& segment : segments)
            {
                if (SegmentHitsDividerWall(segment.a, segment.b))
                {
                    ++g.crossings;
                }
            }
        }
        else if (g.levelIndex == kBoxLevelIndex)
        {
            for (const Segment& segment : segments)
            {
                if (SegmentHitsBoxWall(segment.a, segment.b))
                {
                    ++g.crossings;
                }
            }
        }

        bool allConnected = !g.paths.empty();
        for (const PathState& path : g.paths)
        {
            if (!path.connected)
            {
                allConnected = false;
                break;
            }
        }

        const bool wasCleared = g.cleared;
        g.cleared = (!g.drawing && allConnected && g.crossings == 0);
        if (g.cleared && !wasCleared)
        {
            g.completedLevels[static_cast<size_t>(g.levelIndex)] = true;
            g.unlockedLevels = std::min(static_cast<int>(g.defs.size()), std::max(g.unlockedLevels, g.levelIndex + 2));
            SaveProgress();
            g.pendingReturnToSelect = true;
            g.clearedAtTick = GetTickCount();
        }

        if (g.cleared)
        {
            g.status = L"stable";
        }
        else if (g.drawing && g.activeWire >= 0)
        {
            const WireDefinition& wire = g.wires[g.activeWire];
            g.status = L"Drawing " + ColorName(wire.color) + L". Release on the matching dot.";
        }
        else
        {
            g.status = g.hint + L"  crossings: " + std::to_wstring(g.crossings);
        }
    }

    // 根据关卡编号，把静态配置装载成运行时状态。
    void LoadLevel(int index)
    {
        const LevelDefinition& def = g.defs[static_cast<size_t>(index)];
        g.screen = ScreenMode::Playing;
        g.levelIndex = index;
        g.levelName = def.name;
        g.hint = def.hint;
        g.wires = def.wires;
        g.paths.assign(g.wires.size(), {});
        g.hoveredLevel = -1;
        g.activeWire = -1;
        g.activeFromStart = true;
        g.drawing = false;
        g.cleared = false;
        g.pendingReturnToSelect = false;
        g.clearedAtTick = 0;
        g.activeCursor = {};
        if (index == kResizeLevelIndex)
        {
            RECT client{};
            GetClientRect(g.hwnd, &client);
            const int clientHeight = std::max(1, HeightOf(client));
            g.resizeLevelTopY = std::max(170, static_cast<int>(clientHeight * 0.44f));
            g.resizeLevelHiddenY = clientHeight + 92;
        }
        else
        {
            g.resizeLevelTopY = 0;
            g.resizeLevelHiddenY = 0;
        }

        CaptureLevelGeometry(index);
        g.lastBoardRect = g.fixedAnchorRect;
        RefreshState();
    }

    // 重开当前关卡。
    void RestartLevel()
    {
        LoadLevel(g.levelIndex);
    }

    // 进入下一关；如果已经是最后一关，就从第一关重新开始。
    void NextLevel()
    {
        if (g.screen == ScreenMode::LevelSelect)
        {
            if (IsLevelUnlocked(g.levelIndex))
            {
                LoadLevel(g.levelIndex);
            }
            return;
        }

        if (!g.cleared)
        {
            return;
        }

        OpenLevelSelect(g.levelIndex);
    }

    bool FindLevelButtonHit(POINT point, int& levelIndex)
    {
        for (int i = 0; i < static_cast<int>(g.defs.size()); ++i)
        {
            if (!IsLevelUnlocked(i))
            {
                continue;
            }

            const RECT button = GetLevelButtonRect(i);
            if (PtInRect(&button, point))
            {
                levelIndex = i;
                return true;
            }
        }

        return false;
    }

    // 判断鼠标是否点中了某条线路的某个端点。
    bool FindEndpointHit(POINT point, int& wireIndex, bool& startsAtStartAnchor)
    {
        const Vec2 cursor{ static_cast<float>(point.x), static_cast<float>(point.y) };
        const float hitRadius = static_cast<float>(kEndpointRadius + 8);
        const float hitRadius2 = hitRadius * hitRadius;

        for (size_t i = 0; i < g.wires.size(); ++i)
        {
            const Vec2 start = AnchorToClient(g.wires[i].startAnchor);
            const Vec2 end = AnchorToClient(g.wires[i].endAnchor);
            if (DistanceSquared(cursor, start) <= hitRadius2)
            {
                wireIndex = static_cast<int>(i);
                startsAtStartAnchor = true;
                return true;
            }
            if (DistanceSquared(cursor, end) <= hitRadius2)
            {
                wireIndex = static_cast<int>(i);
                startsAtStartAnchor = false;
                return true;
            }
        }

        return false;
    }

    // 开始画某条线。玩家每次重新按下端点，都会覆盖该颜色已有的旧路径。
    void BeginDraw(int wireIndex, bool startsAtStartAnchor)
    {
        g.pendingReturnToSelect = false;
        g.cleared = false;
        g.activeWire = wireIndex;
        g.activeFromStart = startsAtStartAnchor;
        g.drawing = true;

        PathState& path = g.paths[wireIndex];
        path.points.clear();
        path.connected = false;
        path.startsAtStartAnchor = startsAtStartAnchor;

        const int anchor = startsAtStartAnchor ? g.wires[wireIndex].startAnchor : g.wires[wireIndex].endAnchor;
        path.points.push_back(AnchorToClient(anchor));
        g.activeCursor = path.points.back();

        SetCapture(g.hwnd);
        RefreshState();
        InvalidateRect(g.hwnd, nullptr, FALSE);
    }

    // 绘制过程中不断追加路径点，形成玩家自己画出的折线。
    void AppendActivePoint(Vec2 point)
    {
        if (!g.drawing || g.activeWire < 0)
        {
            return;
        }

        PathState& path = g.paths[g.activeWire];
        if (g.levelIndex == kDividerLevelIndex || g.levelIndex == kBoxLevelIndex)
        {
            RECT client{};
            GetClientRect(g.hwnd, &client);

            // 第六关允许玩家从窗口下方绕过无限隔板；
            // 第七关允许玩家从被窗口遮住的方框外侧绕回目标点。
            constexpr int kClientPadding = 6;
            constexpr int kOutsideBottomRoom = 520;
            constexpr int kOutsideSideRoom = 520;
            if (g.levelIndex == kBoxLevelIndex)
            {
                point.x = std::clamp(point.x, static_cast<float>(client.left - kOutsideSideRoom), static_cast<float>(client.right + kOutsideSideRoom));
                point.y = std::clamp(point.y, static_cast<float>(client.top - kOutsideSideRoom), static_cast<float>(client.bottom + kOutsideSideRoom));
            }
            else
            {
                point.x = std::clamp(point.x, static_cast<float>(client.left + kClientPadding), static_cast<float>(client.right - kClientPadding));
                point.y = std::clamp(point.y, static_cast<float>(client.top + kClientPadding), static_cast<float>(client.bottom + kOutsideBottomRoom));
            }
        }
        else
        {
            point = ClampToBoard(point);
        }
        g.activeCursor = point;

        if (path.points.empty())
        {
            path.points.push_back(point);
            return;
        }

        const Vec2 start = path.points.back();
        const float distance2 = DistanceSquared(start, point);
        if (distance2 < 4.0f)
        {
            return;
        }

        // 把一次较长的鼠标移动拆成多个小样本点，避免曲线被压扁成一条直线。
        if (g.levelIndex == kBoxLevelIndex && PointOutsideClient(start) && !PointOutsideClient(point))
        {
            // 从窗口外重新进入时，不插入中间采样点，形成“从窗口背后绕进来”的效果。
            // 但如果这条回来的线穿过了仍然可见的墙，就说明玩家是在硬闯，必须拦住。
            if (!SegmentHitsBoxWall(start, point))
            {
                path.points.push_back(point);
            }
            else
            {
                g.activeCursor = path.points.back();
            }
            return;
        }

        const float distance = std::sqrt(distance2);
        const int steps = std::max(1, static_cast<int>(std::ceil(distance / kDrawPointStep)));
        for (int i = 1; i <= steps; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            Vec2 sample{
                start.x + (point.x - start.x) * t,
                start.y + (point.y - start.y) * t
            };

            if (SegmentHitsActiveWall(path.points.back(), sample))
            {
                g.activeCursor = path.points.back();
                break;
            }

            if (DistanceSquared(path.points.back(), sample) >= 4.0f)
            {
                path.points.push_back(sample);
            }
        }
    }

    // 鼠标移动时实时更新当前正在画的线。
    void UpdateDraw(POINT point)
    {
        if (!g.drawing || g.activeWire < 0)
        {
            return;
        }

        AppendActivePoint(Vec2{ static_cast<float>(point.x), static_cast<float>(point.y) });

        // Dragging only needs to extend the visible line.
        // We defer the expensive crossing / clear check until the player releases the mouse.
        InvalidateRect(g.hwnd, nullptr, FALSE);
    }

    // 鼠标松开时，只有落在正确的同色端点上才算连接成功，否则这次绘制作废。
    void EndDraw(POINT point)
    {
        if (!g.drawing || g.activeWire < 0)
        {
            return;
        }

        PathState& path = g.paths[g.activeWire];
        const WireDefinition& wire = g.wires[g.activeWire];
        const int targetAnchor = g.activeFromStart ? wire.endAnchor : wire.startAnchor;
        const Vec2 target = AnchorToClient(targetAnchor);
        const Vec2 cursor{ static_cast<float>(point.x), static_cast<float>(point.y) };
        AppendActivePoint(cursor);

        const bool canReachTargetWithoutWall = !path.points.empty() && !SegmentHitsActiveWall(path.points.back(), target);
        if (canReachTargetWithoutWall &&
            DistanceSquared(cursor, target) <= static_cast<float>((kEndpointRadius + 10) * (kEndpointRadius + 10)))
        {
            if (path.points.size() < 2 || !NearlySamePoint(path.points.back(), target))
            {
                path.points.push_back(target);
            }
            else
            {
                path.points.back() = target;
            }
            path.connected = true;
        }
        else
        {
            path.points.clear();
            path.connected = false;
        }

        g.drawing = false;
        g.activeWire = -1;
        ReleaseCapture();
        RefreshState();
        InvalidateRect(g.hwnd, nullptr, FALSE);
    }

    // 如果鼠标捕获意外丢失，就安全地取消当前绘制。
    void CancelDraw()
    {
        if (!g.drawing || g.activeWire < 0)
        {
            return;
        }

        PathState& path = g.paths[g.activeWire];
        if (!path.connected)
        {
            path.points.clear();
        }

        g.drawing = false;
        g.activeWire = -1;
        RefreshState();
        InvalidateRect(g.hwnd, nullptr, FALSE);
    }

    void UpdateHoverLevel(POINT point)
    {
        if (g.screen != ScreenMode::LevelSelect)
        {
            return;
        }

        int levelIndex = -1;
        if (FindLevelButtonHit(point, levelIndex))
        {
            g.hoveredLevel = levelIndex;
            g.levelIndex = levelIndex;
        }
        else
        {
            g.hoveredLevel = -1;
        }
    }

    void UpdateAutoReturn()
    {
        if (g.screen != ScreenMode::Playing || !g.pendingReturnToSelect)
        {
            return;
        }

        if (GetTickCount() - g.clearedAtTick < kClearReturnDelayMs)
        {
            return;
        }

        OpenLevelSelect(g.levelIndex);
        InvalidateRect(g.hwnd, nullptr, FALSE);
    }

    // 负责绘制整帧画面。
    void DrawScene(HDC hdc, const RECT& clientRect)
    {
        FillRectColor(hdc, clientRect, CurrentBackgroundColor());

        if (g.screen == ScreenMode::LevelSelect)
        {
            for (int i = 0; i < kLevelSlotCount; ++i)
            {
                const RECT tile = GetLevelButtonRect(i);
                RECT shadow = tile;
                OffsetRect(&shadow, 0, kLevelTileShadowOffset);

                const bool hasLevel = i < static_cast<int>(g.defs.size());
                const bool unlocked = hasLevel && IsLevelUnlocked(i);
                const bool completed = hasLevel && i < static_cast<int>(g.completedLevels.size()) &&
                    g.completedLevels[static_cast<size_t>(i)];
                const bool focused = hasLevel && (i == g.levelIndex);
                const bool hovered = hasLevel && (i == g.hoveredLevel);

                COLORREF fill = RGB(247, 247, 247);
                if (completed)
                {
                    fill = RGB(232, 232, 232);
                }
                else if (unlocked)
                {
                    fill = RGB(241, 241, 241);
                }

                if (unlocked && (focused || hovered))
                {
                    fill = RGB(188, 188, 188);
                }

                DrawRoundedRectBlock(hdc, shadow, kLevelTileCornerRadius, RGB(250, 250, 250), RGB(250, 250, 250), 1);
                DrawRoundedRectBlock(hdc, tile, kLevelTileCornerRadius, fill, fill, 1);
            }
            return;
        }


        // 玩家正在画某条线时，把目标端点再用同色扩大显示，降低操作门槛。

        // 路径会先做一次显示平滑，所以视觉上比原始折线更接近曲线。
        if (g.levelIndex == kBendLevelIndex)
        {
            DrawBendStructure(hdc);
        }
        else if (g.levelIndex == kGapLevelIndex)
        {
            DrawGapStructure(hdc);
        }
        else if (g.levelIndex == kDividerLevelIndex)
        {
            DrawDividerStructure(hdc);
        }
        else if (g.levelIndex == kBoxLevelIndex)
        {
            DrawBoxStructure(hdc);
        }

        for (size_t i = 0; i < g.paths.size(); ++i)
        {
            const PathState& path = g.paths[i];
            if (path.points.size() < 2)
            {
                continue;
            }

            const bool appendCursor = g.drawing && static_cast<int>(i) == g.activeWire;
            const std::vector<Vec2> displayPath = BuildDisplayPath(path, appendCursor);
            if (displayPath.size() < 2)
            {
                continue;
            }

            HPEN pen = CreateRoundedPen(g.wires[i].color, kWireThickness);
            HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));

            MoveToEx(hdc, static_cast<int>(displayPath[0].x), static_cast<int>(displayPath[0].y), nullptr);
            for (size_t p = 1; p < displayPath.size(); ++p)
            {
                LineTo(hdc, static_cast<int>(displayPath[p].x), static_cast<int>(displayPath[p].y));
            }

            SelectObject(hdc, oldPen);
            DeleteObject(pen);
        }

        // 如果玩家正在画线，给当前鼠标经过的末端加一个同色小点。
        if (g.drawing && g.activeWire >= 0)
        {
            const COLORREF activeColor = g.wires[g.activeWire].color;
            DrawCircle(hdc, g.activeCursor, kAnchorHintRadius, activeColor, activeColor, 1);
        }

        // 绘制所有彩色端点。现在端点比旧版更大，也更接近参考作品的视觉重点。
        for (const WireDefinition& wire : g.wires)
        {
            DrawCircle(hdc, EndpointVisualToClient(wire.startAnchor), kEndpointRadius, wire.color, wire.color, 1);
            DrawCircle(hdc, EndpointVisualToClient(wire.endAnchor), kEndpointRadius, wire.color, wire.color, 1);
        }

    }

    // 双缓冲绘制：先画到内存位图，再一次性拷贝到窗口，减少闪烁。
    void Render(HWND hwnd)
    {
        PAINTSTRUCT ps{};
        HDC windowDc = BeginPaint(hwnd, &ps);

        RECT client{};
        GetClientRect(hwnd, &client);

        HDC memoryDc = CreateCompatibleDC(windowDc);
        HBITMAP bitmap = CreateCompatibleBitmap(windowDc, WidthOf(client), HeightOf(client));
        HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memoryDc, bitmap));

        DrawScene(memoryDc, client);
        BitBlt(windowDc, 0, 0, WidthOf(client), HeightOf(client), memoryDc, 0, 0, SRCCOPY);

        SelectObject(memoryDc, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(memoryDc);
        EndPaint(hwnd, &ps);
    }
}

// Windows 程序的消息处理函数。
LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        // 窗口刚创建时初始化整个游戏。
        g.hwnd = hwnd;
        g.defs = BuildLevels();
        g.completedLevels.assign(g.defs.size(), false);
        g.unlockedLevels = kDeveloperUnlockAllLevels ? static_cast<int>(g.defs.size()) : 1;
        LoadProgress();
        const std::wstring commandLine = GetCommandLineW();
        if (commandLine.find(L"--level=3") != std::wstring::npos)
        {
            g.unlockedLevels = static_cast<int>(g.defs.size());
            LoadLevel(kBendLevelIndex);
        }
        else if (commandLine.find(L"--level=4") != std::wstring::npos)
        {
            g.unlockedLevels = static_cast<int>(g.defs.size());
            LoadLevel(kGapLevelIndex);
        }
        else if (commandLine.find(L"--level=5") != std::wstring::npos)
        {
            g.unlockedLevels = static_cast<int>(g.defs.size());
            LoadLevel(kResizeLevelIndex);
        }
        else if (commandLine.find(L"--level=6") != std::wstring::npos)
        {
            g.unlockedLevels = static_cast<int>(g.defs.size());
            LoadLevel(kDividerLevelIndex);
        }
        else if (commandLine.find(L"--level=7") != std::wstring::npos)
        {
            g.unlockedLevels = static_cast<int>(g.defs.size());
            LoadLevel(kBoxLevelIndex);
        }
        else if (commandLine.find(L"--level=8") != std::wstring::npos)
        {
            g.unlockedLevels = static_cast<int>(g.defs.size());
            LoadLevel(kNameColorLevelIndex);
        }
        else
        {
            LoadLevel(0);
            OpenLevelSelect(0);
        }
        // 用较高频率重绘，提高画线时的顺滑感。
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
            UpdateDraw(point);
        }
        return 0;
    }

    case WM_LBUTTONUP:
    {
        // 鼠标松开：尝试把当前线闭合到正确的同色端点。
        POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (g.screen == ScreenMode::Playing)
        {
            EndDraw(point);
        }
        return 0;
    }

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
        return 0;

    case WM_PAINT:
        Render(hwnd);
        return 0;

    case WM_TIMER:
        if (wParam == kTimerId)
        {
            UpdateAutoReturn();
            return 0;
        }
        break;

    case WM_ERASEBKGND:
        // 返回 1 告诉系统“背景我自己已经处理了”，配合双缓冲减少闪烁。
        return 1;

    case WM_DESTROY:
        SaveProgress();
        KillTimer(hwnd, kTimerId);
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
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = kWindowClassName;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
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
        return 0;
    }

    ShowWindow(hwnd, showCommand);
    UpdateWindow(hwnd);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0))
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}
