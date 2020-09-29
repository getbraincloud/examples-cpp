#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include "RoomServer.h"

static const uint16_t PORT = 7777;

struct Response
{
    uint16_t size;
    const char message[10] = "CONNECTED";
};

int main(int argc, char** argv)
{
    printf("-- Room Server Example Server 4.5.6 --\n");

    // Create a RoomServer utility class that will fetch the Lobby data
    // from brainCloud.
    RoomServer roomServer;
    if (!roomServer.init()) exit(EXIT_FAILURE);

    // Load your game content. Perform setup that need to be done.
    // ...

    // Create a TCP socket
    auto serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (!serverSocket)
    {
        printf("Failed socket()\n");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 7777
    struct sockaddr_in address;
    int opt = 1; 
    int addrlen = sizeof(address); 

    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt)))
    {
        printf("setsockopt\n");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Forcefully attaching socket to the port 7777 
    if (bind(serverSocket, (struct sockaddr *)&address, sizeof(address)) < 0) 
    { 
        printf("bind failed\n"); 
        exit(EXIT_FAILURE); 
    } 

    // Set the socket for listening
    if (listen(serverSocket, 1) < 0)
    {
        printf("Failed listen()\n");
        exit(EXIT_FAILURE);
    }
    
    // Notify brainCloud that we are ready
    roomServer.ready();

    // Accept new connections
    auto clientSocket = accept(serverSocket, (struct sockaddr *)&address, 
                               (socklen_t*)&addrlen);
    if (clientSocket < 0)
    {
        printf("Failed accept()\n");
        exit(EXIT_FAILURE);
    }
    printf("New connection\n");

    // Read packet from client.
    char buffer[1024] = {0};
    read(clientSocket, buffer, 1024);

    // In our example, the first packet will be his passcode. This way we can
    // validate if he's supposed to be here.
    printf("Client passcode: %s\n", buffer);
    if (!roomServer.validatePasscode(buffer))
    {
        printf("Wrong passcode()\n");
        exit(EXIT_FAILURE);
    }

    // Tell the client he's connected
    Response response;
    response.size = htons((uint16_t)sizeof(Response));
    send(clientSocket, &response, sizeof(response), 0);
    printf("CONNECTED sent\n");

    exit(EXIT_SUCCESS);
}
