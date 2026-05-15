#include "Game.h"

namespace BugGame
{
    // 全局唯一的游戏状态对象。
    // 这个项目是单窗口小游戏，没有复杂的对象生命周期；
    // 用一个 GameState 集中保存运行时数据，可以让各个模块共享当前关卡、路径、窗口尺寸等信息。
    // Game.h 中只写 extern GameState g;，这里才是真正创建变量的地方。
    GameState g;
}
