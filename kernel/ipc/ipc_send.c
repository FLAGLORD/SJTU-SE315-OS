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


u64 sys_ipc_send(u32 conn_cap, u64 msg)
{
	struct ipc_connection *conn = NULL;
	int r;

	conn = obj_get(current_thread->process, conn_cap, TYPE_CONNECTION);
	if (!conn) {
		r = -ECAPBILITY;
		goto out_fail;
	}

    //基于共享内存直接传递消息
    u64 *shared_buf = (u64 *)conn->buf.client_user_addr;
    if(*shared_buf != IPC_READY){
        r = -EAGAIN;
        goto out_fail;
    }
    //在第二个字节处放置传递的消息
    *(shared_buf + 1) = msg;

    //数据可用
    *shared_buf = IPC_DATA_AVAILABLE;

    return 0;

	BUG("This function should never\n");
 out_fail:
	return r;
}