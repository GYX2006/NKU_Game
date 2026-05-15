#include "../Game.h"

// Level01_BOOT.cpp
// 第一关只保留最基础的一组同色点，目的是让玩家在没有干扰的情况下学会拖线。
namespace BugGame
{
    LevelDefinition BuildLevel01Boot()
    {
        // 第 1 关：基础教学关。
        // 只有一对水平摆放的红点，让玩家先理解“按住同色点并连到另一端”的核心操作。
        // { 颜色, 起点锚点编号, 终点锚点编号 }，这里表示红色从 5 号点连到 9 号点。
        return {
            {
                { kRed, 5, 9 },
            }
        };
    }
}
