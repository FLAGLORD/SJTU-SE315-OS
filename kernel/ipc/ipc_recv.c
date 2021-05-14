#include <common/errno.h>
#include <common/kprint.h>
#include <common/macro.h>
#include <common/util.h>
#include <ipc/ipc.h>
#include <exception/exception.h>
#include <common/kmalloc.h>
#include <common/mm.h>
#include <common/uaccess.h>
#include <process/thread.h>
#include <sched/context.h>
#include <sched/sched.h>
#include <common/lock.h>


u64 sys_ipc_recv()
{
    //阻塞时应该放锁
    unlock_kernel();
    while(current_thread->active_conn == NULL){
        kinfo("revcevive");
    }

    volatile u64 *shared_buf = (u64 *)current_thread->active_conn->buf.server_user_addr;
    // 设置为READY
    *shared_buf = IPC_READY;

    while (*shared_buf != IPC_DATA_AVAILABLE) {
        //似乎使用yield比较号，但是这个ipc的实现毕竟是在kernel内
    }

    //重新获取锁
    lock_kernel();
    return *(shared_buf + 1);
}