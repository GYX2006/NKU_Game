#include "Game.h"

namespace BugGame
{
    // Layout.cpp 负责所有“坐标从哪里来”的问题。
    // 游戏里有三类坐标：
    // 1. 窗口客户区坐标：鼠标、GDI 绘图都使用这个坐标系。
    // 2. 棋盘相对坐标：Game.h 中 0.16f、0.24f 这类比例值。
    // 3. 特殊关卡坐标：第五关隐藏在窗口下方，第七关盒子被窗口裁剪。
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
        // 第三、四关需要绘制类似参考图的外框和隔板，所以使用更大的结构化面板。
        // 其他关卡保持普通居中面板，让画面更干净。
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
        // 选关界面是 3 x 3 九宫格。
        // 这里根据窗口大小计算整个网格左上角，使它始终视觉居中。
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
        // 普通关卡共用一套 3 x 5 锚点；
        // 特殊关卡为了贴合墙体位置，会换用专属锚点布局。
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
        // 把“相对棋盘比例坐标”转换为“窗口客户区像素坐标”。
        // 例如 anchor.x = 0.5f 表示棋盘水平中心。
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
        // 目前 visualAnchors 大多和 fixedAnchors 相同。
        // 单独留出这个函数，是为了让以后某些点可以只改变显示位置，而不改变逻辑锚点。
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
        // 这是第六、七关能“缩小窗口挡住隔板”的关键。
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
            // 第七关不用普通 15 点布局，而是手动放置两个红点和一个盒子。
            // 盒子围住右侧红点；当窗口缩小时，被窗口裁掉的墙不再参与碰撞。
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
        // 把第几个关卡按钮转换成按钮中心点。
        // 当前版本按钮上不显示文字，只用浅色圆角方块保持极简感。
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

}
