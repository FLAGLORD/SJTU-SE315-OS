/*
 * Copyright (c) 2020 Institute of Parallel And Distributed Systems (IPADS), Shanghai Jiao Tong University (SJTU)
 * OS-Lab-2020 (i.e., ChCore) is licensed under the Mulan PSL v1.
 * You can use this software according to the terms and conditions of the Mulan PSL v1.
 * You may obtain a copy of Mulan PSL v1 at:
 *   http://license.coscl.org.cn/MulanPSL
 *   THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 *   PURPOSE.
 *   See the Mulan PSL v1 for more details.
 */

// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <common/printk.h>
#include <common/types.h>

static inline __attribute__ ((always_inline))
u64 read_fp()
{
	u64 fp;
	__asm __volatile("mov %0, x29":"=r"(fp));
	return fp;
}

__attribute__ ((optimize("O1")))
int stack_backtrace()
{
	printk("Stack backtrace:\n");

	
	// Your code here.

	//fp of current function(stack_backstrce)
	u64 stackBacktraceFP = read_fp();
	//fp of the caller of current function(stack_backstrace)
	u64 callStackBackstraceFP = *(u64 *)stackBacktraceFP;

	u64 fp = callStackBackstraceFP; // value of fp;

	do{
		printk("LR %lx FP %lx Args ",*(u64 *)(fp + 8),fp);	
		u64 argAdrBegin = fp - 16; // address of argList begins at fp - 16
		for(int i = 0; i < 5; ++i){
			printk("%lx ", *(u64 *)(argAdrBegin + 8 * i));
		}
		printk("\n");
		fp = *(u64 *)fp; // value of parent fp
	}while(fp);//value of fp != 0

	return 0;
}
