/*
 @ 0xCCCCCCCC
*/

#include <iostream>

#include <windows.h>
#include <winsock2.h>

#include "kbase/at_exit_manager.h"
#include "kbase/command_line.h"
#include "kbase/error_exception_util.h"
#include "kbase/scope_guard.h"
#include "kbase/scoped_handle.h"

#include "iocp_utils.h"
#include "scoped_socket.h"

namespace {

BOOL WINAPI ControlCtrlHandler(DWORD ctrl)
{
    switch (ctrl) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
            return TRUE;

        default:
            return FALSE;
    }
}

void InitializeWinsock()
{
    WSADATA data {0};
    auto result_code = WSAStartup(MAKEWORD(2, 2), &data);
    ENSURE(THROW, result_code == 0)(result_code).Require();
    std::cout << "-*- Windows Socket Library Initialized -*-\n";
}

void CleanWinsock()
{
    WSACleanup();
    std::cout << "-*- Windows Socket Library Cleaned -*-\n";
}

ScopedSocketHandle CreateListener(unsigned short port, int max_pending_clients)
{
    ScopedSocketHandle listener(
        WSASocketW(AF_INET, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED));
    ENSURE(CHECK, !!listener).Require();

    sockaddr_in server_addr {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int rv = bind(listener.get(), reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
    ENSURE(CHECK, rv == 0)(WSAGetLastError()).Require();

    rv = listen(listener.get(), max_pending_clients);
    ENSURE(CHECK, rv == 0)(WSAGetLastError()).Require();

    return listener;
}

}   // namespace

int main()
{
    kbase::CommandLine::Init(0, nullptr);
    kbase::AtExitManager exit_manager;

    InitializeWinsock();
    ON_SCOPE_EXIT { CleanWinsock(); };

    SetConsoleCtrlHandler(ControlCtrlHandler, TRUE);
    ON_SCOPE_EXIT { SetConsoleCtrlHandler(nullptr, FALSE); };

    constexpr unsigned short kPort = 8088;
    constexpr int kMaxPending = 10;

    auto listener = CreateListener(kPort, kMaxPending);

    // Default to number of cores in the system.
    auto io_port = utils::CreateNewIOCP(utils::CompletionKeyIO, 0);

    bool success = utils::AssociateDeviceWithIOCP(reinterpret_cast<HANDLE>(listener.get()),
                                                  io_port.get(),
                                                  utils::CompletionKeyIO);
    ENSURE(CHECK, success)(kbase::LastError()).Require();

    std::cin.get();

    return 0;
}
