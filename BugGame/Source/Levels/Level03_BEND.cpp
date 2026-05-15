#include "../Game.h"

// Level03_BEND.cpp
// 第三关开始引入墙体，重点从“连起来”变成“绕开限制再连起来”。
namespace BugGame
{
    LevelDefinition BuildLevel03Bend()
    {
        // 第 3 关：弯折路线关。
        // 这一关会绘制外框和局部隔板，玩家不能只画直线，需要利用空隙绕出弯曲路线。
        // 关卡文件只定义配对关系，具体墙体由绘制/碰撞模块按关卡编号生成。
        return {
            {
                { kRed, 0, 14 },
                { kYellow, 7, 13 },
                { kBlue, 3, 11 },
            }
        };
    }
}
