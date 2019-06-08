//
// Created by iwant on 19-5-30.
//

#ifndef TSHELL_SEM_OP_H
#define TSHELL_SEM_OP_H

typedef union {
    int val;                    /* Value for SETVAL */
    struct semid_ds *buf;       /* Buffer for IPC_STAT, IPC_SET */
    unsigned short *array;      /* Array for GETALL, SETALL */
    struct seminfo *__buf;      /* Buffer for IPC_INFO*/
} sem_un;

static sem_un semun;

/*
 * 创建信号量（集）
 *
 * @param nums 指创建信号量的个数
 * @return 成功时，返回信号集Id。失败时，返回 -1
 */
int create_sem(int nums, const char *pathname, int proj_id);

/*
 * 初始化信号量
 *
 * @param sem_id 信号量集
 * @param flag   信号量
 * @param val    值
 * @return 成功时，返回值大于0。失败时，返回-1
 */
int init_sem(int sems_id, int sem_id, int val);

/*
 * 获取已经创建的信号集ID
 *
 * @return 成功时，返回值大于0(即信号集id)。失败时，返回-1
 */
int get_sem(const char *pathname, int proj_id);

/*
 * 指定信号量进行 p 操作
 *
 * @return 成功时，返回值为0。失败时，返回-1
 */
int p_op(int sems_id, int sem_id);

/*
 * 指定信号量进行 v 操作
 *
 * @return 成功时，返回值为0。失败时，返回-1
 */
int v_op(int sems_id, int sem_id);

/*
 * 释放指定信号量
 *
 *  @return 成功时，返回值为0。失败时，返回-1
 */
int destroy_sem(int sems_id, int sem_id);

#endif // TSHELL_SEM_OP_H
