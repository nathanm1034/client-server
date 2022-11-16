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

        this->sockfd = server_sock;
        memset(&server, 0, sizeof(server));

        //bzero((char *)&server, sizeof(server));
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = INADDR_ANY;
        int portno = atoi(_port_no.c_str());
        server.sin_port = htons(portno);

        if (bind(server_sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
            perror("Error on binding");
        }

        listen(server_sock, 1024);
    }
    else {
        struct sockaddr_in server_info;
        int client_sock;

        client_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (client_sock < 0) {
            perror("Error opening socket");
        }

        this->sockfd = client_sock;
        memset(&server_info, 0, sizeof(server_info));

        //server = gethostbyname(_ip_address.c_str());
        server_info.sin_family = AF_INET;
        int portno = atoi(_port_no.c_str());
        //bzero((char *)&server_info, sizeof(server_info));
        server_info.sin_port = htons((short) portno);
        inet_aton(_ip_address.c_str(), &server_info.sin_addr);

        if (connect(this->sockfd, (struct sockaddr *) &server_info, sizeof(server_info)) < 0) {
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
    int newsockfd;
    newsockfd = accept(sockfd, nullptr, nullptr);
    return newsockfd;
}

int TCPRequestChannel::cread (void* msgbuf, int msgsize) {
    return read(sockfd, msgbuf, msgsize);
}

int TCPRequestChannel::cwrite (void* msgbuf, int msgsize) {
    return write(sockfd, msgbuf, msgsize);
}
