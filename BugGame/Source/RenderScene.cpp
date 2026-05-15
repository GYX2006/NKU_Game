#include "Game.h"

namespace BugGame
{
    // RenderScene.cpp 是画面的总调度。
    // 它负责决定当前屏幕模式下应该画哪些元素：
    // 选关界面只画 9 个圆角方块；普通关卡画墙、线、淡出效果和端点；第九关交给专门函数。

    void DrawScene(HDC hdc, const RECT& clientRect)
    {
        // 每一帧先用当前背景色铺满窗口。
        // 第八关背景色会根据 exe 文件名变化，其他关卡默认白色。
        FillRectColor(hdc, clientRect, CurrentBackgroundColor());

        if (g.screen == ScreenMode::LevelSelect)
        {
            // 选关界面不显示文字，只用 3 x 3 方块表现关卡入口。
            // 已通关、可游玩、悬停状态用轻微灰度差异区分，保持简约。
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

        if (g.levelIndex == kEraserLevelIndex)
        {
            // 最后一关的目标不是连接同色点，而是涂白 Bug 点，所以单独绘制。
            DrawEraserLevel(hdc, clientRect);
            return;
        }


        // 路径会先做一次显示平滑，所以视觉上比原始折线更接近曲线。
        // 根据关卡编号画出对应墙体。墙体的真实碰撞矩形在 GeometryAndCollision.cpp 中保持一致。
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
            // 逐条绘制玩家已经画出的路径。
            // appendCursor 为 true 时，当前鼠标位置也会临时加入路径，使拖动过程实时跟手。
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

        DrawFadingPaths(hdc);

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
        // 双缓冲绘制：先画到内存位图，再一次性拷贝到窗口。
        // 这样即使 240 FPS 高频刷新，也能尽量避免窗口闪烁。
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
