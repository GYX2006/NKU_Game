#include "Source/Game.h"

// BugGame.cpp 是项目的“门面文件”。
// 最初所有代码都可以写在一个 cpp 里，但那样不利于大作业答辩，也不利于展示代码结构。
// 现在真正的 Windows 入口放在 Source/WinMain.cpp，其他功能拆到 Source 目录下：
// 1. Game.h 保存公共结构体、常量和函数声明。
// 2. Layout.cpp 负责端点、棋盘、窗口缩放相关的坐标计算。
// 3. Input.cpp 负责鼠标输入、画线和第九关涂白操作。
// 4. RulesAndFlow.cpp 负责通关判定、非法线淡出、选关和流程切换。
// 5. Graphics.cpp / RenderScene.cpp 负责 GDI 绘制。
// 6. Levels/*.cpp 每个关卡单独维护，方便录视频时逐关讲解。
