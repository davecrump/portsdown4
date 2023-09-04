#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h> 
#include <fcntl.h>
#include "audio.h"

int main(int argc, char *argv[])
{
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr; 

    char sendBuff[1025];
    int n;
    char rcvbuff[1024*2*sizeof(short)];

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, '0', sizeof(serv_addr));
    memset(sendBuff, '0', sizeof(sendBuff)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(5000); 

    bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)); 

    listen(listenfd, 10); 

    connfd = accept(listenfd, (struct sockaddr*)NULL, NULL); 

    /* make it non blocking */
    fcntl(connfd, F_SETFL, fcntl(connfd, F_GETFL) | O_NONBLOCK);

    audio_init();

    while(1)
    {
        if( (n = read(connfd, rcvbuff, sizeof(rcvbuff))) > 0) {
             printf("RX\n");
//            printf("RX: %i %i %i %i %i %i %i %i\n", rcvbuff[0], rcvbuff[1], rcvbuff[2], rcvbuff[3], rcvbuff[4092],
//                                                    rcvbuff[4093], rcvbuff[4094], rcvbuff[4095]);
        }

    }
    close(connfd);
}
