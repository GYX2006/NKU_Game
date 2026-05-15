#include "Game.h"

namespace BugGame
{
    // Graphics.cpp 放底层绘图工具。
    // 它不决定“什么时候画什么关卡”，只提供画圆、画线、画圆角矩形、画淡出线等基础能力。
    // 真正的画面组织在 RenderScene.cpp。

    HICON CreateGameIcon(int size)
    {
        // 程序图标不用外部资源文件，而是运行时用 GDI 画出来。
        // 左上角红、蓝、黄三个小方块呼应游戏只用少量颜色的视觉规则。
        HDC screenDc = GetDC(nullptr);
        HDC memoryDc = CreateCompatibleDC(screenDc);
        HBITMAP colorBitmap = CreateCompatibleBitmap(screenDc, size, size);
        HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memoryDc, colorBitmap));

        RECT iconRect = MakeRect(0, 0, size, size);
        FillRectColor(memoryDc, iconRect, kWhite);

        const int gap = std::max(1, size / 8);
        const int square = std::max(3, (size - gap * 4) / 3);
        const int left = gap;
        const int top = gap;
        const int secondRow = top + square + gap;
        const int secondColumn = left + square + gap;

        FillRectColor(memoryDc, MakeRect(left, top, left + square, top + square), kRed);
        FillRectColor(memoryDc, MakeRect(left, secondRow, left + square, secondRow + square), kBlue);
        FillRectColor(memoryDc, MakeRect(secondColumn, secondRow, secondColumn + square, secondRow + square), kYellow);

        SelectObject(memoryDc, oldBitmap);
        DeleteDC(memoryDc);
        ReleaseDC(nullptr, screenDc);

        const int maskStride = ((size + 15) / 16) * 2;
        std::vector<BYTE> maskBits(static_cast<size_t>(maskStride * size), 0);
        HBITMAP maskBitmap = CreateBitmap(size, size, 1, 1, maskBits.data());
        ICONINFO iconInfo{};
        iconInfo.fIcon = TRUE;
        iconInfo.hbmColor = colorBitmap;
        iconInfo.hbmMask = maskBitmap;
        HICON icon = CreateIconIndirect(&iconInfo);

        DeleteObject(colorBitmap);
        DeleteObject(maskBitmap);
        return icon;
    }

    COLORREF BlendColor(COLORREF from, COLORREF to, float amount)
    {
        // 颜色插值，用于非法线淡出：从原本颜色逐渐接近背景色。
        amount = std::clamp(amount, 0.0f, 1.0f);
        const int red = static_cast<int>(std::round(GetRValue(from) + (GetRValue(to) - GetRValue(from)) * amount));
        const int green = static_cast<int>(std::round(GetGValue(from) + (GetGValue(to) - GetGValue(from)) * amount));
        const int blue = static_cast<int>(std::round(GetBValue(from) + (GetBValue(to) - GetBValue(from)) * amount));
        return RGB(red, green, blue);
    }

    HPEN CreateRoundedPen(COLORREF color, int width)
    {
        // 普通 CreatePen 的线头比较硬，这里使用 ExtCreatePen 创建圆头圆角画笔。
        // 这样玩家画出来的线条更像参考游戏里的柔和手绘路径。
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

    void DrawColoredStroke(HDC hdc, const std::vector<Vec2>& points, COLORREF color, int width)
    {
        // 所有玩家路径最终都会走这个函数绘制。
        // 先 BuildDisplayPath 做视觉平滑，再用圆角画笔连接每个点。
        if (points.size() < 2)
        {
            return;
        }

        PathState whitePath{};
        whitePath.points = points;
        const std::vector<Vec2> displayPath = BuildDisplayPath(whitePath, false);
        if (displayPath.size() < 2)
        {
            return;
        }

        HPEN linePen = CreateRoundedPen(color, width);
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, linePen));
        MoveToEx(hdc, static_cast<int>(displayPath.front().x), static_cast<int>(displayPath.front().y), nullptr);
        for (size_t i = 1; i < displayPath.size(); ++i)
        {
            LineTo(hdc, static_cast<int>(displayPath[i].x), static_cast<int>(displayPath[i].y));
        }
        SelectObject(hdc, oldPen);
        DeleteObject(linePen);
    }

    void DrawWhiteStroke(HDC hdc, const std::vector<Vec2>& points)
    {
        DrawColoredStroke(hdc, points, kWhite, kWireThickness);
    }

    void DrawFadingPaths(HDC hdc)
    {
        // 穿墙或交叉的线不会马上消失，而是在 kInvalidFadeMs 时间内淡出。
        // 这个反馈比“突然清空”更像原视频里不允许操作的感觉。
        const DWORD now = GetTickCount();
        const COLORREF background = CurrentBackgroundColor();
        for (const FadingPath& fadingPath : g.fadingPaths)
        {
            const float rawProgress = static_cast<float>(now - fadingPath.startedAtTick) / static_cast<float>(kInvalidFadeMs);
            const float progress = std::clamp(rawProgress, 0.0f, 1.0f);
            const float eased = progress * progress;
            const COLORREF color = BlendColor(fadingPath.color, background, eased);
            const int width = std::max(1, static_cast<int>(std::round(kWireThickness * (1.0f - 0.55f * eased))));
            DrawColoredStroke(hdc, fadingPath.points, color, width);
        }
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

    // 以下几个函数只负责把特殊关卡的浅灰色墙体画出来。
    // 碰撞矩形由 GeometryAndCollision.cpp 生成，绘制和判定使用同一套墙体数据。
    void DrawBendStructure(HDC hdc)
    {
        for (const RECT& wall : GetBendWallRects())
        {
            FillRectColor(hdc, wall, kWallGray);
        }
    }

    void DrawGapStructure(HDC hdc)
    {
        for (const RECT& wall : GetGapWallRects())
        {
            FillRectColor(hdc, wall, kWallGray);
        }
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

    void DrawEraserLevel(HDC hdc, const RECT& clientRect)
    {
        // 第九关单独绘制：白底、一个由第八关文件名颜色决定的 Bug 点、白色涂抹轨迹。
        // 通关后用打字效果显示 All Bugs Are Fixed，作为整个游戏的收束。
        FillRectColor(hdc, clientRect, kWhite);

        if (!g.cleared)
        {
            const Vec2 bugCenter = FinalBugPointCenter();
            const COLORREF bugColor = FinalBugPointColor();
            const COLORREF outlineColor = bugColor == kWhite ? kWallGray : bugColor;
            DrawCircle(hdc, bugCenter, 34, bugColor, outlineColor, bugColor == kWhite ? 2 : 1);

            for (const std::vector<Vec2>& stroke : g.eraserPaintStrokes)
            {
                DrawWhiteStroke(hdc, stroke);
            }
            if (g.erasing)
            {
                DrawWhiteStroke(hdc, g.activeEraserStroke);
            }

            DrawCircle(hdc, g.eraserCursor, kAnchorHintRadius, kWhite, kWallGray, 1);
            return;
        }

        const std::wstring message = L"All Bugs Are Fixed";
        const DWORD elapsed = GetTickCount() - g.clearedAtTick;
        const size_t visibleCount = std::min(message.size(), static_cast<size_t>(elapsed / 75));
        const std::wstring visibleText = message.substr(0, visibleCount);
        const int pulse = static_cast<int>(std::min<DWORD>(24, elapsed / 30));
        RECT textRect = clientRect;
        textRect.top += HeightOf(clientRect) / 2 - 58;
        textRect.bottom = textRect.top + 116;
        DrawTextBlock(hdc, visibleText, textRect, 42 + pulse / 8, FW_BOLD, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}
