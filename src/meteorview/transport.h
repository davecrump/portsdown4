#ifndef __TRANSPORT_H__
#define __TRANSPORT_H__

extern char serverip[20];

int transport_init();
int transport_send(unsigned char *);

#endif /* __TRANSPORT_H__ */
