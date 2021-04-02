## Question 1:

编译阶段：在`script/linker-aarch64.lds.in`中指定了包括`img_start`和`img_end`在内的物理内存的布局

运行阶段：在`/kernel/mm/mm.c`,在`mm_init`中`free_mem_start`赋值为`img_end`,这是页面元数据的起始地址，这个值后来赋给了`page_meta_start`

```c++
free_mem_start = phys_to_virt(ROUND_UP((vaddr_t) (&img_end), PAGE_SIZE));
```

而页面的起始地址为`START_VADDR`

```c++
start_vaddr = START_VADDR;
```

`START_VADDR`宏定义如下

```c++
#define START_VADDR phys_to_virt(PHYSICAL_MEM_START)	//24M
```

## Question 2:

1. 性能：两个页表基地址使得应用程序在系统调用过程中不需要切换页表，避免了TLB刷新的开销，相比于X86映射每个进程的页表高地址区域的做法，AArch64不同进程可共用内核页表
2. 安全：X86虽然修改进程页表的高地址区域来映射内核，但本质上内核和应用程序共用页表，而AArch64使用两个页表的做法，区分内核和应用程序，有更好的隔离性

## Question 3:

### 1

物理地址

### 2

物理地址

## Question 4:

### 1

空间开销约为 8MB

通过多级页表来降低管理内存的开销。使用单级页表时，整个页表必须在物理内存中连续，其实某项没有用到也需要预留空间。但是使用多级页表时，未分配的虚拟地址空间可以不用分配页表，这允许整个页表结构中出现空洞，从而节省空间

### 2

区别：

- x86 支持段寻址模式，而 AArch64 不支持。
- x86 只有一个页表基地址期存器 CR3, 操作系统把自己映射到应用程序页表的高地址部分，与应用程序共用页表；而 AArch64 有两个TTBR 寄存器来供用户态和内核态使用

优点：

- 性能和安全性更好，具体见 Question 2

## Question 5:

需要。如果不设置页表位属性，存在内核使用的物理空间与用户态进程使用的物理空间毗邻的情况，这样可能导致用户数据覆盖内核数据以及恶意的用户态进程修改内核数据，造成崩溃或者安全隐患。

## Question 6:

### 1

块条目：使用块条目，可以减少页表的空间开销，也可以提高 TLB 命中率，加快内核的访存速度

Boot 阶段映射：Bootloader 和内核的地址空间

延迟映射：页面元数据以及可供应用程序使用的空闲物理空间

### 2

为什么：内核是来管理用户程序的，如果允许用户态程序读写内核内存，就破坏了内核态和用户态的隔离性，可能造成系统崩溃以及安全隐患

具体机制：

- 在 AArch64 中，内核态和用户态使用不同的页表
- 内核内存对应的页表项有相应的属性位来标识访问权限

## Challenge

### 1

`set_pte_flags`修改的部分代码如下,添加了针对`kind`等于`KERNEL_PTE`的特判

```c++
if (flags & VMR_WRITE)
    entry->l3_page.AP = AARCH64_PTE_AP_HIGH_RW_EL0_RW;
else if(kind == KERNEL_PTE)
    entry->l3_page.AP = AARCH64_PTE_AP_HIGH_RW_EL0_NA;
else
    entry->l3_page.AP = AARCH64_PTE_AP_HIGH_RO_EL0_RO;

if (flags & VMR_EXEC)
    entry->l3_page.UXN = AARCH64_PTE_UX;
else if(kind == KERNEL_PTE)
    entry->l3_page.UXN = AARCH64_PTE_UXN;
else
    entry->l3_page.UXN = AARCH64_PTE_UXN;
```

`AARCH64_PTE_AP_HIGH_RW_EL0_NA`是一个宏定义

```c++
//不可访问 NOT ACCESSIBLE
#define AARCH64_PTE_AP_HIGH_RW_EL0_NA     (2)
```

`map_kernel_space`如下：

```c++
void map_kernel_space(vaddr_t va, paddr_t pa, size_t len)
{
	// <lab2>
	vaddr_t *pgtbl = (vaddr_t *)get_ttbr1();
	map_range_in_pgtbl(pgtbl, va, pa, len, KERNEL_PT);
	// </lab2>
}
```

