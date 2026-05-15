#include "Game.h"

namespace BugGame
{
    // Input.cpp 负责把鼠标操作转成游戏里的路径数据。
    // 这个文件不直接画图，也不决定最终通关；它只记录玩家如何拖动，
    // 然后把路径交给 RulesAndFlow.cpp 和 GeometryAndCollision.cpp 去判定。

    void ResetEraserState()
    {
        // 第九关每次进入/重开时清空所有白色涂抹轨迹。
        g.erasing = false;
        g.eraserCursor = {};
        g.eraserStart = {};
        g.erasedCircles.clear();
        g.erasedStroke.clear();
        g.activeEraserStroke.clear();
        g.eraserPaintStrokes.clear();
    }

    void BeginEraserAction(POINT point)
    {
        // 第九关按下鼠标后开始记录白色画笔轨迹。
        // SetCapture 可以让鼠标拖出窗口一点点时仍然继续收到移动事件。
        if (!IsEraserLevel() || g.cleared)
        {
            return;
        }

        g.pendingReturnToSelect = false;
        g.erasing = true;
        g.eraserCursor = Vec2{ static_cast<float>(point.x), static_cast<float>(point.y) };
        g.eraserStart = g.eraserCursor;
        g.activeEraserStroke.clear();
        g.activeEraserStroke.push_back(g.eraserCursor);

        SetCapture(g.hwnd);
        InvalidateRect(g.hwnd, nullptr, FALSE);
    }

    void UpdateEraserAction(POINT point)
    {
        // 鼠标移动时记录白色轨迹点。
        // 只有移动距离达到 kDrawPointStep 才追加，避免存入过多重复点。
        if (!IsEraserLevel() || g.cleared)
        {
            return;
        }

        g.eraserCursor = Vec2{ static_cast<float>(point.x), static_cast<float>(point.y) };
        if (g.erasing &&
            (g.activeEraserStroke.empty() || DistanceSquared(g.activeEraserStroke.back(), g.eraserCursor) >= kDrawPointStep * kDrawPointStep))
        {
            g.activeEraserStroke.push_back(g.eraserCursor);
        }
        InvalidateRect(g.hwnd, nullptr, FALSE);
    }

    void EndEraserAction(POINT point)
    {
        // 松开鼠标后，把当前白色笔画保存到 eraserPaintStrokes。
        // 然后立刻检查彩色 Bug 点是否已经被涂白到足够比例。
        if (!IsEraserLevel() || g.cleared)
        {
            return;
        }

        g.eraserCursor = Vec2{ static_cast<float>(point.x), static_cast<float>(point.y) };
        if (g.erasing)
        {
            if (g.activeEraserStroke.empty() || !NearlySamePoint(g.activeEraserStroke.back(), g.eraserCursor))
            {
                g.activeEraserStroke.push_back(g.eraserCursor);
            }

            if (StrokeLength(g.activeEraserStroke) >= 6.0f)
            {
                g.eraserPaintStrokes.push_back(g.activeEraserStroke);
                if (g.eraserPaintStrokes.size() > 32)
                {
                    g.eraserPaintStrokes.erase(g.eraserPaintStrokes.begin());
                }
            }

            FinishEraserLevelIfSolved();
        }

        g.erasing = false;
        g.activeEraserStroke.clear();
        ReleaseCapture();
        InvalidateRect(g.hwnd, nullptr, FALSE);
    }

    void CancelEraserAction()
    {
        if (!IsEraserLevel())
        {
            return;
        }

        g.erasing = false;
        g.activeEraserStroke.clear();
        InvalidateRect(g.hwnd, nullptr, FALSE);
    }

