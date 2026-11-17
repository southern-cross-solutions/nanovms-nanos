#include <kernel.h>
#include <lwip.h>

#define DIRECT_CLOSE_SERVER     IPADDR4_INIT_BYTES(10, 0, 2, 2)
#define DIRECT_CLOSE_PORT       8081
#define RETRY_INTERVAL_MS       100
#define MAX_RETRIES             100
#define DIRECT_CLOSE_ITERATIONS 20

typedef struct direct_close_test {
    heap h;
    status_handler complete;
    struct timer retry_timer;
    closure_struct(timer_handler, retry);
    closure_struct(connection_handler, ch);
    closure_struct(input_buffer_handler, ibh);
    buffer_handler out;
    boolean connecting;
    boolean got_data;
    int retries;
    int remaining;
} *direct_close_test;

static direct_close_test dct;

static void direct_close_connect(direct_close_test t);

static void direct_close_complete(status s)
{
    direct_close_test t = dct;
    if (!t)
        return;
    dct = 0;
    apply(t->complete, s);
    deallocate(t->h, t, sizeof(*t));
}

static void direct_close_schedule_retry(direct_close_test t)
{
    t->connecting = false;
    if (++t->retries > MAX_RETRIES) {
        direct_close_complete(timm("result", "direct close test: connection retries exhausted"));
        return;
    }
    register_timer(kernel_timers, &t->retry_timer, CLOCK_ID_MONOTONIC,
                   milliseconds(RETRY_INTERVAL_MS), false, 0,
                   (timer_handler)&t->retry);
}

closure_func_basic(timer_handler, void, direct_close_retry_handler,
                   u64 expiry, u64 overruns)
{
    if (overruns == timer_disabled)
        return;
    direct_close_test t = struct_from_field(closure_self(), direct_close_test, retry);
    direct_close_connect(t);
}

static void direct_close_connect(direct_close_test t)
{
    if (t->connecting)
        return;
    t->connecting = true;
    ip_addr_t addr = DIRECT_CLOSE_SERVER;
    struct netif *n = ip_route(&ip_addr_any, &addr);
    if (!n) {
        direct_close_schedule_retry(t);
        return;
    }
    netif_unref(n);
    status s = direct_connect(t->h, &addr, DIRECT_CLOSE_PORT,
                              (connection_handler)&t->ch);
    if (!is_ok(s)) {
        timm_dealloc(s);
        direct_close_schedule_retry(t);
    }
}

closure_func_basic(input_buffer_handler, boolean, direct_close_ibh,
                   buffer data)
{
    direct_close_test t = struct_from_closure(direct_close_test, ibh);
    if (data) {
        if (!t->got_data) {
            t->got_data = true;

            /* Close from the receive path so that the send-side close runs while the receive
             * service is active. */
            apply(t->out, 0);
        }
        return false;
    }
    if (t->got_data) {
        t->got_data = false;
        if (--t->remaining == 0) {
            rprintf("direct close test: OK\n");
            direct_close_complete(STATUS_OK);
        } else {
            direct_close_connect(t);
        }
    } else {
        msg_err("%s: connection closed before response data", func_ss);
        direct_close_complete(timm("result", "direct close test: no response data"));
    }
    return true;
}

closure_func_basic(connection_handler, input_buffer_handler, direct_close_ch,
                   buffer_handler out)
{
    direct_close_test t = struct_from_closure(direct_close_test, ch);
    if (!out) {
        direct_close_schedule_retry(t);
        return INVALID_ADDRESS;
    }
    t->connecting = false;
    t->out = out;
    buffer b = allocate_buffer(t->h, 128);
    if (b == INVALID_ADDRESS) {
        apply(out, 0);
        direct_close_complete(timm_oom);
        return INVALID_ADDRESS;
    }
    buffer_write_sstring(b, ss("GET / HTTP/1.1\r\nHost: 10.0.2.2\r\nConnection: close\r\n\r\n"));
    status s = apply(out, b);
    if (!is_ok(s)) {
        deallocate_buffer(b);
        timm_dealloc(s);
        apply(out, 0);
        direct_close_complete(timm("result", "direct close test: failed to send request"));
        return INVALID_ADDRESS;
    }
    return (input_buffer_handler)&t->ibh;
}

int init(status_handler complete)
{
    heap h = heap_locked(get_kernel_heaps());
    direct_close_test t = allocate(h, sizeof(*t));
    if (t == INVALID_ADDRESS)
        return KLIB_INIT_FAILED;
    zero(t, sizeof(*t));
    t->h = h;
    t->complete = complete;
    t->remaining = DIRECT_CLOSE_ITERATIONS;
    init_timer(&t->retry_timer);
    init_closure_func(&t->retry, timer_handler, direct_close_retry_handler);
    init_closure_func(&t->ch, connection_handler, direct_close_ch);
    init_closure_func(&t->ibh, input_buffer_handler, direct_close_ibh);
    dct = t;
    direct_close_connect(t);
    return KLIB_INIT_IN_PROGRESS;
}
