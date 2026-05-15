#include "../Game.h"

// Level07_BOX.cpp
// 第七关把点关进盒子里，利用“窗口裁剪后墙体不再可见”的规则突破限制。
namespace BugGame
{
    LevelDefinition BuildLevel07Box()
    {
        // 第 7 关：盒中点 Bug。
        // 只有一组红点，但其中一个点被方框围住。
        // 玩家需要通过改变窗口尺寸/进入窗口外侧路径，让线从可行空间绕回目标点。
        return {
            {
                { kRed, 0, 8 },
            }
        };
    }
}
