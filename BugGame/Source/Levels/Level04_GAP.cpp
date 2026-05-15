#include "../Game.h"

// Level04_GAP.cpp
// 第四关把隔板缺口做成解题关键，训练玩家观察哪里才是真正可以通过的位置。
namespace BugGame
{
    LevelDefinition BuildLevel04Gap()
    {
        // 第 4 关：缺口隔板关。
        // 红、黄、蓝三组点分布在隔板两侧，解法依赖墙上的小缺口。
        // 这一关开始强调“线不能穿墙”，必须从真正开放的位置穿过去。
        return {
            {
                { kRed, 0, 14 },
                { kYellow, 6, 8 },
                { kBlue, 2, 12 },
            }
        };
    }
}
