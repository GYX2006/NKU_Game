#include "../Game.h"

// Level06_DIVIDE.cpp
// 第六关让中间隔板像 Bug 一样无限延伸，必须通过缩放窗口从外侧绕过。
namespace BugGame
{
    LevelDefinition BuildLevel06Divide()
    {
        // 第 6 关：无限隔板 Bug。
        // 中间竖直隔板在逻辑上会一直向下延伸；缩放窗口后，玩家能从窗体下方绕过去。
        // 线仍然不能穿过隔板本体，只能利用窗口外侧空间解决。
        return {
            {
                { kYellow, 0, 10 },
                { kRed, 6, 8 },
                { kBlue, 4, 14 },
            }
        };
    }
}
