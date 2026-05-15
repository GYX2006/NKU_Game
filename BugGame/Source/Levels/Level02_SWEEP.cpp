#include "../Game.h"

// Level02_SWEEP.cpp
// 第二关开始加入三种颜色，让玩家理解“不同颜色的线也不能互相交叉”。
namespace BugGame
{
    LevelDefinition BuildLevel02Sweep()
    {
        // 第 2 关：多颜色连线关。
        // 没有隔板，重点是让玩家同时处理红、黄、蓝三条线，并开始注意不同颜色之间不能交叉。
        // 三行锚点编号来自 Game.h 里的 3 x 5 锚点表。
        return {
            {
                { kRed, 0, 9 },
                { kYellow, 5, 14 },
                { kBlue, 10, 4 },
            }
        };
    }
}
