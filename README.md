# NKU大一下大作业

在这个库，我将不断记录并更新大作业的创作进度。

本创作大量依靠vibe coding，总觉得不是亲生的，但确乎花了不少时间，就当是新手时期的记录吧
## The Window Has Bugs

A small Win32 puzzle game inspired by ultra-minimal window games such as `Window is White`.

## Idea

The whole game lives inside one white window.
Each level only has:

- a rounded pane-like black frame
- a few red / yellow / blue endpoint pairs
- the line the player draws by hand

Press a colored endpoint and draw to the other endpoint of the same color.
When every color is connected and no lines cross, the level is cleared.

## Controls

- `Mouse drag`: draw a colored path
- `R`: restart the current level
- `Enter`: next level after solving
- `Esc`: quit

## Project Files

- `BugGame.sln`: Visual Studio solution
- `BugGame/BugGame.vcxproj`: C++ project
- `BugGame/BugGame.cpp`: full game source

## Build

1. Open `BugGame.sln` in Visual Studio.
2. Choose `Debug | x64`.
3. Run with `Local Windows Debugger`.

The output executable is generated at:

- `BugGame/bin/Debug/BugGame.exe`

## Developer Debug Mode

All existing levels are temporarily unlocked from the level-select screen for easier testing.
To restore normal level progression, set `kDeveloperUnlockAllLevels` to `false` in `BugGame/BugGame.cpp`.
