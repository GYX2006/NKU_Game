#include "Game.h"

namespace BugGame
{
    // GeometryAndCollision.cpp 是判定模块。
    // 这里不负责画图，也不直接处理鼠标，而是回答三个核心问题：
    // 1. 两条线有没有交叉？
    // 2. 当前线段有没有穿过墙体/隔板？
    // 3. 第九关的彩色点有没有被白色笔刷涂够面积？

    float DistanceSquared(const Vec2& a, const Vec2& b)
    {
        // 返回距离平方，避免频繁开根号。
        // 只比较远近时，用平方距离速度更快，也足够准确。
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        return dx * dx + dy * dy;
    }

    float DistancePointToSegmentSquared(const Vec2& point, const Vec2& a, const Vec2& b)
    {
        // 计算点到线段的最短距离平方。
        // 第九关需要判断白色笔刷有没有覆盖到圆点上的采样位置，会用到这个函数。
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const float length2 = dx * dx + dy * dy;
        if (length2 <= 0.001f)
        {
            return DistanceSquared(point, a);
        }

        const float rawT = ((point.x - a.x) * dx + (point.y - a.y) * dy) / length2;
        const float t = std::max(0.0f, std::min(1.0f, rawT));
        const Vec2 closest{
            a.x + dx * t,
            a.y + dy * t
        };
        return DistanceSquared(point, closest);
    }

    float StrokeLength(const std::vector<Vec2>& points)
    {
        // 把一条鼠标轨迹的所有小线段长度加起来，得到笔画总长度。
        // 过短的笔画会被忽略，避免误触。
        float length = 0.0f;
        for (size_t i = 1; i < points.size(); ++i)
        {
            length += std::sqrt(DistanceSquared(points[i - 1], points[i]));
        }
        return length;
    }

    bool StrokePassesNear(const std::vector<Vec2>& points, const Vec2& target, float radius)
    {
        // 判断一条笔画是否经过某个目标点附近。
        // 这比要求鼠标正好落在目标点上更宽容，也更符合手绘体验。
        if (points.size() < 2)
        {
            return false;
        }

        const float radius2 = radius * radius;
        for (size_t i = 0; i + 1 < points.size(); ++i)
        {
            if (DistancePointToSegmentSquared(target, points[i], points[i + 1]) <= radius2)
            {
                return true;
            }
        }
        return false;
    }

    bool TryRecognizeCircle(const std::vector<Vec2>& points, EraserCircle& circle)
    {
        // 早期方案：识别玩家是否徒手画了一个圆。
        // 当前最终玩法改为“把彩色点涂白”，但这段保留用于展示迭代过程。
        if (points.size() < 18)
        {
            return false;
        }

        float minX = points.front().x;
        float maxX = points.front().x;
        float minY = points.front().y;
        float maxY = points.front().y;
        for (const Vec2& point : points)
        {
            minX = std::min(minX, point.x);
            maxX = std::max(maxX, point.x);
            minY = std::min(minY, point.y);
            maxY = std::max(maxY, point.y);
        }

        const float width = maxX - minX;
        const float height = maxY - minY;
        if (width < 28.0f || height < 28.0f || width > 180.0f || height > 180.0f)
        {
            return false;
        }

        const float aspect = std::max(width, height) / std::max(1.0f, std::min(width, height));
        if (aspect > 1.65f)
        {
            return false;
        }

        const Vec2 center{
            (minX + maxX) * 0.5f,
            (minY + maxY) * 0.5f
        };

        float radiusSum = 0.0f;
        for (const Vec2& point : points)
        {
            radiusSum += std::sqrt(DistanceSquared(point, center));
        }
        const float averageRadius = radiusSum / static_cast<float>(points.size());
        if (averageRadius < 15.0f || averageRadius > 90.0f)
        {
            return false;
        }

        float variance = 0.0f;
        for (const Vec2& point : points)
        {
            const float radius = std::sqrt(DistanceSquared(point, center));
            const float delta = radius - averageRadius;
            variance += delta * delta;
        }
        const float deviation = std::sqrt(variance / static_cast<float>(points.size()));

        const float closeDistance = std::sqrt(DistanceSquared(points.front(), points.back()));
        const float length = StrokeLength(points);
        if (closeDistance > std::max(24.0f, averageRadius * 0.8f))
        {
            return false;
        }
        if (length < averageRadius * 4.0f)
        {
            return false;
        }
        if (deviation > averageRadius * 0.48f)
        {
            return false;
        }

        circle.center = center;
        circle.radius = averageRadius;
        circle.points = points;
        return true;
    }

    bool StrokeTouchesCircle(const std::vector<Vec2>& points, const EraserCircle& circle)
    {
        return StrokePassesNear(points, circle.center, circle.radius + static_cast<float>(kWireThickness + 12));
    }

    Vec2 FinalBugPointCenter()
    {
        RECT client{};
        GetClientRect(g.hwnd, &client);
        return Vec2{
            client.left + WidthOf(client) * 0.5f,
            client.top + HeightOf(client) * 0.52f
        };
    }

