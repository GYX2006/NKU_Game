#include "Game.h"

namespace BugGame
{
    // SystemProgress.cpp 负责和“程序本身”有关的功能：
    // 1. 读取 exe 文件名，实现第八关改名变色。
    // 2. 保存/读取通关进度，实现关闭后还能继续。
    // 3. 全部通关后重置进度，方便录屏时下次从第一关开始。

    std::wstring GetExecutableFileName()
    {
        // GetModuleFileNameW 可以拿到当前 exe 的完整路径。
        // 这里只截取最后的文件名，例如 A White Bug.exe。
        wchar_t path[MAX_PATH]{};
        GetModuleFileNameW(nullptr, path, MAX_PATH);

        const std::wstring fullPath = path;
        const size_t slash = fullPath.find_last_of(L"\\/");
        if (slash == std::wstring::npos)
        {
            return fullPath;
        }
        return fullPath.substr(slash + 1);
    }

    std::wstring GetExecutableStem()
    {
        // 去掉 .exe 后缀，只留下标题主体。
        // 第八关和窗口标题都用这个“无扩展名文件名”。
        std::wstring fileName = GetExecutableFileName();
        const size_t dot = fileName.find_last_of(L'.');
        if (dot != std::wstring::npos)
        {
            fileName.resize(dot);
        }
        return fileName;
    }

    std::wstring ToLowerCopy(std::wstring text)
    {
        for (wchar_t& ch : text)
        {
            ch = static_cast<wchar_t>(std::towlower(ch));
        }
        return text;
    }

    COLORREF BackgroundColorFromExecutableName()
    {
        // 第八关核心：把文件名当作谜题输入。
        // 玩家把 A White Bug.exe 改成 A Blue Bug.exe 后，背景就会变蓝。
        // 白色端点因此显现出来，形成“通过改程序名修复 Bug”的效果。
        const std::wstring name = ToLowerCopy(GetExecutableStem());
        if (name.find(L"blue") != std::wstring::npos)
        {
            return kBlue;
        }
        if (name.find(L"yellow") != std::wstring::npos)
        {
            return kYellow;
        }
        if (name.find(L"green") != std::wstring::npos)
        {
            return kGreen;
        }
        if (name.find(L"red") != std::wstring::npos)
        {
            return kRed;
        }
        if (name.find(L"black") != std::wstring::npos)
        {
            return kBlack;
        }
        return kWhite;
    }

    COLORREF CurrentBackgroundColor()
    {
        // 只有第八关使用文件名决定背景色。
        // 其他关卡保持白底，避免颜色机制影响普通谜题。
        if (g.screen == ScreenMode::Playing && g.levelIndex == kNameColorLevelIndex)
        {
            return BackgroundColorFromExecutableName();
        }
        return kWhite;
    }

    std::wstring WindowTitleFromExecutableName()
    {
        // 窗口标题也跟随 exe 文件名变化。
        // 这样玩家改名后，标题栏会和第八关背景变化形成呼应。
        const std::wstring stem = GetExecutableStem();
        if (stem.empty() || ToLowerCopy(stem) == L"buggame")
        {
            return kDefaultWindowTitle;
        }
        return stem;
    }

    std::wstring GetExecutableDirectory()
    {
        wchar_t path[MAX_PATH]{};
        GetModuleFileNameW(nullptr, path, MAX_PATH);

        const std::wstring fullPath = path;
        const size_t slash = fullPath.find_last_of(L"\\/");
        if (slash == std::wstring::npos)
        {
            return L".";
        }
        return fullPath.substr(0, slash);
    }

    std::wstring GetSaveFilePath()
    {
        // 存档文件放在 exe 同目录，用户移动整个 Bug 文件夹时进度也跟着走。
        const std::wstring directory = GetExecutableDirectory();
        if (directory.empty() || directory == L".")
        {
            return L"bug_progress.txt";
        }
        return directory + L"\\bug_progress.txt";
    }

