#include <mellanox/vma_extra.h>

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

const unsigned int rtpReceiveBuffer = 4096;
const unsigned int sendBufferSize = 4096;

int join(uint32_t bind_ip, uint32_t src_ip, uint32_t grp_ip, uint16_t ip_port) {

    int socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ == -1) {
	std::cerr << "Failed to setupt socket " << errno << std::endl;
	return -1;
    }

    // Set non-blocking mode.
    unsigned long arg = 1;
    ioctl(socket_, FIONBIO, &arg);

    // Set socket to re-use address before we bind.
    if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, (const char *)&arg, sizeof(arg)) != 0) {
	close(socket_);
        fprintf(stderr, "setsockopt: %d\n", errno);
        return -1;
    }		

    // set socket buffer sizes
    if (setsockopt(socket_, SOL_SOCKET, SO_RCVBUF, (const char *)&rtpReceiveBuffer, sizeof(int)) != 0)	{
	close(socket_);
	return -1;
    }
    if (setsockopt(socket_, SOL_SOCKET, SO_SNDBUF, (const char *)&sendBufferSize, sizeof(int)) != 0) {
	close(socket_);
	return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ip_port); // was 0;              // auto select port (or use ip_port)
    addr.sin_addr.s_addr = INADDR_ANY; // was bind_ip;
    if (bind(socket_, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) != 0) {
	std::cout << "Failed to bind socket to ip " << errno << std::endl;
    }

    // Set the TTL
    int timeToLive(30);
    if (setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_TTL, (const char *) &timeToLive, sizeof(int)) != 0) {
        return -1;
    }
    
    struct ip_mreq_source imr;
    imr.imr_multiaddr.s_addr  = grp_ip;
    imr.imr_sourceaddr.s_addr = src_ip;
    imr.imr_interface.s_addr  = bind_ip;

    // This is an IGMP v3 socket option command.
    int status = setsockopt(socket_, IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP,
			   (const char*)&imr, sizeof(ip_mreq_source));
    if (status != 0) {
	std::cout << "setsockopt for IP_ADD_SOURCE_MEMBERSHIP failed " << status << " " << errno << std::endl;
        return -1;
    }
   
    return socket_;
}

int global_packet_count;

void poll(int socket_)
{
    u_long pending_data = 0;
    ioctl(socket_, FIONREAD, &pending_data);
   
    struct sockaddr_in srcaddr;
    static unsigned char buffer[rtpReceiveBuffer];

    if (pending_data > 0) {
	unsigned int fromlen = sizeof(struct sockaddr_in);
	int recvlen = recvfrom(socket_, buffer, rtpReceiveBuffer, 0, (struct sockaddr *)&srcaddr, &fromlen);
	//printf("recvlen %d\n", recvlen);
	(void)recvlen;
	global_packet_count++;
    }
}

int main(int argc, char** argv) {

    struct vma_api_t* vma_api;
    vma_api = vma_get_api();
    //cout << "vma_api = " << vma_api << endl;
    if (vma_api) {
	printf("Running with VMA\n");
    } else {
	printf("Not using VMA\n");
    }

    if(argc < 4)
    {
        printf("Args <bind ip> <source_ip> <multicast ip> <port>\n");
        return 0;
    }

    uint32_t bind_ip = inet_addr(argv[1]);
    uint32_t src_ip = inet_addr(argv[2]);
    uint32_t grp_ip = inet_addr(argv[3]);
    uint16_t ip_port = static_cast<uint16_t>(atoi(argv[4]));
    
    int socket_ = join(bind_ip, src_ip, grp_ip, ip_port);
    if (socket_ < 0) {
	printf("IGMP join failed\n");
	return 0;
    }

    printf("socket_ is %d\n", socket_);
    global_packet_count = 0;

    /*
    printf("Callig  poll for 10 seconds\n");
    for (int i = 0; i<1000; i++) {
	poll(socket_);
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    printf("In 10 seconds got %d packets\n", global_packet_count);
    */

    // use thread to read from socket
    bool stop(false);
    thread t([&]() {
        while (!stop) {
	    poll(socket_);
        }
    });
 
    //printf("Press <Return> to terminate\n");
    //getchar();
    printf("Waiting for 10 seconds\n");
    std::this_thread::sleep_for(std::chrono::seconds(10));
    printf("\nStopping...\n");
    stop = true;
    t.join();

    printf("global_packet_count = %d\n", global_packet_count);

    return 0;
}
