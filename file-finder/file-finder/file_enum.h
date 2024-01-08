#pragma once

#ifndef _FILEENUM_H_
#define _FILEENUM_H_

#include <Windows.h>

#include <string>
#include <thread>

// Undefine to remove search delay (ms)
#undef FILE_ENUM_DELAY_MS           1

bool isPathDirectory(std::string path);
bool isPathFile(std::string path);

// Callback that invokes substring threads on each new file (that is not a directory)
typedef void (*fileEnumCallback)(std::string path);

class FileEnumerator {
private:
    const std::string               searchPath;

    // This callback is invoked on each newly discovered file, for example the function 
    //  that dispatches filepaths to the substring parser
    const fileEnumCallback          fileCallback;

    // Separate thread runs the enumerator
    EventSignal                     threadSync;
    bool                            isRunning;

    std::thread                     *enumThread;

public:
    FileEnumerator(std::string searchPath, fileEnumCallback cb) :
        searchPath(searchPath),
        fileCallback(cb), 
        isRunning(false),
        enumThread(nullptr)
    {
        // Strip last character if it's '/'
        if (!searchPath.empty() && (searchPath.back() == '\\' || searchPath.back() == '/')) {
            searchPath.pop_back();
        }
    }

    ~FileEnumerator(void)
    {

    }

    // Thread control and cleanup
    void StartFileEnum(void);
    void StopFileEnum(void);

    bool GetIsRunning(void) const
    {
        return isRunning;
    }

private:
    // Thread handles substring function
    void fileEnumThread(void);

    void fileEnumRecursive(std::string path);
};

#endif _FILEENUM_H_