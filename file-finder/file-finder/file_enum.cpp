#include <Windows.h>

#include "main.h"
#include "file_enum.h"
#include "debug.h"

void FileEnumerator::StartFileEnum(void)
{
    if (isRunning || !isPathDirectory(searchPath) || !fileCallback) {
        return;
    }

    threadSync.ResetSignal();

    isRunning = true;
    enumThread = new std::thread(&FileEnumerator::fileEnumThread, this);

    // I ran into an issue with enumThread->join() causing an abort() (in StopFileEnum()), not enough time to investigate it,
    //  but calling detach() prior seemed to resolve the exception
    enumThread->detach();
}

void FileEnumerator::StopFileEnum(void)
{
    threadSync.SetSignal();
    isRunning = false;

    if (enumThread->joinable()) {
        enumThread->join();
    }

    delete enumThread;
}

void FileEnumerator::fileEnumRecursive(std::string path)
{
    WIN32_FIND_DATAA findData = { 0 };
    HANDLE findDataHandle = INVALID_HANDLE_VALUE;

    findDataHandle = FindFirstFileA((path + "\\*").c_str(), &findData);
    if (findDataHandle == INVALID_HANDLE_VALUE) {
        //isRunning = false; // account for directories without access
        return;
    }

    do {
#if defined(FILE_ENUM_DELAY_MS)
        Sleep(FILE_ENUM_DELAY_MS);
#endif //FILE_ENUM_DELAY_MS
        const std::string currPath = path + "\\" + findData.cFileName;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (currPath.back() == '.' || currPath.back() == '..') {
                continue;
            }

            fileEnumRecursive(currPath);
        } else {
            if (fileCallback) {
                fileCallback(currPath);
            }
        }

    } while (!threadSync.IsSignaled() && FindNextFileA(findDataHandle, &findData));

    FindClose(findDataHandle);
}

void FileEnumerator::fileEnumThread(void)
{
    fileEnumRecursive(searchPath);
    isRunning = false;
}

bool isPathDirectory(std::string path)
{
    const DWORD attributes = GetFileAttributesA(path.c_str());

    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    else if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
        return true;
    }

    return false;
}

bool isPathFile(std::string path)
{
    const DWORD attributes = GetFileAttributesA(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return false;
    }

    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return true;
    }

    return false;
}