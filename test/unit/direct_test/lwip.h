#ifndef _DIRECT_TEST_LWIP_H_
#define _DIRECT_TEST_LWIP_H_

typedef int err_t;

#define ERR_OK 0
#define ERR_MEM -1
#define ERR_ABRT -2
#define ERR_RST -3
#define ERR_CLSD -4

#define TCP_WRITE_FLAG_COPY 0x01

typedef struct ip_addr {
    u32 addr;
} ip_addr_t;

#define IPADDR_TYPE_ANY 0

extern ip_addr_t direct_test_ip_any;
#define IP_ANY_TYPE (&direct_test_ip_any)

struct tcp_pcb {
    int dummy;
};

struct pbuf {
    struct pbuf *next;
    u16 len;
    void *payload;
};

struct tcp_pcb *tcp_new_ip_type(u8 type);
void tcp_ref(struct tcp_pcb *pcb);
void tcp_unref(struct tcp_pcb *pcb);
void tcp_arg(struct tcp_pcb *pcb, void *arg);
void tcp_err(struct tcp_pcb *pcb, void (*err)(void *, err_t));
void tcp_recv(struct tcp_pcb *pcb, err_t (*recv)(void *, struct tcp_pcb *, struct pbuf *, err_t));
void tcp_sent(struct tcp_pcb *pcb, err_t (*sent)(void *, struct tcp_pcb *, u16));
err_t tcp_write(struct tcp_pcb *pcb, const void *data, u16 len, u8 flags);
err_t tcp_output(struct tcp_pcb *pcb);
u16 tcp_sndbuf(struct tcp_pcb *pcb);
void tcp_recved(struct tcp_pcb *pcb, u16 len);
err_t tcp_close(struct tcp_pcb *pcb);
void tcp_lock(struct tcp_pcb *pcb);
void tcp_unlock(struct tcp_pcb *pcb);
err_t tcp_bind(struct tcp_pcb *pcb, const ip_addr_t *ipaddr, u16 port);
struct tcp_pcb *tcp_listen_with_backlog(struct tcp_pcb *pcb, u8 backlog);
#define tcp_listen(pcb) tcp_listen_with_backlog(pcb, 0)
void tcp_accept(struct tcp_pcb *pcb, err_t (*accept)(void *, struct tcp_pcb *, err_t));
err_t tcp_connect(struct tcp_pcb *pcb, const ip_addr_t *ipaddr, u16 port,
                  err_t (*connected)(void *, struct tcp_pcb *, err_t));
void pbuf_free(struct pbuf *p);

#endif
