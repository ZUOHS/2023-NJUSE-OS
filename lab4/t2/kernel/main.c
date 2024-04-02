
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
	readers = 0;
	writers = 0;
	writing = 0;


    empty.value = 1;
    count_A.value = 0;
    count_B.value = 0;

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

PRIVATE read_proc(){
    p_proc_ready->sum++;
    sleep_ms(TIME_SLICE,0); 
}

PRIVATE	write_proc(){
    p_proc_ready->sum++;
    sleep_ms(TIME_SLICE,0); 
}


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
            for(int j = NR_TASKS + 1; j < NR_PROCS + NR_TASKS; j++){
				int num = (proc_table + j)->sum;
				printf("%c%c%c ", '\06',num / 10 + '0', num % 10 + '0');
			}
            printf("\n");
        }
        sleep_ms(TIME_SLICE,0);
        time++;
    }
}
void WriterE()
{
	while(1){
        P(&empty);
        write_proc();
        V(&count_A);
	}
}

void WriterF()
{
    while(1){
        P(&empty);
        write_proc();
        V(&count_B);
    }
}

void ReaderB()
{

	while(1){
        P(&count_A);
        read_proc();
        V(&empty);
	}
}

void ReaderC()
{

	while(1){
        P(&count_B);
        read_proc();
        V(&empty);
	}
}

void ReaderD()
{

    while(1){
        P(&count_B);
        read_proc();
        V(&empty);
    }
}

