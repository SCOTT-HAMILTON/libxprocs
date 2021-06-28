
/**
    \copyright GNU General Public License v3.0 Copyright (c) 2019, Adewale Azeez 
    \author Adewale Azeez <azeezadewale98@gmail.com>
    \date 05 January 2019
    \file processes.cpp
*/
#include <libopen/processes.h> 
#include <iostream> //TODO: remove
#include <QDebug>
#include <Psapi.h>
#include <tchar.h>
#include <winrt/base.h>

std::string GetErrorMessage(DWORD errorMessageID)
{
    //Get the error message ID, if any.
    if(errorMessageID == 0) {
        return std::string(); //No error message has been recorded
    }

    LPSTR messageBuffer = nullptr;

    //Ask Win32 to give us the string version of that message ID.
    //The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

    //Copy the error message into a std::string.
    std::string message(messageBuffer, size);

    //Free the Win32's string's buffer.
    LocalFree(messageBuffer);

    return message;
}
namespace libopen {

/**
 */
LIBOPEN_API void InitProcess( PROCESS *process ) 
{
    process->status = PROCESS_STATUS::UNKNOWN;
    process->Id = 0;
    process->cpuUsage = 0;
    process->memoryUsage = 0;
    process->networkUsage = 0;
    process->diskUsage = 0;
    process->userId = 0;
    process->lifeTime = 0;
    process->threadCount = 1;
}

/**
 */
LIBOPEN_API PROCESS GetProcessById( unsigned int processID )
{
    PROCESS p;
    InitProcess(&p);
    p.Id = processID;
#ifdef _WIN32
    WCHAR szProcessName[MAX_PATH] = TEXT("Unknown");
    WCHAR szProcessFileName[MAX_PATH] = TEXT("Unknown");
    size_t szProcessNameSize = 0;
    size_t szProcessFileNameSize = 0;
    // Get a handle to the process.
    HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION |
                                  PROCESS_VM_READ,
                                  FALSE, processID );
    // Get the process name.
    if (NULL != hProcess ) {
        HMODULE hMod;
        DWORD cbNeeded;

        if ( EnumProcessModulesEx( hProcess, &hMod, sizeof(hMod),
        &cbNeeded, LIST_MODULES_ALL) ) {
           szProcessNameSize = GetModuleBaseName( hProcess, hMod, szProcessName,
                                   sizeof(szProcessName)/sizeof(WCHAR));
           if (szProcessNameSize == 0) {
               throw FailedToGetProcessError(GetErrorMessage(GetLastError()).c_str());
            }
            szProcessFileNameSize = GetModuleFileName(hMod, szProcessFileName, sizeof(szProcessFileName)/sizeof(WCHAR));
            if (szProcessFileNameSize == 0) {
                throw FailedToGetProcessError(GetErrorMessage(GetLastError()).c_str());
            }
        } else {
            throw FailedToGetProcessError(GetErrorMessage(GetLastError()).c_str());
        }
    } else {
        throw FailedToGetProcessError(GetErrorMessage(GetLastError()).c_str());
    }
    p.exeName = winrt::to_string(std::wstring(szProcessName,szProcessNameSize));
    p.exePath = winrt::to_string(std::wstring(szProcessFileName,szProcessFileNameSize));
    qDebug() << "path : " << p.exePath.c_str();
    // Release the handle to the process.

    CloseHandle( hProcess );
#endif
    return p;
}

/**
    Get the list of running processes. The processes are returned in std::vector. 
    The first argument is a callback function that is invoked with the `PROCESS` and 
    second argument `extraParam` if it not NULL everytime a `PROCESS` is found. 
    If the callback function returns true it is added to the `std::vector<PROCESS>` to 
    be returned if it returns false the `PROCESS` is not added to the return value.
    If the first and seond param is `NULL` all running process is returned
    
    To get all the processes without any condition or calback the first and second parameters 
    should be `NULL` 
    
    \code{.cpp}
    std::vector<PROCESS> processes = RunningProcesses(NULL, NULL);
    \endcode
 
    \param callbackCondition The call back function that must return either true or false.
    \param extraParam extra parameter passed to the callbackCondition callback function if specified.
    
    \return the std::vector of running PROCESSes
    
*/

