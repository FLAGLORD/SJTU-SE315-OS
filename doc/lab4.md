# LAB 4

## EXERCISE 1

在 lab 3 中，secondary CPUs 会通过循环一直 hang,系统未真正使用它们执行用户代码。而在 lab 4 中，CPU 读取`mpidr_el1`寄存器的值，`cbz x8,primary`指令根据该值来识别当前处理器是否为 primary.在 secondary CPUs 的相关代码中，它们会去检查`secondary_boot_flag`的值，如果它为0，CPUs 会一直 spin.primary进行boot后，在`start_kernel`中进入`main`函数，而在`main`中会调用`enable_smp_cores`，在其中它会设置`secondary_boot_flag`的值。secondary CPUs 检查到 flag 非零后，就会进入 boot

## EXERCISE 3

不正确。在注释掉 EXERCISE 2 中关于依次激活 secondary CPU(等待cpu_status 变为 cpu_run)的代码，`make qemu`可以发现`[INFO] AP x is activated!`的打印信息出现了混乱，因为多个 CPU 内核在同时执行内核代码

## EXERCISE 6

在`el0_syscall`调用`lock_kernel`时，exception level 并未发生改变且寄存器中的值在后续代码中仍要使用，如果不保存到栈上，值可能会被覆盖而造成错误。然而在`exception_return`中，`unlock_kernel`后调用的`exception_exit`将保存在栈上的值加载到寄存器中，恢复 context，并未使用到寄存器中的值，且之后 exception level 将发生改变，而不同的 exception level 将使用不同的寄存器。所以在`unlock_kernel`前不需要保存寄存器的值。

## EXERCISE 8

在`eret_to_thread`中`exception_return`会释放 kernel lock,而对于 IDLE  THREAD,它比较特殊，由于它运行在 kernel mode 中，如果不获取锁，在调度并`eret_to_thread`时将没有锁可以释放，从而使下次获取 kernel lock 时可能造成阻塞。

## LAB4-BONUS

`ipc_send`以及`ipc_recv`定义分别如下 

```c
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
```

```c
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
        //似乎使用yield比较好，但是这个ipc的实现毕竟是在kernel内
    }

    //重新获取锁
    lock_kernel();
    return *(shared_buf + 1);
}
```

大体思路是基于共享内存直接进行信息传递，设计上来看很像老师以前上课提到过的系统与硬件交互的早期的一种实现方式：使用 status 寄存器来传递状态，使用 data 来传递数据；而在这里共享内存 buf 的第一个八字节来传递状态，第二个用来传递数据。

我定义了两个宏，`IPC_READY`表示 receiver 可以接收数据，`IPC_DATA_AVAILABLE`表示 sender 数据已发送，receiver 可以读取。

在`ipc_recv`有两个 while，在阻塞前应该放锁。

另一个需要改动的是`/kernel/ipc/ipc_client.c`的`create_connection`，改动内容如下:

```c
conn->target = target;	
target->active_conn = conn;
target->active_conn->source = source;
```

在原来的 ipc 中 target 由函数进行创建，在此处直接设置即可；同时也应改设置`target->active_conn`以及`target->active->source`，否则`ipc_recv`会分别阻塞在第一个和第二个 while 处。

测试文件是在`/user/lab4/`目录下的`ipc_send_test`和`ipc_recv_test`,测试结果如下：

```bash
[Parent] create the receiver process.
[Sender] begin send messgage
[Sender] send message 521!
[Sender] exit
[Receiver] receive message 521
[Receiver] exit
```

