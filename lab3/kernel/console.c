
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			      console.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
						    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*
	回车键: 把光标移到第一列
	换行键: 把光标前进到下一行
*/


#include "type.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "keyboard.h"
#include "proto.h"

PRIVATE void set_cursor(unsigned int position);
PRIVATE void set_video_start_addr(u32 addr);
PRIVATE void flush(CONSOLE* p_con);
PRIVATE void PushCursor(CONSOLE* p_con,unsigned int toStore);
PRIVATE unsigned int PopCursor(CONSOLE* p_con);
PRIVATE void ChangeColor(CONSOLE *p_con);
PRIVATE void SwitchTONormal(CONSOLE *p_con, u8* p_vmem);

/*======================================================================*
			   init_screen
 *======================================================================*/
PUBLIC void init_screen(TTY* p_tty)
{
	int nr_tty = p_tty - tty_table;
	p_tty->p_console = console_table + nr_tty;

	int v_mem_size = V_MEM_SIZE >> 1;	/* 显存总大小 (in WORD) */

	int con_v_mem_size                   = v_mem_size / NR_CONSOLES;
	p_tty->p_console->original_addr      = nr_tty * con_v_mem_size;
	p_tty->p_console->v_mem_limit        = con_v_mem_size;
	p_tty->p_console->current_start_addr = p_tty->p_console->original_addr;

	/* 默认光标位置在最开始处 */
	p_tty->p_console->cursor = p_tty->p_console->original_addr;

	p_tty->p_console->endOfNormal = p_tty->p_console->cursor;
    p_tty->p_console->cursor_record.index = 0;
    p_tty->p_console->char_record.index = 0;
    p_tty->p_console->char_record.endOfNormalIndex = 0;
    for(int i = 0; i < SCREEN_SIZE; i++){
        p_tty->p_console->char_record.content[i] = ' ';
    }

	p_tty->p_console->cursor_record.len = SCREEN_SIZE;

	if (nr_tty == 0) {
		/* 第一个控制台沿用原来的光标位置 */
		p_tty->p_console->cursor = disp_pos / 2;
		disp_pos = 0;
	}
	else {
		out_char(p_tty->p_console, nr_tty + '0');
		out_char(p_tty->p_console, '#');
	}

	set_cursor(p_tty->p_console->cursor);
}


/*======================================================================*
			   is_current_console
*======================================================================*/
PUBLIC int is_current_console(CONSOLE* p_con)
{
	return (p_con == &console_table[nr_current_console]);
}


/*======================================================================*
			   out_char
 *======================================================================*/
PUBLIC void out_char(CONSOLE* p_con, char ch)
{
	u8* p_vmem = (u8*)(V_MEM_BASE + p_con->cursor * 2);

	if (mode == 2 && ch == '\r') {
		mode = 0;
		p_con->char_record.index = p_con->char_record.endOfNormalIndex;
	} else if (mode == 2) {
		return;
	}

	switch(ch) {
	// '\n' for Enter
	case '\n':
		if (mode == 0 && p_con->cursor < p_con->original_addr + p_con->v_mem_limit - SCREEN_WIDTH) {
			PushCursor(p_con, p_con->cursor);
			p_con->cursor = p_con->original_addr + SCREEN_WIDTH * ((p_con->cursor - p_con->original_addr) / SCREEN_WIDTH + 1);
		} else {
			mode = 2;
			ChangeColor(p_con);
		}
		break;

	// '\b'for Backspace
	case '\b':
		if (p_con->cursor > p_con->original_addr && p_con->cursor_record.index > 0) {
			unsigned int temp = PopCursor(p_con);
            if(mode == 1 && temp < p_con->endOfNormal){
                PushCursor(p_con, temp);
                break;
            } 
			if(temp != -1){
				for (int i = 0; p_con->cursor > temp; p_con->cursor--, i++) {
					*(p_vmem - 2 * i - 2) = ' ';
                    *(p_vmem - 2 * i - 1) = DEFAULT_CHAR_COLOR;
				}
			}
		}
		break;

	// '\t' for Tab
	case '\t':
        PushCursor(p_con, p_con->cursor);
        for(int i = 0; i < 4; i++){
            *p_vmem++ = ' ';
            *p_vmem++ = BLUE; 
            p_con->cursor++;
        }
        break;

    case '\r':
        if(mode == 1){ 
            p_con->char_record.endOfNormalIndex = p_con->char_record.index;
            p_con->endOfNormal = p_con->cursor;
        } else { 
			SwitchTONormal(p_con, p_vmem);
        }
        break;

	default:
		if (p_con->cursor < p_con->original_addr + p_con->v_mem_limit - 1) {
            PushCursor(p_con, p_con->cursor);
			*p_vmem++ = ch;
			*p_vmem = mode == 0 ? DEFAULT_CHAR_COLOR : RED;
			*p_vmem++ = ch == ' ' ? DEFAULT_CHAR_COLOR : *p_vmem;
			p_con->endOfNormal = mode == 0 ? p_con->cursor : p_con->endOfNormal;
			p_con->cursor++;
		}
		break;
	}

	while (p_con->cursor >= p_con->current_start_addr + SCREEN_SIZE) {
		scroll_screen(p_con, SCR_DN);
	}

	flush(p_con);
}

