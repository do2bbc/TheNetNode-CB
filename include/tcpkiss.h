#ifndef TNN_TCPKISS_H
#define TNN_TCPKISS_H

#define KISS_TCP 18

BOOLEAN tcpkiss_attach(void);
BOOLEAN tcpkiss_detach(int port);
void tcpkiss_dump(MBHEAD *mbp, int port);
void tcpkiss_poll(void);
void tcpkiss_exit(void);

#endif