    COLORREF FinalBugPointColor()
    {
        return BackgroundColorFromExecutableName();
    }

    float FinalBugPaintCoverage()
    {
        // 第九关不要求逐像素计算，因为那样代码复杂且性能浪费。
        // 这里在圆点内部放一组采样点，统计多少采样点被白色笔画覆盖。
        // 覆盖比例达到阈值，就认为玩家成功“修复最后一个 Bug”。
        const Vec2 center = FinalBugPointCenter();
        constexpr float bugRadius = 34.0f;
        constexpr float sampleStep = bugRadius * 0.42f;
        const float brushReach = static_cast<float>(kWireThickness) + 8.0f;

        int covered = 0;
        int total = 0;
        for (int y = -2; y <= 2; ++y)
        {
            for (int x = -2; x <= 2; ++x)
            {
                const Vec2 sample{
                    center.x + x * sampleStep,
                    center.y + y * sampleStep
                };
                if (DistanceSquared(sample, center) > bugRadius * bugRadius)
                {
                    continue;
                }

                ++total;
                bool sampleCovered = false;
                for (const std::vector<Vec2>& stroke : g.eraserPaintStrokes)
                {
                    if (StrokePassesNear(stroke, sample, brushReach))
                    {
                        sampleCovered = true;
                        break;
                    }
                }
                if (!sampleCovered && g.erasing)
                {
                    sampleCovered = StrokePassesNear(g.activeEraserStroke, sample, brushReach);
                }
                if (sampleCovered)
                {
                    ++covered;
                }
            }
        }

        if (total == 0)
        {
            return 0.0f;
        }
        return static_cast<float>(covered) / static_cast<float>(total);
    }

