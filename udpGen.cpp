#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>

#include <thread>
#include <iostream>
using namespace std;


int main(int argc, char** argv) {

    if (argc < 4) {
        printf("Args <bind ip> <dest_ip> <port> [packetSize]\n");
        return 0;
    }

    char *adapter = argv[1];
    char *destination = argv[2];
    char *port = argv[3];

    int sendBufferSize = 1472;
    if (argc > 4) {
        sendBufferSize = atoi(argv[4]);
    }
    printf("Using buffer size of %d\n", sendBufferSize);
    //int sendBufferSize = 1600;
    char buffer[sendBufferSize];

    uint32_t bind_ip = inet_addr(argv[1]);
    uint32_t dst_ip = inet_addr(argv[2]);
    uint16_t ip_port = static_cast<uint16_t>(atoi(argv[3]));

    int socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ == -1) {
	fprintf(stderr, "failed to create socket\n");
	return -1;
    }

    int disable = 1;
    if (setsockopt(socket_, SOL_SOCKET, SO_NO_CHECK, (void*)&disable, sizeof(disable)) < 0) {
        printf("setsockopt failed\n");
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ip_port);
    addr.sin_addr.s_addr = bind_ip;  //INADDR_ANY;
    if (bind(socket_, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) != 0) {
	close(socket_);
	fprintf(stderr, "bind: %d\n", errno);
	return -1;
    }

    // set detination
    memset(&addr, 0, sizeof(struct sockaddr_in));  
    addr.sin_family = AF_INET;  
    addr.sin_addr.s_addr = dst_ip;
    addr.sin_port = htons(ip_port);
    if (connect(socket_,(struct sockaddr *)&addr,sizeof(addr)) != 0) {
	close(socket_);
	fprintf(stderr, "connect error: %d\n", errno);
	return -1;
    }

    // send some data
    printf("Sending packets to %s:%s from adapter %s\n", destination, port, adapter);
    printf("Use Crtl-C to stop\n");

    while (true) {
	send(socket_, (const char *)buffer, (int)sendBufferSize, 0);
	usleep(1000);
    }

    close(socket_);
}
