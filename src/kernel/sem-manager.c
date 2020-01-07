///////////////////////////////////////////////////////////////////////////////
//
//                          IMPORTANT NOTICE
//
// The following open source license statement does not apply to any
// entity in the Exception List published by FMSoft.
//
// For more information, please visit:
//
// https://www.fmsoft.cn/exception-list
//
//////////////////////////////////////////////////////////////////////////////
/*
 *   This file is part of MiniGUI, a mature cross-platform windowing
 *   and Graphics User Interface (GUI) support system for embedded systems
 *   and smart IoT devices.
 *
 *   Copyright (C) 2002~2020, Beijing FMSoft Technologies Co., Ltd.
 *   Copyright (C) 1998~2002, WEI Yongming
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *   Or,
 *
 *   As this program is a library, any link to this program must follow
 *   GNU General Public License version 3 (GPLv3). If you cannot accept
 *   GPLv3, you need to be licensed from FMSoft.
 *
 *   If you have got a commercial license of this program, please use it
 *   under the terms and conditions of the commercial license.
 *
 *   For more information about the commercial license, please refer to
 *   <http://www.minigui.com/blog/minigui-licensing-policy/>.
 */
/*
** fixstr.c: the Fixed String module for MiniGUI.
**
** Current maintainer: Wei Yongming.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "common.h"

#ifdef _MGRM_PROCESSES

#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/mman.h>

#include "minigui.h"
#include "gdi.h"
#include "window.h"
#include "cliprect.h"
#include "gal.h"
#include "internals.h"
#include "ourhdr.h"

static struct _manager {
    int             semid;
    int             nr_sems;
    int             len_use_bmp;
    BYTE*           use_bmp;
#ifndef _MGRM_STANDALONE
    pthread_mutex_t lock;
#endif
} manager;

BOOL mg_InitSemManger (int nr_sems)
{
    if (nr_sems <= 0) {
        _ERR_PRINTF ("SemManager: cannot allocate use bitmap.\n");
        return FALSE;
    }

    manager.len_use_bmp = (nr_sems + 7) >> 3;
    manager.use_bmp = malloc(manager.len_use_bmp);
    if (manager.use_bmp == NULL) {
        _ERR_PRINTF ("SemManager: cannot allocate use bitmap.\n");
        return FALSE;
    }

    manager.semid = semget (IPC_PRIVATE, nr_sems,
                0666 | IPC_CREAT | IPC_EXCL);
    if (manager.semid == -1) {
        _ERR_PRINTF ("SemManager: cannot create semset for compositing.\n");
        free (manager.use_bmp);
        return FALSE;
    }

#ifndef _MGRM_STANDALONE
    pthread_mutex_init (&manager.lock, NULL);
#endif

    manager.nr_sems = nr_sems;
    memset (manager.use_bmp, 0xFF, manager.len_use_bmp);
    return TRUE;
}

void mg_TerminateSemManager (void)
{
    union semun ignored;
    if (manager.nr_sems == 0) {
        _ERR_PRINTF ("SemManager: not intialized.\n");
        return;
    }

    if (semctl (manager.semid, 0, IPC_RMID, ignored) < 0) {
        _WRN_PRINTF ("SemManager: failed to remove semset for compositing.\n");
    }

    free (manager.use_bmp);

    manager.nr_sems = 0;

#ifndef _MGRM_STANDALONE
    pthread_mutex_destroy (&manager.lock);
#endif
}

int __mg_alloc_sem_for_compos (void)
{
    int n = -1, i, j;
    BYTE* bitmap = manager.use_bmp;

#ifndef _MGRM_STANDALONE
    pthread_mutex_lock (&manager.lock);
#endif

    for (i = 0; i < manager.len_use_bmp; i++) {
        for (j = 0; j < 8; j++) {
            if (*bitmap & (0x80 >> j)) {
                *bitmap &= (~(0x80 >> j));
                n = i * 8 + j;
                break;
            }
        }

        bitmap++;
    }

    if (n >= manager.nr_sems)
        n = -1;

#ifndef _MGRM_STANDALONE
    pthread_mutex_unlock (&manager.lock);
#endif

    return n;
}

void __mg_free_sem_for_compos (int sem_num)
{
    BYTE* bitmap = manager.use_bmp;

    if (sem_num < 0 || sem_num >= manager.nr_sems) {
        _DBG_PRINTF("Bad semaphore number: %d\n", sem_num);
        return;
    }

#ifndef _MGRM_STANDALONE
    pthread_mutex_lock (&manager.lock);
#endif

    bitmap = bitmap + (sem_num >> 3);

#ifdef _DEBUG
    if (*bitmap & (0x80 >> (sem_num % 8))) {
        _WRN_PRINTF("The semaphore is not marked as used: %d\n", sem_num);
    }
#endif

    *bitmap |= (0x80 >> (sem_num % 8));

#ifndef _MGRM_STANDALONE
    pthread_mutex_unlock (&manager.lock);
#endif
}

#endif /* _MGRM_PROCESSES */