    bool HasCompletedAllLevels()
    {
        return !g.completedLevels.empty() &&
            std::all_of(g.completedLevels.begin(), g.completedLevels.end(), [](bool completed)
                {
                    return completed;
                });
    }

    void ResetProgressToStart()
    {
        const size_t levelCount = g.defs.empty() ? g.completedLevels.size() : g.defs.size();
        g.completedLevels.assign(levelCount, false);
        g.unlockedLevels = levelCount == 0 ? 0 : 1;
    }

    void NormalizeSequentialProgress()
    {
        // 防止旧的开发调试存档破坏正式流程。
        // 如果存档里出现“前面没通，后面却通了”的断层，只保留连续通过的前缀。
        if (HasCompletedAllLevels())
        {
            ResetProgressToStart();
            return;
        }

        const int levelCount = static_cast<int>(g.completedLevels.size());
        int completedPrefix = 0;
        while (completedPrefix < levelCount && g.completedLevels[static_cast<size_t>(completedPrefix)])
        {
            ++completedPrefix;
        }

        // 正式版本只允许第一个未完成关卡可玩。
        // 旧调试阶段可能留下的跳关记录会在这里被清理掉。
        for (int i = completedPrefix; i < levelCount; ++i)
        {
            g.completedLevels[static_cast<size_t>(i)] = false;
        }
        g.unlockedLevels = levelCount == 0 ? 0 : std::clamp(completedPrefix + 1, 1, levelCount);
    }

    void SaveProgress()
    {
        // 用简单文本文件保存进度，方便课程作业展示。
        // 格式非常直观：解锁到第几关、每关是否完成。
        FILE* file = nullptr;
        if (_wfopen_s(&file, GetSaveFilePath().c_str(), L"w") != 0 || file == nullptr)
        {
            return;
        }

        fwprintf(file, L"WINDOW_BUG_PROGRESS 1\n");
        fwprintf(file, L"unlocked %d\n", g.unlockedLevels);
        fwprintf(file, L"completed ");
        for (bool completed : g.completedLevels)
        {
            fwprintf(file, completed ? L"1" : L"0");
        }
        fwprintf(file, L"\n");
        fclose(file);
    }

    bool LoadProgress()
    {
        // 读取 bug_progress.txt。
        // 如果文件不存在或格式不对，就返回 false，让游戏从第一关开始。
        FILE* file = nullptr;
        if (_wfopen_s(&file, GetSaveFilePath().c_str(), L"r") != 0 || file == nullptr)
        {
            return false;
        }

        wchar_t line[256]{};
        if (fgetws(line, static_cast<int>(sizeof(line) / sizeof(line[0])), file) == nullptr)
        {
            fclose(file);
            return false;
        }

        const std::wstring header = line;
        if (header.find(L"WINDOW_BUG_PROGRESS") == std::wstring::npos)
        {
            fclose(file);
            return false;
        }

        int savedUnlocked = g.unlockedLevels;
        std::vector<bool> savedCompleted(g.completedLevels.size(), false);
        while (fgetws(line, static_cast<int>(sizeof(line) / sizeof(line[0])), file) != nullptr)
        {
            const std::wstring text = line;
            if (text.rfind(L"unlocked ", 0) == 0)
            {
                savedUnlocked = _wtoi(text.c_str() + 9);
            }
            else if (text.rfind(L"completed ", 0) == 0)
            {
                constexpr size_t kCompletedPrefixLength = 10;
                for (size_t i = 0; i < savedCompleted.size() && kCompletedPrefixLength + i < text.size(); ++i)
                {
                    savedCompleted[i] = text[kCompletedPrefixLength + i] == L'1';
                }
            }
        }
        fclose(file);

        const int levelCount = static_cast<int>(g.defs.size());
        g.completedLevels = savedCompleted;
        g.unlockedLevels = std::clamp(savedUnlocked, 1, std::max(1, levelCount));
        NormalizeSequentialProgress();
        return true;
    }
}