LIBOPEN_API std::vector<PROCESS> RunningProcessesWithEnumProcesses( ProcessCondition callbackCondition, void* extraParam ) {
    std::vector<PROCESS> processes;
#ifdef _WIN32
    DWORD aProcesses[1024], cbNeeded, cProcesses;
    unsigned int i;
    if ( !EnumProcesses( aProcesses, sizeof(aProcesses), &cbNeeded ) )
    {
        return processes;
    }
    // Calculate how many process identifiers were returned.
    cProcesses = cbNeeded / sizeof(DWORD);
    // Print the name and process identifier for each process.
    for ( i = 0; i < cProcesses; i++ )
    {
        if( aProcesses[i] != 0 )
        {
            try {
                PROCESS p = GetProcessById((unsigned int)aProcesses[i] );
                if ( callbackCondition != NULL ) {
                    if (callbackCondition(p, extraParam) == true) {
                        processes.push_back(p);
                    }
                } else {
                    processes.push_back(p);
                }
            } catch (const FailedToGetProcessError& e) {
//                qDebug() << e.what();
            }
        }
    }
#else
#endif
    return processes;
}
LIBOPEN_API std::vector<PROCESS> RunningProcessesWithCreateToolhelp32Snapshot( ProcessCondition callbackCondition, void* extraParam ) {
    std::vector<PROCESS> processes;
#ifdef _WIN32
    HANDLE hProcessSnap;
    HANDLE hProcess;
    PROCESSENTRY32 pe32;

    // Take a snapshot of all processes in the system.
    hProcessSnap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
    if( hProcessSnap == INVALID_HANDLE_VALUE ) {
        qDebug() << "[error] : CreateToolhelp32Snapshot returned INVALID_HANDLE_VALUE: " << GetErrorMessage(GetLastError()).c_str();
        return processes;
    }
    // Set the size of the structure before using it.
    pe32.dwSize = sizeof( PROCESSENTRY32 );
    // Retrieve information about the first process,
    // and exit if unsuccessful
    if( !Process32First( hProcessSnap, &pe32 ) ) {
        CloseHandle( hProcessSnap ); // clean the snapshot object
        qDebug() << "[error] : Process32First Failed: " << GetErrorMessage(GetLastError()).c_str();
        return processes;
    }
    // Now walk the snapshot of processes, and
    // display information about each process in turn
    do {
        PROCESS p;
        InitProcess(&p);
        p.exeName = winrt::to_string(std::wstring(pe32.szExeFile));
        // Retrieve the priority class.
//        hProcess = OpenProcess( PROCESS_QUERY_INFORMATION|PROCESS_VM_READ, FALSE, pe32.th32ProcessID );
        hProcess = OpenProcess( PROCESS_ALL_ACCESS, FALSE, pe32.th32ProcessID );
        if( hProcess == NULL ) {
//            qDebug() << "[error] : OpenProcess Failed: " << GetErrorMessage(GetLastError()).c_str();
            continue;
        }
        p.Id = pe32.th32ProcessID;
        p.exePath = p.exeName;
//        qDebug() << "PROCESS ID : " << pe32.th32ProcessID;
        if ( callbackCondition != NULL ) {
            if (callbackCondition(p, extraParam) == true) {
                processes.push_back(p);
            }
        } else {
            processes.push_back(p);
        }    } while( Process32Next( hProcessSnap, &pe32 ) );
    CloseHandle( hProcessSnap );
#else
#endif
    return processes;
}

LIBOPEN_API std::vector<PROCESS> RunningProcesses( ProcessCondition callbackCondition, void* extraParam ) 
{
    return RunningProcessesWithCreateToolhelp32Snapshot(callbackCondition, extraParam);
}

/**

*/
bool CompareProcNameCondition( PROCESS process, void* extraParam )
{
    return (process.exeName == ((char*) extraParam));
}

/**

*/
LIBOPEN_API PROCESS GetProcessByName( const char* processName )
{
    PROCESS process;
    InitProcess(&process);
    process.exeName = processName;
    std::vector<PROCESS> processes = GetProcessesByName(processName);
    if ( processes.size() > 0) 
    {
        process = processes.at(0);
        processes.clear();
    }
    return process;
}

/**
    //TODO: possibly return the first process spawned
*/
LIBOPEN_API std::vector<PROCESS> GetProcessesByName( const char* processName )
{
    return RunningProcesses(&CompareProcNameCondition, (void*)processName);
}

/**

*/
bool CompareProcPathLikeCondition( PROCESS process, void* extraParam )
{
    return (process.exePath.find((char*) extraParam) != std::string::npos);
}

/**
    Get the first process by part of the execuitable path value `PROCESS.exePath`
    //TODO: possibly return the first process spawned
*/
LIBOPEN_API PROCESS GetProcessByPart( const char* processName )
{
    PROCESS process;
    InitProcess(&process);
    process.exeName = processName;
    std::vector<PROCESS> processes = GetProcessesByPart(processName);
    if ( processes.size() > 0) 
    {
        process = processes.at(0);
        processes.clear();
    }
    return process;
}

