#include "../Game.h"

// Level08_WHITE.cpp
// 第八关把程序文件名变成谜题输入：改名改变背景色，白色端点才会显现。
namespace BugGame
{
    LevelDefinition BuildLevel08White()
    {
        // 第 8 关：标题改色 Bug。
        // 程序会读取 exe 文件名里的颜色词，例如 Blue / Yellow / Green。
        // 改名后背景色改变，白色端点才会从白色背景里显现出来。
        return {
            {
                { kWhite, 0, 14 },
            }
        };
    }
}
