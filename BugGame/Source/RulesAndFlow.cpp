#include "Game.h"

namespace BugGame
{
    // RulesAndFlow.cpp 负责“规则”和“流程”。
    // 鼠标模块只负责收集玩家画出的路径；这里负责判断它们是否合法、是否通关、
    // 是否需要淡出非法线，以及通关后如何回到选关界面。

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

    bool SegmentHasIllegalContact(const Segment& candidate)
    {
        // 玩家拖动时每新增一小段线，就用这个函数即时检查。
        // 如果这一小段已经穿墙或撞到别的线，就立刻触发淡出，而不是等松开鼠标。
        if (SegmentHitsActiveWall(candidate.a, candidate.b))
        {
            return true;
        }

        const std::vector<Segment> segments = BuildSegments();
        for (const Segment& segment : segments)
        {
            if (segment.pathIndex == candidate.pathIndex &&
                std::abs(segment.segmentIndex - candidate.segmentIndex) <= 1)
            {
                continue;
            }

            if (SegmentsIntersect(candidate, segment))
            {
                return true;
            }
        }

        return false;
    }

    bool PathHasIllegalContact(int pathIndex)
    {
        // 鼠标松开并尝试连接终点后，再检查整条路径是否存在非法接触。
        // 这样可以防止“最后一下自动吸附到终点”时穿过墙体。
        if (pathIndex < 0 || pathIndex >= static_cast<int>(g.paths.size()))
        {
            return false;
        }

        const std::vector<Segment> segments = BuildSegments();
        for (const Segment& segment : segments)
        {
            if (segment.pathIndex == pathIndex && SegmentHitsActiveWall(segment.a, segment.b))
            {
                return true;
            }
        }

        for (size_t i = 0; i < segments.size(); ++i)
        {
            for (size_t j = i + 1; j < segments.size(); ++j)
            {
                if (segments[i].pathIndex != pathIndex && segments[j].pathIndex != pathIndex)
                {
                    continue;
                }

                if (segments[i].pathIndex == segments[j].pathIndex &&
                    std::abs(segments[i].segmentIndex - segments[j].segmentIndex) <= 1)
                {
                    continue;
                }

                if (SegmentsIntersect(segments[i], segments[j]))
                {
                    return true;
                }
            }
        }

        return false;
    }

    void AddFadingPath(const PathState& path, COLORREF color)
    {
        // 把非法路径复制到 fadingPaths，而不是直接丢掉。
        // RenderScene 每帧绘制 fadingPaths，形成“错误操作被系统抹掉”的视觉反馈。
        if (path.points.size() < 2)
        {
            return;
        }

        g.fadingPaths.push_back(FadingPath{ path.points, color, GetTickCount() });
        if (g.fadingPaths.size() > 10)
        {
            g.fadingPaths.erase(g.fadingPaths.begin());
        }
    }

    void RejectActivePathWithFade()
    {
        if (!g.drawing || g.activeWire < 0 || g.activeWire >= static_cast<int>(g.paths.size()))
        {
            return;
        }

        PathState& path = g.paths[static_cast<size_t>(g.activeWire)];
        AddFadingPath(path, g.wires[static_cast<size_t>(g.activeWire)].color);
        path.points.clear();
        path.connected = false;

        g.drawing = false;
        g.activeWire = -1;
        ReleaseCapture();
        RefreshState();
        InvalidateRect(g.hwnd, nullptr, FALSE);
    }

    bool UpdateFadingPaths()
    {
        // 定时器每帧调用，清理已经淡出结束的旧路径。
        // 返回 true 表示画面发生变化，需要重绘。
        if (g.fadingPaths.empty())
        {
            return false;
        }

        const DWORD now = GetTickCount();
        g.fadingPaths.erase(
            std::remove_if(g.fadingPaths.begin(), g.fadingPaths.end(),
                [&](const FadingPath& fadingPath)
                {
                    return now - fadingPath.startedAtTick >= kInvalidFadeMs;
                }),
            g.fadingPaths.end());
        return true;
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
        // 正式模式下只能进入已经解锁的关卡。
        // 开发调试时 kDeveloperUnlockAllLevels 可以绕过这个限制。
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
        // 回到选关界面时要清理绘制状态、鼠标捕获和淡出路径。
        // focusIndex 用来保持当前关卡方块处于焦点位置，视觉上不会跳。
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
        g.fadingPaths.clear();
        ReleaseCapture();
        g.lastBoardRect = GetAnchorRect();
    }

    // 每次绘制或鼠标移动后，实时重新计算交叉数和通关状态。
    void RefreshState()
    {
        // 这是普通关卡的核心判定函数。
        // 它会重新计算交叉/穿墙数量，确认所有线是否都已经连接到正确端点。
        if (g.screen != ScreenMode::Playing)
        {
            g.crossings = 0;
            g.cleared = false;
            return;
        }

        if (IsEraserLevel())
        {
            // 第九关使用涂白覆盖率判定，不走普通“所有线都连通”的规则。
            g.crossings = 0;
            FinishEraserLevelIfSolved();
            if (!g.cleared)
            {
                g.status = L"Paint the last colored bug white.";
            }
            return;
        }

        const std::vector<Segment> segments = BuildSegments();
        g.crossings = 0;

        for (size_t i = 0; i < segments.size(); ++i)
        {
            for (size_t j = i + 1; j < segments.size(); ++j)
            {
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

        for (const Segment& segment : segments)
        {
            // 墙体碰撞也计入 crossings。
            // 名字虽然叫 crossings，但在界面逻辑里它代表“当前非法接触数量”。
            if (SegmentHitsActiveWall(segment.a, segment.b))
            {
                ++g.crossings;
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
            // 第一次通关时保存进度、解锁下一关，并设置稍后自动回选关。
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
            g.status = L"crossings: " + std::to_wstring(g.crossings);
        }
    }

    // 根据关卡编号，把静态配置装载成运行时状态。
    void LoadLevel(int index)
    {
        // 把静态关卡配置复制到运行时状态。
        // 每次进入关卡都重新创建 paths，这样重开关卡不会保留旧线。
        const LevelDefinition& def = g.defs[static_cast<size_t>(index)];
        g.screen = ScreenMode::Playing;
        g.levelIndex = index;
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
        g.fadingPaths.clear();
        ResetEraserState();
        if (index == kResizeLevelIndex)
        {
            // 第五关在进入时记录当前窗口高度。
            // 隐藏点会放在初始窗口底部之外，玩家必须拖大窗口才看得到。
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
        if (index == kEraserLevelIndex)
        {
            RECT client{};
            GetClientRect(g.hwnd, &client);
            g.eraserCursor = Vec2{
                client.left + WidthOf(client) * 0.5f,
                client.top + HeightOf(client) * 0.82f
            };
        }
        g.lastBoardRect = g.fixedAnchorRect;
        RefreshState();
    }

    // 重开当前关卡。
    void RestartLevel()
    {
        LoadLevel(g.levelIndex);
    }

    // 确认当前关已经通过后，回到选关界面。
    // 真正的“下一关解锁”在 RefreshState 里完成，玩家从选关界面进入下一关。
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
        // 选关界面点击检测。
        // 只有已解锁的方块才会返回 true，保证正式流程按顺序推进。
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

}