    bool IsFinalBugPainted()
    {
        return FinalBugPaintCoverage() >= 0.58f;
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
        // 玩家画线时，同一条线内部可能有相邻小段共享端点；
        // 共享端点不代表交叉，所以这里先排除完全相同的端点。
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

    bool PointInsideRect(const Vec2& point, const RECT& rect)
    {
        // RECT 是 Win32 的整数矩形，Vec2 是浮点坐标。
        // 这里统一转成 float 比较，用于墙体碰撞检测。
        return point.x >= static_cast<float>(rect.left) &&
            point.x <= static_cast<float>(rect.right) &&
            point.y >= static_cast<float>(rect.top) &&
            point.y <= static_cast<float>(rect.bottom);
    }

    bool SegmentIntersectsRect(const Vec2& a, const Vec2& b, const RECT& rect)
    {
        // 线段撞矩形有两种情况：
        // 1. 端点已经在矩形内部；
        // 2. 线段和矩形四条边相交。
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

    bool PointNearVisibleEndpoint(const Vec2& point)
    {
        // 端点圆点会画在墙体上方，如果严格按墙体矩形检测，
        // 玩家刚从圆点出发就可能被判定“穿墙”。
        // 所以端点附近留出安全区域，让线可以正常从点里出来。
        if (g.screen != ScreenMode::Playing)
        {
            return false;
        }

        const float safeRadius = static_cast<float>(kEndpointRadius + kWireThickness);
        const float safeRadius2 = safeRadius * safeRadius;
        for (const WireDefinition& wire : g.wires)
        {
            if (DistanceSquared(point, EndpointVisualToClient(wire.startAnchor)) <= safeRadius2 ||
                DistanceSquared(point, EndpointVisualToClient(wire.endAnchor)) <= safeRadius2)
            {
                return true;
            }
        }
        return false;
    }

    bool SegmentHitsWallRect(const Vec2& a, const Vec2& b, const RECT& rect)
    {
        // 先做快速矩形相交检测，再沿线段取样。
        // 取样的原因是：端点附近要特殊放行，只靠矩形相交无法表达这种细节。
        if (!SegmentIntersectsRect(a, b, rect))
        {
            return false;
        }

        const float length = std::sqrt(DistanceSquared(a, b));
        const int steps = std::max(1, static_cast<int>(std::ceil(length / 3.0f)));
        for (int i = 0; i <= steps; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const Vec2 sample{
                a.x + (b.x - a.x) * t,
                a.y + (b.y - a.y) * t
            };

            // 端点圆点压在某些墙端上方。把端点附近当作开口，
            // 否则线还没离开彩色点就会被拒绝。
            if (PointInsideRect(sample, rect) && !PointNearVisibleEndpoint(sample))
            {
                return true;
            }
        }
        return false;
    }

    bool SegmentHitsWallRects(const Vec2& a, const Vec2& b, const std::vector<RECT>& walls)
    {
        for (const RECT& wall : walls)
        {
            if (SegmentHitsWallRect(a, b, wall))
            {
                return true;
            }
        }
        return false;
    }

    std::vector<RECT> GetBendWallRects()
    {
        // 第三关墙体：用若干水平/竖直矩形拼出参考图里的外框和小隔板。
        // 使用相对比例而不是写死像素，方便窗口大小变化时仍能保持布局。
        const RECT board = g.fixedAnchorRect;
        const int thickness = kWallThickness;
        std::vector<RECT> walls;
        walls.reserve(10);

        auto xAt = [&](float rel)
        {
            return board.left + static_cast<int>(std::round(rel * WidthOf(board)));
        };
        auto yAt = [&](float rel)
        {
            return board.top + static_cast<int>(std::round(rel * HeightOf(board)));
        };
        auto horizontal = [&](float leftRel, float rightRel, float centerYRel)
        {
            const int y = yAt(centerYRel);
            walls.push_back(MakeRect(xAt(leftRel), y - thickness / 2, xAt(rightRel), y + thickness / 2));
        };
        auto vertical = [&](float centerXRel, float topRel, float bottomRel)
        {
            const int x = xAt(centerXRel);
            walls.push_back(MakeRect(x - thickness / 2, yAt(topRel), x + thickness / 2, yAt(bottomRel)));
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
        return walls;
    }

    std::vector<RECT> GetGapWallRects()
    {
        // 第四关墙体：外框 + 中间隔板 + 两处缺口。
        // 玩家必须从真正留出的缺口走，不能直接穿过浅灰色墙体。
        const RECT board = g.fixedAnchorRect;
        const int thickness = kWallThickness;
        std::vector<RECT> walls;
        walls.reserve(9);

        auto xAt = [&](float rel)
        {
            return board.left + static_cast<int>(std::round(rel * WidthOf(board)));
        };
        auto yAt = [&](float rel)
        {
            return board.top + static_cast<int>(std::round(rel * HeightOf(board)));
        };
        auto horizontal = [&](float leftRel, float rightRel, float centerYRel)
        {
            const int y = yAt(centerYRel);
            walls.push_back(MakeRect(xAt(leftRel), y - thickness / 2, xAt(rightRel), y + thickness / 2));
        };
        auto vertical = [&](float centerXRel, float topRel, float bottomRel)
        {
            const int x = xAt(centerXRel);
            walls.push_back(MakeRect(x - thickness / 2, yAt(topRel), x + thickness / 2, yAt(bottomRel)));
        };

        horizontal(0.0f, 1.0f, 0.0f);
        horizontal(0.0f, 1.0f, 1.0f);
        vertical(0.0f, 0.0f, 1.0f);
        vertical(1.0f, 0.0f, 1.0f);
        vertical(0.500f, 0.0f, 0.305f);
        vertical(0.500f, 0.705f, 1.0f);
        horizontal(0.0f, 0.115f, 0.500f);
        horizontal(0.145f, 0.245f, 0.500f);
        horizontal(0.755f, 1.0f, 0.500f);
        return walls;
    }

    bool SegmentHitsBendWall(const Vec2& a, const Vec2& b)
    {
        return g.screen == ScreenMode::Playing &&
            g.levelIndex == kBendLevelIndex &&
            SegmentHitsWallRects(a, b, GetBendWallRects());
    }

    bool SegmentHitsGapWall(const Vec2& a, const Vec2& b)
    {
        return g.screen == ScreenMode::Playing &&
            g.levelIndex == kGapLevelIndex &&
            SegmentHitsWallRects(a, b, GetGapWallRects());
    }

    RECT GetDividerWallRect()
    {
        // 第六关的中间隔板在视觉上只显示在窗口内，
        // 但逻辑上会随客户区高度变化，玩家需要通过窗口下方绕过去。
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
            SegmentHitsWallRect(a, b, GetDividerWallRect());
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
        // 第七关的盒子由四条矩形墙组成。
        // 后续碰撞会和当前客户区取交集，实现“窗口遮住的墙不参与碰撞”。
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

            if (SegmentHitsWallRect(a, b, visibleWall))
            {
                return true;
            }
        }
        return false;
    }

    bool SegmentHitsActiveWall(const Vec2& a, const Vec2& b)
    {
        // 统一入口：输入模块不需要关心当前是哪一关，
        // 只要问“这段线有没有碰到当前关卡有效墙体”即可。
        return SegmentHitsBendWall(a, b) ||
            SegmentHitsGapWall(a, b) ||
            SegmentHitsDividerWall(a, b) ||
            SegmentHitsBoxWall(a, b);
    }

    bool IsEraserLevel()
    {
        return g.screen == ScreenMode::Playing && g.levelIndex == kEraserLevelIndex;
    }

    bool IsEraserPuzzleSolved()
    {
        return IsFinalBugPainted();
    }

    void FinishEraserLevelIfSolved()
    {
        // 第九关通关后不自动回选关，而是留在最终画面播放打字动画。
        // 因为这是收尾关卡，节奏上应该让玩家看到 All Bugs Are Fixed。
        if (!IsEraserLevel() || g.cleared || !IsEraserPuzzleSolved())
        {
            return;
        }

        g.cleared = true;
        g.completedLevels[static_cast<size_t>(g.levelIndex)] = true;
        g.unlockedLevels = std::min(static_cast<int>(g.defs.size()), std::max(g.unlockedLevels, g.levelIndex + 2));
        SaveProgress();
        g.clearedAtTick = GetTickCount();
        g.pendingReturnToSelect = false;
        g.status = L"All bugs are fixed.";
    }
}
