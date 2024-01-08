#pragma once

#include "queue.h"
#include "signal.h"

#include <thread>
#include <string>

std::string toLower(const std::string& input);

// Substring processing class
// A class is substantiated for each substring to search for, along with a thread that receives paths
// from FileEnumerator and parses them. PathSubstr will then append to substrArray
class PathSubstr {
private:
    const std::string           targetSubstr;

    SafeQueue<std::string>      inputQueue;
    EventSignal                 threadSync;
    std::thread                 *parsingThread;

    uint32_t                    totalMatches;

public:
    PathSubstr(std::string targetSubstr) :
        targetSubstr(targetSubstr),
        parsingThread(nullptr),
        totalMatches(0)
    {

    }

    ~PathSubstr(void)
    {

    }

    void InsertSubstrQuery(std::string path);

    void StartSubstrThread(void);
    void StopSubstrThread(void);

    uint32_t GetTotalMatches(void) const
    {
        return totalMatches;
    }

    std::string GetSubstrString(void) const
    {
        return targetSubstr;
    }

private:
    void processSubstrThread(void);
};
