#include "TCPRequestChannel.h"

using namespace std;


TCPRequestChannel::TCPRequestChannel (const std::string _ip_address, const std::string _port_no) {
    if (_ip_address == "") {
        struct sockaddr_in server;
        int server_sock;

        server_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (server_sock < 0) {
            perror("Error opening socket");
        }

        int portno = atoi(_port_no.c_str());
        bzero((char *)&server, sizeof(server));
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = INADDR_ANY;
        server.sin_port = htons(portno);

        if (bind(server_sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
            perror("Error on binding");
        }

        listen(server_sock, 5);
    }
    else {
        struct sockaddr_in server_info;
        struct hostent* server;
        int client_sock;

        client_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (client_sock < 0) {
            perror("Error opening socket");
        }

        server = gethostbyname(_ip_address.c_str());
        int portno = atoi(_port_no.c_str());
        bzero((char *)&server_info, sizeof(server_info));
        bcopy((char *)server->h_addr, (char *)&server_info.sin_addr.s_addr, server->h_length);
        server_info.sin_family = AF_INET;
        server_info.sin_port = htons(portno);

        if (connect(client_sock, (struct sockaddr *) &server_info, sizeof(server_info)) < 0) {
            perror ("Error on connecting");
        }
    }
}

TCPRequestChannel::TCPRequestChannel (int _sockfd) {
    sockfd = _sockfd;
}

TCPRequestChannel::~TCPRequestChannel () {
    close(sockfd);
}

int TCPRequestChannel::accept_conn () {
    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);
    int newsockfd;

    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    return newsockfd;
}

int TCPRequestChannel::cread (void* msgbuf, int msgsize) {
    return read(sockfd, msgbuf, msgsize);
}

int TCPRequestChannel::cwrite (void* msgbuf, int msgsize) {
    return write(sockfd, msgbuf, msgsize);
}
