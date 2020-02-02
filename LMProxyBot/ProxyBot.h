#pragma once
#include <winsock2.h>

class ProxyBot 
{
public:
    ProxyBot(std::string InOpponent, std::string InServerAddress, std::string InServerPort, std::string InSc2Port, std::string InStartPort, std::string InRemoteServer)
        : Opponent(InOpponent)
        , ServerAddress(InServerAddress)
        , ServerPort(InServerPort)
        , Sc2Port(InSc2Port)
        , StartPort(InStartPort)
        , RemoteServer(InRemoteServer)
        , PlayVsHuman(true)
    {}
    ProxyBot(std::string InOpponent, std::string InServerAddress, std::string InServerPort, std::string InSc2Port, std::string InStartPort, std::string InUsername, std::string InPassword, std::string InRemoteServer)
        : Opponent(InOpponent)
        , ServerAddress(InServerAddress)
        , ServerPort(InServerPort)
        , Sc2Port(InSc2Port)
        , StartPort(InStartPort)
        , Username(InUsername)
        , Password(InPassword)
        , RemoteServer(InRemoteServer)
        , PlayVsHuman(false)
    {}


	std::string GenerateInitialRequest();

	uint64_t StartSC2Instance();

    SOCKET ConnectTo(std::string ServerAddress, std::string ServerPort);

    void RunProxy();

    std::mutex CritLock;

private:
    std::string Opponent;
    std::string ServerAddress;
    std::string ServerPort;
    std::string Sc2Port;
    std::string StartPort;
    std::string Username;
    std::string Password;
    std::string RemoteServer;
    bool PlayVsHuman;


};


void RunProxy(std::string Opponent, std::string ServerAddress, std::string ServerPort, std::string LocalSc2Port);

void RunProxy(std::string Opponent, std::string ServerAddress, std::string ServerPort, std::string LocalSc2Port, std::string Username, std::string Password);

/*
int main(int argc, char** argv)
{
    if (argc != 5)
    {
        printf("usage: %s Bot-name server-name server-port\n", argv[0]);
        return 1;
    }
    RunProxy(argv[1], argv[2], argv[3], argv[4]);
}
*/
