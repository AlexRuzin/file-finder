#include <Windows.h>
#include <Shlwapi.h>

#include "main.h"
#include "debug.h"
#include "file_enum.h"
#include "queue.h"

#include <string>
#include <vector>
#include <mutex>
#include <cctype>
#include <iostream>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>

#pragma comment(lib, "Shlwapi.lib")

// Interval of substringMatches to stdout in ms
#define DUMP_SUBSTRING_MATCHES_INTERVAL_MS 5000

// Dump thread prototypes
static void initializeDumpThread(void);
static void cleanupDumpThread(void);
static void dumpAllSubstringsAndClear(void);

// Validates argv
static bool validateInput(const std::vector<std::string>& in);

// Callback prototype invoked by FileEnumerator, and handled by Pathsubstr
void processPathSubstr(std::string path);

// Initializes the Pathsubstr objects (handle substring)
static void initializeSubstringThreads(const std::vector<std::string>& inputParams);

// Cleanup
static void cleanupSubstringThreads(void);


// Dump thread globals
static std::thread *dumpThreadObj = nullptr;
static EventSignal dumpThreadSync;

// Vector of substring processing classes
static std::vector<PathSubstr *> substrArray;

// Substring path container (i.e. paths that matched a substring)
std::vector<std::string> substringMatches;
std::mutex substringMatchSync;

// FileEnumerator object needs to be handled by exitCallback(), so make it global
static FileEnumerator *fileEnum = nullptr;

int32_t main(int32_t argc, char *argv[])
{
    // Parse input into vector
    const std::vector<std::string> inputParams(argv, argv + argc);

    if (!validateInput(inputParams)) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }

    const std::string basePath(inputParams[1]);
    LOG_DEBUG(" [[[ Starting file-finder ]]] ");
    LOG_DEBUG(" Help: press 'd' to dump, press 'e' to exit at any time");
    LOG_DEBUG("Starting search at directory: " + basePath);
    LOG_DEBUG("Substring search: ");
    for (std::vector<std::string>::const_iterator i = inputParams.begin() + 2; i != inputParams.end(); i++) {
        LOG_NOLINE(*i + " ");
    }

    initializeDumpThread();
    initializeSubstringThreads(inputParams);

    // FileEnumerator needs a callback for processing substrings
    fileEnum = new FileEnumerator(basePath, processPathSubstr);
    fileEnum->StartFileEnum();

    while (true) {
        bool breakOut = false;
        if (_kbhit()) {
            const char ch = _getch();

            switch (ch) {
            case 'd':
                LOG_DEBUG(">dump signal");
                Sleep(500);
                dumpAllSubstringsAndClear();
                continue;
            case 'e':
                breakOut = true;
                break;
            default:
                continue;
            }

            if (breakOut) {
                break;
            }
        }
    }

    LOG_DEBUG("Received exit signal. Stopping all threads...");
    Sleep(2000);

    if (fileEnum) {
        fileEnum->StopFileEnum();
        delete fileEnum;
    }
    cleanupDumpThread();
    cleanupSubstringThreads();

    Sleep(1000);
    LOG_DEBUG("Clean Exit.");
    return 0;
}

// Also handle stdin/console i/o
static void dumpThread(void)
{
    while (!dumpThreadSync.WaitForSignalTimeout(DUMP_SUBSTRING_MATCHES_INTERVAL_MS)) {
        if (substringMatches.size() == 0) {
            continue;
        }

        dumpAllSubstringsAndClear();
    }
}

static void dumpAllSubstringsAndClear(void)
{
    system("cls");
    LOG_DEBUG("*** Printing current substringMatches ***");

    std::unique_lock<std::mutex> mlock(substringMatchSync);

    for (std::vector<std::string>::const_iterator i = substringMatches.begin(); i != substringMatches.end(); i++) {
        LOG_DEBUG(*i);
    }

    if (!fileEnum->GetIsRunning()) {
        LOG_DEBUG("Enumerator is not running, press 'e' to exit. " +
            std::to_string(substringMatches.size()) + " matches in the substring array");
    }

    substringMatches.clear();
}

static void initializeDumpThread(void)
{
    dumpThreadSync.ResetSignal();
    dumpThreadObj = new std::thread(&dumpThread);
}

static void cleanupDumpThread(void)
{
    dumpThreadSync.SetSignal();
    if (dumpThreadObj->joinable()) {
        dumpThreadObj->join();
    }

    delete dumpThreadObj;
}

static void initializeSubstringThreads(const std::vector<std::string> &inputParams)
{
    for (std::vector<std::string>::const_iterator i = inputParams.begin() + 2; i != inputParams.end(); i++) {
        PathSubstr *newSubstr = new PathSubstr(toLower(*i));
        newSubstr->StartSubstrThread();
        substrArray.push_back(newSubstr);
    }
}

static void cleanupSubstringThreads(void)
{
    LOG_DEBUG("Total Matches: ");

    for (std::vector<PathSubstr *>::iterator i = substrArray.begin(); i != substrArray.end(); i++) {
        (*i)->StopSubstrThread();
        LOG_DEBUG((*i)->GetSubstrString() + " : " + std::to_string((*i)->GetTotalMatches()));
        delete *i;
    }

    substrArray.clear();
}

static bool validateInput(const std::vector<std::string> &in)
{
    if (in.size() < 3) {
        displayHelp();
        return false;
    }

    // Folder must exist
    if (!isPathDirectory(in[1])) {
        LOG_ERROR("Target path is not available");
        return false;
    }

    return true;
}

// Callback function that is invoked from FileEnumerator, on each path that is a valid file
//  This function will dispatch substr
static void processPathSubstr(std::string path)
{
    //LOG_DEBUG("CurrPath: " + path);
    for (std::vector<PathSubstr *>::iterator i = substrArray.begin(); i != substrArray.end(); i++) {
        (*i)->InsertSubstrQuery(path);
    }
}

//
// PathSubstr class definitions
//
void PathSubstr::InsertSubstrQuery(std::string path)
{
    inputQueue.pushback(path);
}

void PathSubstr::StartSubstrThread(void)
{
    threadSync.ResetSignal();
    parsingThread = new std::thread(&PathSubstr::processSubstrThread, this);
}

void PathSubstr::StopSubstrThread(void)
{
    threadSync.SetSignal();

    if (parsingThread->joinable()) {
        parsingThread->join();
    }

    delete parsingThread;
}

void PathSubstr::processSubstrThread(void)
{
    while (!threadSync.WaitForSignalTimeout(10)) {
        while (inputQueue.size() != 0) {
            const std::string targetPath = inputQueue.frontpop_nonblock();

#if 0 // this used up over 60% CPU, due to GetFileAttributesA() on each file
            if (!isPathFile(targetPath)) {
                continue;
            }
#endif

            // Get the filename only
            const std::string filename(toLower(PathFindFileNameA(targetPath.c_str())));

            if (filename.find(targetSubstr) != std::string::npos) {
                std::unique_lock<std::mutex> mlock(substringMatchSync);
                totalMatches++;
                substringMatches.push_back(targetPath);
            }
        }
    }
}

// chatgpt
std::string toLower(const std::string& input) {
    std::string result = input;
    for (char& c : result) {
        c = std::tolower(static_cast<unsigned char>(c));
    }
    return result;
}

//EOF