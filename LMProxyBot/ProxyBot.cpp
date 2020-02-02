#include <iostream>
#define WIN32_LEAN_AND_MEAN

#define RAPIDJSON_HAS_STDSTRING 1

#include "rapidjson.h"
#include "document.h"
#include "writer.h"
#include "stringbuffer.h"

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <thread>
#include <mutex>


#include "sc2utils/sc2_manage_process.h"
#include "sc2api/sc2_game_settings.h"
#include "sc2api/sc2_args.h"


#define DEFAULT_BUFLEN 16384
#define DEFAULT_PORT "26910"
//#define PROXY_SERVER "35.246.50.155"
#define PROXY_SERVER "127.0.0.1"
#define CONN_TIMEOUT 30
#define SC2PORT 5679
#include "ProxyBot.h"

static int conn_count = 0;
using namespace rapidjson;

/** Returns true on success, or false if there was an error */
bool SetSocketBlockingEnabled(int fd, bool blocking)
{
    if (fd < 0) return false;

#ifdef _WIN32
    unsigned long mode = blocking ? 0 : 1;
    return (ioctlsocket(fd, FIONBIO, &mode) == 0) ? true : false;
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
    return (fcntl(fd, F_SETFL, flags) == 0) ? true : false;
#endif
}

static void ShipData(const char* source, SOCKET ClientSocket, SOCKET ServerSocket, std::mutex *CritLock)
{
    char* recvbuf = (char*)malloc(DEFAULT_BUFLEN);
    if (!recvbuf)
    {
        return;
    }
    clock_t last_call = clock();
    int iResult;
    int recvbuflen = DEFAULT_BUFLEN;
    bool bShouldExit = false;
    while (!bShouldExit)
    {
        ZeroMemory(recvbuf, recvbuflen);
//        CritLock->lock();
        iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
//        CritLock->unlock();
        if (iResult < 0)
        {
            int result = WSAGetLastError();
            if (result == WSAEWOULDBLOCK)
            {
                double secs_since_last = double(clock() - last_call) / CLOCKS_PER_SEC;
                if (secs_since_last > CONN_TIMEOUT)
                {
                    printf("%s Connection timeout of %d seconds\n", source, WSAGetLastError());
                    bShouldExit = true;
                }
            }
            else
            {
                printf("%s recv failed with error: %d\n", source, WSAGetLastError());
                bShouldExit = true;
            }
        }

        else if (iResult == 0)
        {
            printf("%s Connection close\r\n", source);
            bShouldExit = true;
        }

        else
        {
            if (conn_count > 0)
            {
                printf("%s Recieved %d bytes\r\n", source, iResult);
                printf("\r\n\r\n%s\r\n\r\n", recvbuf);
                conn_count--;
            }
//            printf("%s Recieved %d bytes\r\n", source, iResult);
//            printf("\r\n\r\n%s\r\n\r\n", recvbuf);
            last_call = clock();
            char* tmp_data = recvbuf;
            int data_remaining = iResult;

            while (data_remaining > 0 && !bShouldExit)
            {
                int pkt_data_size = min(data_remaining, DEFAULT_BUFLEN); // replace 1024 with whatever maximum chunk size you want...

                char* pkt = (char*)malloc(pkt_data_size);
                if (!pkt)
                {
                    bShouldExit = true;
                }
                ZeroMemory(pkt, pkt_data_size);
                // fill header as needed...
                memcpy(pkt, tmp_data, pkt_data_size);

                char* tmp_pkt = pkt;
//                CritLock->lock();
                int Sresult = send(ServerSocket, tmp_pkt, pkt_data_size, 0);
//                CritLock->unlock();
                if (Sresult == INVALID_SOCKET)
                {
                    printf("%s Server Connection close\r\n", source);
                    bShouldExit = true;
                }
                if (Sresult == INVALID_SOCKET)
                {
                    bShouldExit = true;
                }
                else if (Sresult == SOCKET_ERROR )
                {
                    int resultCode = WSAGetLastError();
                    if (resultCode != WSAEWOULDBLOCK)
                    {
                        bShouldExit = true;
                    }
                }
                tmp_data += Sresult;
                data_remaining -= Sresult;
                free(pkt);
            }
//            printf("%s Sent %d bytes\r\n", source, (int)(tmp_data - recvbuf));

        }
    }
    free(recvbuf);
}

void RunProxy(std::string Opponent, std::string ServerAddress, std::string ServerPort, std::string LocalSc2Port, std::string LocalStartPort, std::string RemoteServer)
{
    ProxyBot ThisProxy(Opponent, ServerAddress, ServerPort, LocalSc2Port, LocalStartPort, RemoteServer);
    ThisProxy.RunProxy();
}

void RunProxy(std::string Opponent, std::string ServerAddress, std::string ServerPort, std::string LocalSc2Port, std::string LocalStartPort, std::string Username, std::string Password, std::string RemoteServer)
{
    ProxyBot ThisProxy(Opponent, ServerAddress, ServerPort, LocalSc2Port, LocalStartPort, Username, Password, RemoteServer);
    ThisProxy.RunProxy();
}


