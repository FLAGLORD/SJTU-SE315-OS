#include <lib/bug.h>
#include <lib/ipc.h>
#include <lib/launcher.h>
#include <lib/syscall.h>
#include <lib/type.h>


int main(int argc, char *argv[], char *envp[])
{
	int ret;
	void *info_page_addr;
	struct info_page *info_page;

	info_page_addr = (void *)(envp[0]);
	fail_cond(info_page_addr == NULL, "[Server] no info received.\n");

	ret = ipc_register_server(0);
	fail_cond(ret < 0, "[IPC Server] register server failed\n", ret);

	info_page = (struct info_page *)info_page_addr;
	info_page->ready_flag = 1;

	while (info_page->exit_flag != 1) {
		usys_yield();
	}
	info_page->exit_flag = 0;

	ret = ipc_recv();
	

	while (info_page->exit_flag != 1) {
		usys_yield();
	}
	printf("[Receiver] receive message %d\n",ret);
	printf("[Receiver] exit\n");
	return 0;
}
