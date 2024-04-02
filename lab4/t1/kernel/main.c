
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                            main.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                                    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include "type.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "proto.h"


int strategy;
int delay_time;
int t;
PUBLIC void print_status(PROCESS* p);


PRIVATE void init_tasks()
{
	init_screen(tty_table);
	clean(console_table);

	// 表驱动，对应进程0, 1, 2, 3, 4, 5, 6
	int prior[7] = {1, 1, 1, 1, 1, 1, 1};
	for (int i = 0; i < 7; ++i) {
        proc_table[i].ticks    = prior[i];
        proc_table[i].priority = prior[i];
	}

	// initialization
	k_reenter = 0;
	ticks = 0;
	reader_count = 0;
	writers = 0;
	writing = 0;

	strategy = 1; // 切换策略，读写公平为0,读者优先为1,写者优先为2

    delay_time = strategy == 2 ? 1 : 0;
    t = 1;

	p_proc_ready = proc_table;
}
/*======================================================================*
                            kernel_main
 *======================================================================*/
PUBLIC int kernel_main()
{
	disp_str("-----\"kernel_main\" begins-----\n");

	TASK*		p_task		= task_table;
	PROCESS*	p_proc		= proc_table;
	char*		p_task_stack	= task_stack + STACK_SIZE_TOTAL;
	u16		selector_ldt	= SELECTOR_LDT_FIRST;
	int i;
    u8              privilege;
    u8              rpl;
    int             eflags;
	for (i = 0; i < NR_TASKS + NR_PROCS; i++) {
        if (i < NR_TASKS) {     /* 任务 */
                        p_task    = task_table + i;
                        privilege = PRIVILEGE_TASK;
                        rpl       = RPL_TASK;
                        eflags    = 0x1202; /* IF=1, IOPL=1, bit 2 is always 1 */
                }
                else {                  /* 用户进程 */
                        p_task    = user_proc_table + (i - NR_TASKS);
                        privilege = PRIVILEGE_USER;
                        rpl       = RPL_USER;
                        eflags    = 0x202; /* IF=1, bit 2 is always 1 */
                }
                
		strcpy(p_proc->p_name, p_task->name);	// name of the process
		p_proc->pid = i;			// pid
		p_proc->sleeping = 0; // 初始化结构体新增成员
		p_proc->blocked = 0;
        p_proc->issleep = 0;
		
		p_proc->ldt_sel = selector_ldt;

		memcpy(&p_proc->ldts[0], &gdt[SELECTOR_KERNEL_CS >> 3],
		       sizeof(DESCRIPTOR));
		p_proc->ldts[0].attr1 = DA_C | privilege << 5;
		memcpy(&p_proc->ldts[1], &gdt[SELECTOR_KERNEL_DS >> 3],
		       sizeof(DESCRIPTOR));
		p_proc->ldts[1].attr1 = DA_DRW | privilege << 5;
		p_proc->regs.cs	= (0 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.ds	= (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.es	= (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.fs	= (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.ss	= (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.gs	= (SELECTOR_KERNEL_GS & SA_RPL_MASK) | rpl;

		p_proc->regs.eip = (u32)p_task->initial_eip;
		p_proc->regs.esp = (u32)p_task_stack;
		p_proc->regs.eflags = eflags;

		p_proc->nr_tty = 0;

		p_task_stack -= p_task->stacksize;
		p_proc++;
		p_task++;
		selector_ldt += 1 << 3;
	}

	init_tasks();
	init_clock();
    init_keyboard();
	restart();

	while(1){}
}

PRIVATE read_proc(char proc, int slices){
    sleep_ms(slices * TIME_SLICE,0); // 读耗时slices个时间片
}

PRIVATE	write_proc(char proc, int slices){
    sleep_ms(slices * TIME_SLICE,0); // 写耗时slices个时间片
}

//读写公平方案
void read_fair(char proc, int slices){
	P(&wait_mutex); //读写公平
    P(&read_count_mutex); //最大写入数量
	P(&read_mutex); // 更新readcount时互斥
	if (reader_count == 0) {
		P(&read_write_mutex); // 有读者正在使用，写者不可抢占
    }
	reader_count++;
	V(&read_mutex);
	V(&wait_mutex);
	read_proc(proc, slices);
	P(&read_mutex);
	reader_count--;
	if (reader_count == 0) {
		V(&read_write_mutex); // 没有读者时，可以开始写了
    }
	V(&read_mutex);
    V(&read_count_mutex);
}

void write_fair(char proc, int slices){
	P(&wait_mutex); //读写公平
	P(&read_write_mutex);// 有写者正在使用，读者不可抢占
	writing = 1;
	V(&wait_mutex);
	// 写过程
	write_proc(proc, slices);
	writing = 0;
	V(&read_write_mutex);
}

// 读者优先
void read_rfirst(char proc, int slices){
    P(&read_count_mutex);//设置最大读人数
    P(&read_mutex); // 更新readcount时互斥
    if (reader_count==0) {
        P(&read_write_mutex); // 实现读者优先
    }
    reader_count++;
    V(&read_mutex);
    // 读过程
    read_proc(proc, slices);
    P(&read_mutex);
    reader_count--;
    if (reader_count==0) {
        V(&read_write_mutex); // 没有读者，可以开始写了
    }
    V(&read_mutex);
    V(&read_count_mutex);

}

void write_rfirst(char proc, int slices){
    P(&read_write_mutex); //读写互斥
    writing = 1;
    // 写过程
    write_proc(proc, slices);
    writing = 0;
    V(&read_write_mutex);
}

// 写者优先
void read_wfirst(char proc, int slices){
    P(&read_count_mutex); //最多同时写入
    P(&wait_mutex); //写着优先
    P(&read_mutex); // 更新readcount时互斥
    if (reader_count==0) {
        P(&read_write_mutex); 
    }
    reader_count++;
    V(&read_mutex);
    V(&wait_mutex);
    //读过程开始
    read_proc(proc, slices);
    P(&read_mutex);
    reader_count--;
    if (reader_count==0) {
        V(&read_write_mutex); // 没有读者，可以开始写了
    }
    V(&read_mutex);
    V(&read_count_mutex);
}

void write_wfirst(char proc, int slices){
    P(&write_mutex); //写writer互斥
    // 写过程
    if (writers == 0) {
        P(&wait_mutex); //实现写着优先
    }
    writers++;
    V(&write_mutex);

    P(&read_write_mutex); //保证读者写者互斥
    writing = 1;
    write_proc(proc, slices);
    writing = 0;
    V(&read_write_mutex);

    P(&write_mutex);
    writers--;
    if (writers == 0) {
        V(&wait_mutex);
    }
    V(&write_mutex);
}

read_f read_funcs[3] = {read_fair, read_rfirst, read_wfirst};
write_f write_funcs[3] = {write_fair, write_rfirst, write_wfirst};

/*======================================================================*
                               Reporter
 *======================================================================*/
void ReporterA()
{
    sleep_ms(TIME_SLICE,0);
    char white = '\06';
    int time = 1;
    while (1) {
        if (time < 21){
            if (time < 10){
				printf("%c%c  ", white, time + '0');
			}else{
				int temp1 = time / 10;
				int temp2 = time - temp1 * 10;
                printf("%c%c%c ", white, temp1 + '0', temp2 + '0');
			}
            for (PROCESS* p = proc_table+NR_TASKS+1; p < proc_table+NR_TASKS+NR_PROCS; p++) {
                if (strategy == 2 && time == 1 && p < proc_table + 5){
                    p->issleep = 0;
                    printf("%cX ", '\04');
                } else {
                    print_status(p);
                }
                
            }
            printf("\n");
        }
        sleep_ms(TIME_SLICE,0);
        time++;
    }
}

/*======================================================================*
                               ReaderB
 *======================================================================*/
void ReaderB()
{
    sleep_ms(delay_time * TIME_SLICE,1);
	while(1){
		read_funcs[strategy]('B', 2);
        sleep_ms(t * TIME_SLICE,1);
	}
}

/*======================================================================*
                               ReaderC
 *======================================================================*/
void ReaderC()
{
    sleep_ms(delay_time * TIME_SLICE,1);
	while(1){
		read_funcs[strategy]('C', 3);
        sleep_ms(t * TIME_SLICE,1);
	}
}

/*======================================================================*
                               ReaderD
 *======================================================================*/
void ReaderD()
{
    sleep_ms(delay_time * TIME_SLICE,1);
    while(1){
        read_funcs[strategy]('D', 3);
        sleep_ms(t * TIME_SLICE,1);
    }
}

/*======================================================================*
                               WriterE
 *======================================================================*/
void WriterE()
{
	while(1){
		write_funcs[strategy]('E', 3);
        sleep_ms(t * TIME_SLICE,1);
	}
}

/*======================================================================*
                               WriterF
 *======================================================================*/
void WriterF()
{
    while(1){
        write_funcs[strategy]('F', 4);
        sleep_ms(t * TIME_SLICE,1);
    }
}

PUBLIC void print_status(PROCESS* p){
    char blue = '\03';
	char green = '\02';
	char red = '\04';
    if (p->blocked){
        printf("%cX ", red);
        return;
    }
    if (p->issleep == 1){
        printf("%cZ ", blue);
        return;
    }
    if (p->issleep == 2){
        p->issleep = 0;
        printf("%cZ ", blue);
        return;
    }

    printf("%cO ", green);
}