std::string ProxyBot::GenerateInitialRequest()
{
    Document ResponseDoc;
    ResponseDoc.SetObject();
    rapidjson::Document::AllocatorType& allocator = ResponseDoc.GetAllocator();
    ResponseDoc.AddMember( rapidjson::Value("BotName", allocator).Move(), rapidjson::Value(Opponent, allocator).Move(), allocator);
    ResponseDoc.AddMember( rapidjson::Value("StartPort", allocator).Move(), rapidjson::Value(StartPort, allocator).Move(), allocator);
    if (Username != "")
    {
        ResponseDoc.AddMember(rapidjson::Value("Username", allocator).Move(), rapidjson::Value(Username, allocator).Move(), allocator);
        ResponseDoc.AddMember(rapidjson::Value("Token", allocator).Move(), rapidjson::Value(Password, allocator).Move(), allocator);
    }
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    ResponseDoc.Accept(writer);
    buffer.ShrinkToFit();
    std::string OutputString = buffer.GetString();
    return OutputString;
}

uint64_t ProxyBot::StartSC2Instance()
{
    sc2::ProcessSettings process_settings;
    sc2::GameSettings game_settings;
    int SettingsArgc = 1;
    char* SettingsArgv[1] = { const_cast<char*>("ProxyBot") };
    sc2::ParseSettings(SettingsArgc, SettingsArgv, process_settings, game_settings);
    return sc2::StartProcess(process_settings.process_path,
        { "-listen", "127.0.0.1",
          "-port", std::to_string(SC2PORT),
          "-displaymode", "0",
          "-mastervolume", "0.0"
          "-dataVersion", process_settings.data_version });
}

SOCKET ProxyBot::ConnectTo(std::string ServerAddress, std::string ServerPort)
{
    int iResult;
    struct addrinfo* result = NULL, * ptr = NULL, hints;
    SOCKET ConnectSocket = INVALID_SOCKET;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    std::cout << " Connecting to " << ServerAddress << ":" << ServerPort << std::endl;
    // Resolve the server address and port
    iResult = getaddrinfo(ServerAddress.c_str(), ServerPort.c_str(), &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return INVALID_SOCKET;
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
            ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return INVALID_SOCKET;
        }

        // Connect to server.
        iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }
   
    freeaddrinfo(result);
    return ConnectSocket;
}

void ProxyBot::RunProxy()
{
   WSADATA wsaData;
   SOCKET ConnectSocket = INVALID_SOCKET;
   struct addrinfo* result = NULL, *ptr = NULL;
   char* recvbuf = (char*)malloc(DEFAULT_BUFLEN);
   if (recvbuf == nullptr)
   {
       printf("Failed to allocate memory for buffer");
       return;
   }
   int iResult;
   int recvbuflen = DEFAULT_BUFLEN;
   std::string InitialRequest = GenerateInitialRequest();

   // Initialize Winsock
   iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
   if (iResult != 0) {
       printf("WSAStartup failed with error: %d\n", iResult);
       return;
   }

//   ConnectSocket = ConnectTo("35.234.134.236", DEFAULT_PORT);
   ConnectSocket = ConnectTo(RemoteServer, DEFAULT_PORT);
   if (ConnectSocket == INVALID_SOCKET) {
        printf("Unable to connect to server!\n");
        WSACleanup();
        return;
    }
    // Send an initial buffer
    iResult = send(ConnectSocket, InitialRequest.c_str(), (int)strlen(InitialRequest.c_str()), 0);
    if (iResult == SOCKET_ERROR) {
        printf("send failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        return;
    }

//    printf("Bytes Sent: %ld\n", iResult);

    // Receive Initial Response
    ZeroMemory(recvbuf, recvbuflen);
    iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);

    if (iResult < 0)
    {
        printf("recv failed with error: %d\n", WSAGetLastError());
        return;
    }
    else if (iResult == 0)
    {
        printf("Connection closed\n");
        return;
    }

    printf("recieved response %s", recvbuf);
    Document RequestDoc;
    if (RequestDoc.Parse(recvbuf).HasParseError())
    {
        printf("Invalid Json Document");
        return;
    }
    if (RequestDoc.HasMember("Error") && RequestDoc["Error"].IsString())
    {
        printf("Server returned error: %s", RequestDoc["Error"].GetString());
        return;
    }
    if (!RequestDoc.HasMember("Success") || !RequestDoc["Success"].IsString())
    {
        printf("Server did not return success result");
        return;
    }
    printf("Server returned success, starting clients");

//    StartSC2Instance();
//    sc2::SleepFor(5000);
    // Set Sockets for non blocking
    u_long mode = 1;  // 1 to enable non-blocking socket
    SOCKET Sc2Socket = ConnectTo("127.0.0.1", Sc2Port);
    if (Sc2Socket == INVALID_SOCKET) {
        printf("Unable to connect to sc2 instance!\n");
        WSACleanup();
        return;
    }
    SetSocketBlockingEnabled(Sc2Socket, true);
    SetSocketBlockingEnabled(ConnectSocket, true);

    conn_count = 0;
    std::thread ClientThread(&ShipData, "CLIENT", ConnectSocket, Sc2Socket, &CritLock);
    std::thread Sc2Thread(&ShipData, "SC2INSTANCE", Sc2Socket, ConnectSocket, &CritLock);

    ClientThread.join();
    Sc2Thread.join();
     // cleanup
   closesocket(ConnectSocket);
   closesocket(Sc2Socket);
   WSACleanup();

   free(recvbuf);
   return;
}

