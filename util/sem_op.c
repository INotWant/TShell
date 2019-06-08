//
// Created by iwant on 19-5-30.
//

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <stdio.h>
#include <memory.h>

#include "sem_op.h"

int create_sem(int nums, const char *pathname, int proj_id) {
    key_t key = ftok(pathname, proj_id);
    int sems_id = semget(key, nums, IPC_CREAT | 0666);
    if (sems_id < 0) {
        perror("create segment");
        return -1;
    }
    return sems_id;
}

int init_sem(int sems_id, int sem_id, int val) {
    semun.val = val;
    int ret = semctl(sems_id, sem_id, SETVAL, semun);
    if (ret < 0) {
        perror("init segment");
        return -1;
    }
    return ret;
}


int get_sem(const char *pathname, int proj_id) {
    key_t key = ftok(pathname, proj_id);
    int sems_id = semget(key, 0, 0);
    if (sems_id < 0) {
        perror("get segment");
        return -1;
    }
    return sems_id;
}

static int sem_op(int sems_id, unsigned short int sem_id, short int op) {
    struct sembuf sem;
    memset(&sem, '\0', sizeof(sem));
    sem.sem_num = sem_id;
    sem.sem_op = op;
    sem.sem_flg = SEM_UNDO;
    if (semop(sems_id, &sem, 1) < 0) {
        perror("segment op");
        return -1;
    }
    return 0;
}

int p_op(int sems_id, int sem_id) {
    return sem_op(sems_id, (unsigned short) sem_id, -1);
}

int v_op(int sems_id, int sem_id) {
    return sem_op(sems_id, (unsigned short) sem_id, +1);
}

int destroy_sem(int sems_id, int sem_id) {
    if (sems_id >= 0)
        if (semctl(sems_id, sem_id, IPC_RMID, NULL) < 0) {
            perror("segment op");
            return -1;
        }
    return 0;
}
