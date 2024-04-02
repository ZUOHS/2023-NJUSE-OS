### 1. 包装系统调用

```c++
PUBLIC void sys_sleep(int milli_sec,int type)
{
	int ticks = milli_sec / 1000 * HZ * 10;  // 乘10是为了修正系统的时钟中断错误
	p_proc_ready->sleeping = ticks;
    p_proc_ready->issleep = type;
    schedule();
}

PUBLIC void sys_write_str(char* buf, int len)
{
    CONSOLE* p_con = console_table;
	for (int i = 0;i < len; i++){
		out_char(p_con, buf[i]);
	}
}
```

### 2. 信号量与PV操作

```c++
PUBLIC void p_proc(SEMAPHORE* s){
	disable_int();
    s->value--;
    if (s->value < 0){
        p_proc_ready->blocked = 1;
        s->p_list[s->tail] = p_proc_ready;
        s->tail = (s->tail + 1) % NR_PROCS;
        schedule();
    }
	enable_int();
}


PUBLIC void v_proc(SEMAPHORE* s){
	disable_int();
    s->value++;
    if (s->value <= 0){
        s->p_list[s->head]->blocked = 0; 
        s->head = (s->head + 1) % NR_PROCS;
    }
	enable_int();
}
```

### 3. 读者写者问题

#### 3.1 三种策略

分别设计读者优先、写者优先以及读写公平，通过表驱动来实现不同的策略

#### 3.2 具体实现

```c++
void read_fair(char proc, int slices){
	P(&wait_mutex);
    P(&read_count_mutex);
	P(&read_mutex);
	if (reader_count == 0) {
		P(&read_write_mutex); 
    }
	reader_count++;
	V(&read_mutex);
	V(&wait_mutex);
	read_proc(proc, slices);
	P(&read_mutex);
	reader_count--;
	if (reader_count == 0) {
		V(&read_write_mutex);
    }
	V(&read_mutex);
    V(&read_count_mutex);
}

void write_fair(char proc, int slices){
	P(&wait_mutex);
	P(&read_write_mutex);
	writing = 1;
	V(&wait_mutex);
	write_proc(proc, slices);
	writing = 0;
	V(&read_write_mutex);
}

void read_rfirst(char proc, int slices){
    P(&read_count_mutex);
    P(&read_mutex); 
    if (reader_count==0) {
        P(&read_write_mutex); 
    }
    reader_count++;
    V(&read_mutex);
    read_proc(proc, slices);
    P(&read_mutex);
    reader_count--;
    if (reader_count==0) {
        V(&read_write_mutex);
    }
    V(&read_mutex);
    V(&read_count_mutex);

}

void write_rfirst(char proc, int slices){
    P(&read_write_mutex);
    writing = 1;
    write_proc(proc, slices);
    writing = 0;
    V(&read_write_mutex);
}

void read_wfirst(char proc, int slices){
    P(&read_count_mutex);
    P(&wait_mutex);
    P(&read_mutex);
    if (reader_count==0) {
        P(&read_write_mutex);
    }
    reader_count++;
    V(&read_mutex);
    V(&wait_mutex);
    read_proc(proc, slices);
    P(&read_mutex);
    reader_count--;
    if (reader_count==0) {
        V(&read_write_mutex);
    }
    V(&read_mutex);
    V(&read_count_mutex);
}

void write_wfirst(char proc, int slices){
    P(&write_mutex);
    if (writers == 0) {
        P(&wait_mutex);
    }
    writers++;
    V(&write_mutex);
    P(&read_write_mutex);
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

```

#### 3.3 reporter

在每个时间片最后，reporter会打印出一行状态。将reporter在最开始时休息一个时间片再打印，每打印一次后休息一个时间片

#### 3.4 运行结果

##### 3.4.1 读者优先，最大3读者，休息1个时间片

![读优先3读1休](https://haosn.oss-cn-beijing.aliyuncs.com/typora/%E8%AF%BB%E4%BC%98%E5%85%883%E8%AF%BB1%E4%BC%91.png)

##### 3.4.2 写者优先，最大3读者，休息1个时间片

![写优先1休](https://haosn.oss-cn-beijing.aliyuncs.com/typora/%E5%86%99%E4%BC%98%E5%85%881%E4%BC%91.png)

#### 3.4.3 读写公平，最大3读者，休息1个时间片

![公平3读1休](https://haosn.oss-cn-beijing.aliyuncs.com/typora/%E5%85%AC%E5%B9%B33%E8%AF%BB1%E4%BC%91.png)

### 4. 生产者消费者问题

#### 4.1 信号量的设计

分别设计empty、count_A、count_B三个信号量表示空仓库数量、仓库中A产品数量、仓库中B产品数量

#### 4.2 具体实现

```c++
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
```

#### 4.3 reporter

在每个时间片最后，reporter会打印出一行状态。将reporter在最开始时休息一个时间片再打印，每打印一次后休息一个时间片

#### 4.4 运行结果

##### 4.4.1 仓库为1时

![生产消费仓库1](https://haosn.oss-cn-beijing.aliyuncs.com/typora/%E7%94%9F%E4%BA%A7%E6%B6%88%E8%B4%B9%E4%BB%93%E5%BA%931.png)

##### 4.4.2 仓库为5时

![生产消费仓库5](https://haosn.oss-cn-beijing.aliyuncs.com/typora/%E7%94%9F%E4%BA%A7%E6%B6%88%E8%B4%B9%E4%BB%93%E5%BA%935.png)