    // 判断鼠标是否点中了某条线路的某个端点。
    bool FindEndpointHit(POINT point, int& wireIndex, bool& startsAtStartAnchor)
    {
        // 点击端点时允许一点误差，不要求鼠标精确点在圆心。
        // startsAtStartAnchor 用来记录玩家是从线的哪一端开始画。
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
        // 每条颜色线同一时间只保留一条路径。
        // 重新从端点开始画，会清空这条线之前的失败/旧路线。
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
        // 这是“画线顺不顺滑”的核心函数。
        // 它会把鼠标当前位置追加到当前路径里，同时处理特殊关卡的窗口外绕线规则。
        if (!g.drawing || g.activeWire < 0)
        {
            return;
        }

        PathState& path = g.paths[g.activeWire];
        const int activeWire = g.activeWire;
        if (g.levelIndex == kDividerLevelIndex || g.levelIndex == kBoxLevelIndex)
        {
            // 第六、七关允许玩家把线画到窗口外侧。
            // 这不是普通“越界”，而是关卡机制：从窗口外绕过仍然存在的隔板/盒子。
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
                Segment candidate{
                    g.wires[static_cast<size_t>(activeWire)].color,
                    path.points.back(),
                    point,
                    activeWire,
                    static_cast<int>(path.points.size()) - 1
                };
                path.points.push_back(point);
                if (SegmentHasIllegalContact(candidate))
                {
                    RejectActivePathWithFade();
                }
            }
            else
            {
                path.points.push_back(point);
                RejectActivePathWithFade();
            }
            return;
        }

        const float distance = std::sqrt(distance2);
        const int steps = std::max(1, static_cast<int>(std::ceil(distance / kDrawPointStep)));
        // 如果鼠标一帧移动很远，直接连线会变成又硬又直的一段。
        // 这里按 kDrawPointStep 插入中间采样点，线条就能跟上手绘曲线。
        for (int i = 1; i <= steps; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            Vec2 sample{
                start.x + (point.x - start.x) * t,
                start.y + (point.y - start.y) * t
            };

            if (DistanceSquared(path.points.back(), sample) >= 4.0f)
            {
                Segment candidate{
                    g.wires[static_cast<size_t>(activeWire)].color,
                    path.points.back(),
                    sample,
                    activeWire,
                    static_cast<int>(path.points.size()) - 1
                };
                path.points.push_back(sample);
                if (SegmentHasIllegalContact(candidate))
                {
                    RejectActivePathWithFade();
                    return;
                }
            }
        }
    }

    // 鼠标移动时实时更新当前正在画的线。
    void UpdateDraw(POINT point)
    {
        // 拖动过程中只做必要的路径追加和局部非法检测。
        // 完整通关判断放到 EndDraw，避免每一帧都做过重计算导致卡顿。
        if (!g.drawing || g.activeWire < 0)
        {
            return;
        }

        AppendActivePoint(Vec2{ static_cast<float>(point.x), static_cast<float>(point.y) });

        // 拖动时只需要延长可见线条。
        // 较重的整体 crossing / clear 检查延后到松开鼠标时做，减少卡顿。
        InvalidateRect(g.hwnd, nullptr, FALSE);
    }

    // 鼠标松开时，只有落在正确的同色端点上才算连接成功，否则这次绘制作废。
    void EndDraw(POINT point)
    {
        // 松开鼠标时会尝试“吸附”到目标端点。
        // 只有靠近正确的同色端点，并且最后一段没有穿墙，才算 connected。
        if (!g.drawing || g.activeWire < 0)
        {
            return;
        }

        const int wireIndex = g.activeWire;
        PathState& path = g.paths[static_cast<size_t>(wireIndex)];
        const WireDefinition& wire = g.wires[static_cast<size_t>(wireIndex)];
        const int targetAnchor = g.activeFromStart ? wire.endAnchor : wire.startAnchor;
        const Vec2 target = AnchorToClient(targetAnchor);
        const Vec2 cursor{ static_cast<float>(point.x), static_cast<float>(point.y) };
        AppendActivePoint(cursor);
        if (!g.drawing || g.activeWire != wireIndex)
        {
            return;
        }

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
            if (PathHasIllegalContact(wireIndex))
            {
                RejectActivePathWithFade();
                return;
            }
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
        // 选关界面 hover 检测，给鼠标下的关卡方块一个灰度反馈。
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
        // 普通关卡通过后不立刻切走，而是等待 kClearReturnDelayMs。
        // 这段短暂停留让玩家能感知“这一关修复成功”。
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
}
