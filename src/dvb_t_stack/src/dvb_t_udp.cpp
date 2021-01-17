/*
	Simple udp server
*/
#include <stdio.h>	//printf
#include <string.h> //memset
#include <stdlib.h> //exit(0);
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <queue>
#include "dvb_t.h"

using namespace std;

#define BUFLEN 512	//Max length of buffer
#define PORT 1314	//The port on which to listen for incoming data

static struct sockaddr_in m_me;
static struct sockaddr_in m_other;
static int m_s;
static pthread_t m_tid[1];
static bool m_ts_running;
static pthread_mutex_t m_mutex;

static queue <uint8_t> m_ts_q;

int ts_q_len(void){
	return m_ts_q.size();
 }
void ts_q_push(uint8_t val){
	m_ts_q.push(val);
}
int ts_q_read(uint8_t *b, uint32_t len){
	int l = 0;
    pthread_mutex_lock( &m_mutex );
	if(m_ts_q.size() >= len){
		for( uint32_t i = 0; i < len; i++){
			b[i] = m_ts_q.front();
			m_ts_q.pop();
		}
		l = len;
	}
    pthread_mutex_unlock( &m_mutex );
	return l;
}
void* ts_thread(void *arg)
{
	m_ts_running = false;
	DVBTFormat *fmt = (DVBTFormat*)arg;

	//create a UDP socket
	if ((m_s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) > 0){
		// zero out the structure
		memset((char *) &m_me, 0, sizeof(m_me));

		m_me.sin_family = AF_INET;
		m_me.sin_port = htons(fmt->port);
		m_me.sin_addr.s_addr = htonl(INADDR_ANY);
		int reuse = 1;
		setsockopt(m_s, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));
		//bind socket to port
		if( bind(m_s , (struct sockaddr*)&m_me, sizeof(m_me) ) == 0){
			m_ts_running = true;
		}else{
			printf("Failed to bind socket\n");
		}
	}else{
		printf("Socket Create Failed\n");
	}
	int recv_len;
	socklen_t slen;
	uint8_t ts[188*7];
	while(m_ts_running == true){
		if ((recv_len = recvfrom(m_s, ts, 188*7, 0, (struct sockaddr *) &m_other, &slen)) > 0)
		{
			if(ts[0] != 0x47)
			{
				recvfrom(m_s, ts, 1, 0, (struct sockaddr *) &m_other, &slen);
			}else{
				// Valid buffer, push it to the queue
			    pthread_mutex_lock( &m_mutex );
				for( int i = 0; i < recv_len; i++) m_ts_q.push(ts[i]);
			    pthread_mutex_unlock( &m_mutex );
			}
		}
	}
	close(m_s);
	return arg;
}

int dvb_t_udp_open(DVBTFormat *fmt){
	m_mutex =  PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

	if(pthread_create(&(m_tid[0]), NULL, &ts_thread, fmt)!=0){
        printf("TS thread cannot be started\n");
        return -1;
    }
	printf("Thread Started\n");
	return 0;
}
void dvb_t_udp_close(void){
	m_ts_running = false;
}





/*
		//print details of the client/peer and the data received
		printf("Received packet from %s:%d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
		printf("Data: %s\n" , buf);

		//now reply the client with the same data
		if (sendto(s, buf, recv_len, 0, (struct sockaddr*) &si_other, slen) == -1)
		{
			die("sendto()");
		}
	}

	close(s);
	return 0;
}
*/
