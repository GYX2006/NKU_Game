#include "../Game.h"

// Level09_ERASE.cpp
// 第九关是收束关，不再连线，而是用白色画笔把最后一个彩色 Bug 点涂白。
namespace BugGame
{
    LevelDefinition BuildLevel09Erase()
    {
        // 第 9 关：最终修复 Bug。
        // 这一关不是传统两点连线，而是让玩家用白色画笔把最后一个彩色 Bug 点“涂白”。
        // wires 为空表示不使用普通连线判定，胜利条件由 GeometryAndCollision.cpp 的涂白检测负责。
        return {
            {
            }
        };
    }
}
