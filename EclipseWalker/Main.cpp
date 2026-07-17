#include "EclipseWalkerGame.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace
{
    void WriteStartupErrorLog(const std::wstring& message)
    {
        wchar_t modulePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, modulePath, MAX_PATH);

        const std::filesystem::path logPath =
            std::filesystem::path(modulePath).parent_path() / L"EclipseWalker_startup_error.log";

        std::wofstream log(logPath, std::ios::app);
        if (!log.is_open())
        {
            return;
        }

        log << L"==================================================\n";
        log << message << L"\n";
    }

    bool HasRequiredAssetRoot(const std::filesystem::path& path)
    {
        std::error_code ec;
        return std::filesystem::exists(path / L"Textures" / L"Title.dds", ec) &&
            std::filesystem::exists(path / L"Textures" / L"myfile.spritefont", ec);
    }

    void NormalizeWorkingDirectory()
    {
        std::error_code ec;
        const std::filesystem::path current = std::filesystem::current_path(ec);
        if (!ec && HasRequiredAssetRoot(current))
        {
            return;
        }

        wchar_t modulePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
        const std::filesystem::path exeDir = std::filesystem::path(modulePath).parent_path();

        const std::filesystem::path candidates[] =
        {
            exeDir,
            exeDir.parent_path() / L"EclipseWalker",
            exeDir.parent_path().parent_path() / L"EclipseWalker",
            current / L"EclipseWalker",
            current.parent_path() / L"EclipseWalker"
        };

        for (const auto& candidate : candidates)
        {
            if (HasRequiredAssetRoot(candidate))
            {
                SetCurrentDirectoryW(candidate.wstring().c_str());
                return;
            }
        }

        WriteStartupErrorLog(L"Required asset root was not found. Make sure the Textures folder is available.");
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
    UNREFERENCED_PARAMETER(prevInstance);
    UNREFERENCED_PARAMETER(cmdLine);
    UNREFERENCED_PARAMETER(showCmd);

#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        NormalizeWorkingDirectory();

        EclipseWalkerGame theGame(hInstance);
        if (!theGame.Initialize())
        {
            WriteStartupErrorLog(L"EclipseWalkerGame::Initialize() returned false.");
            return 0;
        }

        return theGame.Run();
    }
    catch (DxException& e)
    {
        WriteStartupErrorLog(e.ToString());
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
    catch (const std::exception& e)
    {
        const std::string message = e.what();
        const std::wstring wideMessage(message.begin(), message.end());
        WriteStartupErrorLog(L"Unhandled std::exception: " + wideMessage);
        MessageBoxA(nullptr, e.what(), "Unhandled Exception", MB_OK);
        return 0;
    }
    catch (...)
    {
        WriteStartupErrorLog(L"Unhandled unknown exception.");
        MessageBox(nullptr, L"Unhandled unknown exception.", L"Unhandled Exception", MB_OK);
        return 0;
    }
}
