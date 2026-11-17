#ifndef _DIRECT_TEST_KERNEL_H_
#define _DIRECT_TEST_KERNEL_H_

#include <runtime.h>

void direct_test_async_apply(thunk t);
void direct_test_async_apply_1(void *a, void *arg0);

#define async_apply(t) direct_test_async_apply(t)
#define async_apply_1(a, arg0) direct_test_async_apply_1(a, arg0)

#endif
