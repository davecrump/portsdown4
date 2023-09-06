#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 
#include <time.h>
#include <fcntl.h>
#include <stdbool.h>
#include "transport.h"
#include "sdr_interface.h"

static int sockfd = 0;
bool connection_made;

void transport_init()
{
    int err=0;
    connection_made = false;
    struct sockaddr_in serv_addr; 
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Error : Could not create socket \n");
        return;
    } 

    memset(&serv_addr, '0', sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(7680); 

    if(inet_pton(AF_INET, "95.154.252.125", &serv_addr.sin_addr)<=0)
    {
        printf("\n inet_pton error occured\n");
        return;
    } 

    if ((err=connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0)
    {
       printf("Error : Connect Failed %i %i ",err,errno);
       printf("connect: %s\n", strerror(errno));
       return;
    } 

    /* make it non blocking */
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);

    connection_made = true;
    printf("Connection to server established\n");
}


void transport_send(unsigned char *data) {
    int err=0;

//    if (connection_made) {
//        write(sockfd, data, TCP_DATA_PRE + FFT_BUFFER_SIZE*sizeof(short)*2);
//    }
    do {
        err = write(sockfd, data, TCP_DATA_PRE + FFT_BUFFER_SIZE*sizeof(short)*2);
        if (err<0) {
            printf("Error : Write failed %i %i ",err,errno);
            printf(" : %s\n", strerror(errno));
            usleep(10000);
        }
    } while ((err<0) && (errno==11));
    if (err<0) {
        printf("Reconnecting \n");
        transport_init();
    }
}
