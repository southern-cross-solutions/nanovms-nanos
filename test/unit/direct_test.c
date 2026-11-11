#include "../test_utils.h"
#include <kernel.h>
#include <lwip.h>
#include "../../src/net/direct.c"

#define MAX_ASYNC 8
#define MAX_FREES 64

static thunk async_queue[MAX_ASYNC];
static int async_count;

static void *freed[MAX_FREES];
static int freed_count;

typedef struct tracking_heap {
    struct heap h;
    heap parent;
} *tracking_heap;

static u64 tracking_alloc(heap h, bytes b)
{
    tracking_heap th = (tracking_heap)h;
    return th->parent->alloc(th->parent, b);
}

static void tracking_dealloc(heap h, u64 a, bytes b)
{
    tracking_heap th = (tracking_heap)h;
    if (freed_count < MAX_FREES)
        freed[freed_count++] = (void *)a;
    th->parent->dealloc(th->parent, a, b);
}

static boolean was_freed(void *p)
{
    for (int i = 0; i < freed_count; i++)
        if (freed[i] == p)
            return true;
    return false;
}

static int freed_index(void *p)
{
    for (int i = 0; i < freed_count; i++)
        if (freed[i] == p)
            return i;
    return -1;
}

static int freed_count_of(void *p)
{
    int count = 0;
    for (int i = 0; i < freed_count; i++)
        if (freed[i] == p)
            count++;
    return count;
}

void direct_test_async_apply(thunk t)
{
    if (async_count < MAX_ASYNC)
        async_queue[async_count++] = t;
}

void direct_test_async_apply_1(void *a, void *arg0)
{
    (void)a;
    (void)arg0;
}

ip_addr_t direct_test_ip_any;

static struct tcp_pcb fake_pcb;

struct tcp_pcb *tcp_new_ip_type(u8 type)
{
    (void)type;
    return &fake_pcb;
}

void tcp_ref(struct tcp_pcb *pcb)
{
    (void)pcb;
}

void tcp_unref(struct tcp_pcb *pcb)
{
    (void)pcb;
}

void tcp_arg(struct tcp_pcb *pcb, void *arg)
{
    (void)pcb;
    (void)arg;
}

void tcp_err(struct tcp_pcb *pcb, void (*err)(void *, err_t))
{
    (void)pcb;
    (void)err;
}

void tcp_recv(struct tcp_pcb *pcb, err_t (*recv)(void *, struct tcp_pcb *, struct pbuf *, err_t))
{
    (void)pcb;
    (void)recv;
}

void tcp_sent(struct tcp_pcb *pcb, err_t (*sent)(void *, struct tcp_pcb *, u16))
{
    (void)pcb;
    (void)sent;
}

err_t tcp_write(struct tcp_pcb *pcb, const void *data, u16 len, u8 flags)
{
    (void)pcb;
    (void)data;
    (void)len;
    (void)flags;
    return ERR_OK;
}

err_t tcp_output(struct tcp_pcb *pcb)
{
    (void)pcb;
    return ERR_OK;
}

u16 tcp_sndbuf(struct tcp_pcb *pcb)
{
    (void)pcb;
    return 0;
}

void tcp_recved(struct tcp_pcb *pcb, u16 len)
{
    (void)pcb;
    (void)len;
}

err_t tcp_close(struct tcp_pcb *pcb)
{
    (void)pcb;
    return ERR_OK;
}

void tcp_lock(struct tcp_pcb *pcb)
{
    (void)pcb;
}

void tcp_unlock(struct tcp_pcb *pcb)
{
    (void)pcb;
}

err_t tcp_bind(struct tcp_pcb *pcb, const ip_addr_t *ipaddr, u16 port)
{
    (void)pcb;
    (void)ipaddr;
    (void)port;
    return ERR_OK;
}

struct tcp_pcb *tcp_listen_with_backlog(struct tcp_pcb *pcb, u8 backlog)
{
    (void)backlog;
    return pcb;
}

void tcp_accept(struct tcp_pcb *pcb, err_t (*accept)(void *, struct tcp_pcb *, err_t))
{
    (void)pcb;
    (void)accept;
}

err_t tcp_connect(struct tcp_pcb *pcb, const ip_addr_t *ipaddr, u16 port,
                  err_t (*connected)(void *, struct tcp_pcb *, err_t))
{
    (void)pcb;
    (void)ipaddr;
    (void)port;
    (void)connected;
    return ERR_OK;
}

void pbuf_free(struct pbuf *p)
{
    (void)p;
}

closure_struct(input_buffer_handler, fake_ibh);
closure_struct(connection_handler, fake_new);

closure_func_basic(input_buffer_handler, boolean, fake_ibh_func, buffer b)
{
    (void)b;
    return false;
}

closure_func_basic(connection_handler, input_buffer_handler, fake_new_func, buffer_handler out)
{
    (void)out;
    return (input_buffer_handler)&fake_ibh;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    heap parent = init_process_runtime();
    test_assert(parent != INVALID_ADDRESS);

    struct tracking_heap th = {0};
    th.parent = parent;
    th.h.alloc = tracking_alloc;
    th.h.dealloc = tracking_dealloc;
    th.h.pagesize = parent->pagesize;

    init_closure_func(&fake_ibh, input_buffer_handler, fake_ibh_func);
    init_closure_func(&fake_new, connection_handler, fake_new_func);

    direct d = direct_alloc(&th.h, (connection_handler)&fake_new);
    test_assert(d != INVALID_ADDRESS);
    direct_conn dc = direct_conn_alloc(d, d->p);
    test_assert(dc != INVALID_ADDRESS);
    test_assert(d->p == 0);
    test_assert(async_count == 1);

    thunk service = async_queue[0];
    async_count = 0;

    qbuf q = allocate(d->h, sizeof(struct qbuf));
    test_assert(q != INVALID_ADDRESS);
    q->b = 0;
    direct_conn_send_internal(dc, q, false);

    test_assert(!was_freed(dc));
    test_assert(!was_freed(d));
    test_assert(!d->closed);

    apply(service);

    int dc_index = freed_index(dc);
    int d_index = freed_index(d);
    test_assert(dc_index >= 0);
    test_assert(d_index >= 0);
    test_assert(dc_index < d_index);
    test_assert(freed_count_of(dc) == 1);
    test_assert(freed_count_of(d) == 1);

    return 0;
}
