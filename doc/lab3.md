# LAB 3

## exercise  2

>`process_create_root`
>
>>`ramdisk_read_file`
>>
>>`process_create`
>>
>>>`processs_init`
>>>
>>>`vmspace_init`
>>
>>`thread_create_main`
>>
>>> `pmo_init`
>>>
>>> `vmspace_map_range`
>>>
>>> `load_binary`
>>>
>>> > `elf_parse_file`
>>> >
>>> > `pmo_init`
>>> >
>>> > `vmspace_map_range`
>>>
>>> `prepare_env`
>>>
>>> `	thread_init`
>>>
>>> > `create_thread_ctx`
>>> >
>>> > `init_thread_ctx`
>>>
>>> `flush_idcache`

1. 使用`ramdisk_read_file`读取二进制文件
2. `process_create`中创建进程并为其分配内存空间，并完成各自结构的初始化
3. 调用`thread_create_main`为根进程创建 main thread.在其中，为 thread 分配并设置 user stack，将对应的物理地址空间映射至  vmspace.接着调用`load_binary`,解析文件结构 ,并根据 program header以 segment 为单位逐段映射到 vmspace.在`thread_init`中创建 thread control  block 并完成初始化
4. 将线程加入 ready queue

## exercise 4

system call 会触发 `sync_el0_64`异常.在异常处理函数中，`esr_el1`中的值会通过`lsr`来获取前六位的值来分辨 exception class，如果 exception causes为`svc`，那么会进入`el0_syscall`,根据系统调用号来计算函数地址，最后调用函数

## exercise 7

```gdb
(gdb) n
Single stepping until exit from function sync_el0_64,
which has no line number information.
0x0000000000000000 in ?? ()
```

`START `不是被调用的子函数，所以当它返回时，pc被置为0