/*======================================================================*
                           flush
*======================================================================*/
PRIVATE void flush(CONSOLE* p_con)
{
        set_cursor(p_con->cursor);
        set_video_start_addr(p_con->current_start_addr);
}

/*======================================================================*
			    set_cursor
 *======================================================================*/
PRIVATE void set_cursor(unsigned int position)
{
	disable_int();
	out_byte(CRTC_ADDR_REG, CURSOR_H);
	out_byte(CRTC_DATA_REG, (position >> 8) & 0xFF);
	out_byte(CRTC_ADDR_REG, CURSOR_L);
	out_byte(CRTC_DATA_REG, position & 0xFF);
	enable_int();
}

/*======================================================================*
			  set_video_start_addr
 *======================================================================*/
PRIVATE void set_video_start_addr(u32 addr)
{
	disable_int();
	out_byte(CRTC_ADDR_REG, START_ADDR_H);
	out_byte(CRTC_DATA_REG, (addr >> 8) & 0xFF);
	out_byte(CRTC_ADDR_REG, START_ADDR_L);
	out_byte(CRTC_DATA_REG, addr & 0xFF);
	enable_int();
}



/*======================================================================*
			   select_console
 *======================================================================*/
PUBLIC void select_console(int nr_console)	/* 0 ~ (NR_CONSOLES - 1) */
{
	if ((nr_console < 0) || (nr_console >= NR_CONSOLES)) {
		return;
	}

	nr_current_console = nr_console;

	set_cursor(console_table[nr_console].cursor);
	set_video_start_addr(console_table[nr_console].current_start_addr);
}

/*======================================================================*
			   scroll_screen
 *----------------------------------------------------------------------*
 滚屏.
 *----------------------------------------------------------------------*
 direction:
	SCR_UP	: 向上滚屏
	SCR_DN	: 向下滚屏
	其它	: 不做处理
 *======================================================================*/
PUBLIC void scroll_screen(CONSOLE* p_con, int direction) {
	if (direction == SCR_UP) {
		if (p_con->current_start_addr > p_con->original_addr) {
			p_con->current_start_addr -= SCREEN_WIDTH;
		}
	}
	else if (direction == SCR_DN) {
		if (p_con->current_start_addr + SCREEN_SIZE <
		    p_con->original_addr + p_con->v_mem_limit) {
			p_con->current_start_addr += SCREEN_WIDTH;
		}
	}
	else{
	}

	set_video_start_addr(p_con->current_start_addr);
	set_cursor(p_con->cursor);
}


//用栈维护光标的位置，每次变化压栈
PRIVATE void PushCursor(CONSOLE* p_con, unsigned int toStore){
    if (p_con->cursor_record.index < p_con->cursor_record.len){
        p_con->cursor_record.content[p_con->cursor_record.index] = toStore;
        p_con->cursor_record.index++;
    }
}

PRIVATE unsigned int PopCursor(CONSOLE* p_con){
    if (p_con->cursor_record.index > 0){
		p_con->cursor_record.index--;
        unsigned int toReturn = p_con->cursor_record.content[p_con->cursor_record.index];
        return toReturn;
    }
	return -1;
}

//mode 1按下enter后进行字符匹配
PRIVATE void ChangeColor(CONSOLE *p_con){
    int len1 = p_con->cursor - p_con->endOfNormal;
	int len2 = p_con->endOfNormal;
    if(len1 == 0 || len1 > len2){
        return;
    }
    u8* character;    
    u8* character_color;   
	u8* standard;   
    u8* standard_color;  
    for(int i = 0; i < len2 - len1 + 1; i++){
        int isHere = 1;
        for(int j = 0; j < len1; j++){
            character = (u8*)(V_MEM_BASE + 2 * i + 2 * j);
            character_color = (u8*)(V_MEM_BASE + 2 * i + 2 * j + 1);
            standard = (u8*)(V_MEM_BASE + len2 * 2 + 2 * j);
            standard_color = (u8*)(V_MEM_BASE + len2 * 2 + 2 * j + 1);
            if (*character != *standard) {
                isHere = 0;
                break;
            } else if (*character == ' ' && *character_color != *standard_color) {
				isHere = 0;
				break;
			}
        }
        if (isHere == 1) {
            for(int j = 0; j < len1; j++){
				*(u8*)(V_MEM_BASE + 2 * i + 2 * j + 1) = *(u8*)(V_MEM_BASE + 2 * i + 2 * j) == ' ' ? *(u8*)(V_MEM_BASE + 2 * i + 2 * j + 1) : RED;
            }
        }
    }
}

//回到正常模式，删去查找字符，改回匹配颜色
PRIVATE void SwitchTONormal(CONSOLE *p_con, u8* p_vmem) {
	if (p_con->cursor > p_con->original_addr){
        int i = 0;
        while(p_con->cursor > p_con->endOfNormal){
            unsigned int temp = PopCursor(p_con); 
			for (; p_con->cursor > temp; p_con->cursor--, i++) {
				*(p_vmem - 2 * i - 2) = ' ';
                *(p_vmem - 2 * i - 1) = DEFAULT_CHAR_COLOR;
			}
		}
    }
    for(int i = 0; i < p_con->cursor; i++){
        if (*(u8*)(V_MEM_BASE + i * 2 + 1) == RED){
            *(u8*)(V_MEM_BASE + i * 2 + 1) = DEFAULT_CHAR_COLOR;
        }
    }
}