/**

*/
LIBOPEN_API std::vector<PROCESS> GetProcessesByPart( const char* processName )
{
    return RunningProcesses(&CompareProcPathLikeCondition, (void*)processName);
}

/**

*/
LIBOPEN_API std::string GetProcessPathFromId( int processId ) 
{
    return GetProcessById(processId).exePath;
}

LIBOPEN_API std::string ProcessToString( PROCESS process )
{
    std::string str_value;
    str_value += "Id=";
    str_value += std::to_string(process.Id);
    str_value += ",Status=";
    str_value += std::to_string(process.status);
    str_value += ",ExeName=";
    str_value += process.exeName.c_str();
    str_value += ",ExePath=";
    str_value += process.exePath.c_str();
    str_value += ",ThreadCount=";
    str_value += std::to_string(process.threadCount);
    str_value += ",LifeTime=";
    str_value += std::to_string(process.lifeTime);
    str_value += ",CpuUsage=";
    str_value += std::to_string(process.cpuUsage);
    str_value += ",DiskUsage=";
    str_value += std::to_string(process.diskUsage);
    str_value += ",MemoryUsage=";
    str_value += std::to_string(process.memoryUsage);
    return str_value;
}

// Listeners and lifecycles


// hacky

#ifdef USE_HACKY_PROCESSES_MONITOR

//use id as key
LIBOPEN_API void Hacky_MonitorProcess( const char* processName, ProcessStatusChanged processStatusCallback, void* extraParam )
{
    std::map<std::string, PROCESS_STATUS> mapOfProcess;
    PROCESS process;
    do {
        process = GetProcessByPart(processName);
        if (process.status == PROCESS_STATUS::UNKNOWN) {
            if (mapOfProcess.find(process.exeName) != mapOfProcess.end()) {
                if (mapOfProcess[process.exeName] != PROCESS_STATUS::STOPPED) {
                    process.status = PROCESS_STATUS::STOPPED;
                }
                mapOfProcess.erase(process.exeName);
            }
            goto report_process_status;
        }
        if (mapOfProcess.find(process.exeName) == mapOfProcess.end()) {
            process.status = PROCESS_STATUS::STARTED;
            mapOfProcess.insert(std::make_pair(process.exeName, PROCESS_STATUS::UNKNOWN));
        }
        if (mapOfProcess[process.exeName] != process.status) {
            mapOfProcess[process.exeName] = process.status;
            goto report_process_status;
        }
        
        continue;
        report_process_status:
            if (processStatusCallback != NULL) {
                processStatusCallback(process, extraParam);
            }
    } while(true);
}

//use id as key
//also convert name to lowercase
LIBOPEN_API void Hacky_MonitorProcessPath( const char* processName, ProcessStatusChanged processStatusCallback, void* extraParam )
{
    std::map<std::string, PROCESS> mapOfProcess;
    std::vector<PROCESS> processes;
    PROCESS process;
    do {
        processes = GetProcessesByPart(processName);
        std::vector<PROCESS>::iterator it; 
        std::set<std::string> found_processes;
        for(it = processes.begin(); it != processes.end(); ++it) {
            process = *it;
            found_processes.insert(process.exeName);
            if (process.status == PROCESS_STATUS::UNKNOWN) {
                if (mapOfProcess.find(process.exeName) != mapOfProcess.end()) {
                    if (mapOfProcess[process.exeName].status != PROCESS_STATUS::STOPPED) {
                        process.status = PROCESS_STATUS::STOPPED;
                    }
                    mapOfProcess.erase(process.exeName);
                }
                goto report_process_status;
            }
            if (mapOfProcess.find(process.exeName) == mapOfProcess.end()) {
                mapOfProcess.insert(std::make_pair(process.exeName, process));
                mapOfProcess[process.exeName].status = PROCESS_STATUS::STARTED;
            }
            goto report_process_status;
            
            continue;
            report_process_status:
                if (processStatusCallback != NULL) {
                    processStatusCallback(mapOfProcess[process.exeName], extraParam);
                    mapOfProcess[process.exeName].status = process.status;
                }
        }
        if (mapOfProcess.size() != processes.size()) {
            std::map<std::string, PROCESS>::iterator it;
            for (it = mapOfProcess.begin(); it != mapOfProcess.end(); it++ )
            {
                if (found_processes.find(it->first) == found_processes.end())
                {
                    if (mapOfProcess[it->first].status != PROCESS_STATUS::STOPPED) {
                        mapOfProcess[it->first].status = PROCESS_STATUS::STOPPED;
                    }
                    if (processStatusCallback != NULL) {
                        processStatusCallback(mapOfProcess[it->first], extraParam);
                    }
                    mapOfProcess.erase(it->first);
                }
            }
        }
        found_processes.clear();
    } while(true);
}

#endif

}
