#ifndef __TRANSPORT_H__
#define __TRANSPORT_H__

extern char serverip[20];

void transport_init();
void transport_send(unsigned char *);

#endif /* __TRANSPORT_H__ */
