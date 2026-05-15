#include "Game.h"

namespace BugGame
{
    // 统一生成所有关卡。
    // 这里的顺序就是游戏实际流程顺序，也是选关界面 3 x 3 九宫格的顺序。
    // 每个 BuildLevelXX 函数都在 Levels 文件夹的独立 cpp 文件里，便于单独讲解和修改。
    std::vector<LevelDefinition> BuildLevels()
    {
        return {
            BuildLevel01Boot(),
            BuildLevel02Sweep(),
            BuildLevel03Bend(),
            BuildLevel04Gap(),
            BuildLevel05Resize(),
            BuildLevel06Divide(),
            BuildLevel07Box(),
            BuildLevel08White(),
            BuildLevel09Erase(),
        };
    }
}
