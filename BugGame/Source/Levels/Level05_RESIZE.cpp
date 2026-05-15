#include "../Game.h"

// Level05_RESIZE.cpp
// 第五关利用窗口尺寸制造“看不见的点”，让玩家第一次把窗口本身当作解谜工具。
namespace BugGame
{
    LevelDefinition BuildLevel05Resize()
    {
        // 第 5 关：窗口尺寸 Bug。
        // 画面里先只显示上方三个点，另外三个对应点藏在下边框外。
        // 玩家需要拖动窗口下边框，让隐藏点露出来后再完成三组连接。
        return {
            {
                { kYellow, 0, 10 },
                { kBlue, 1, 11 },
                { kRed, 2, 12 },
            }
        };
    }
}
