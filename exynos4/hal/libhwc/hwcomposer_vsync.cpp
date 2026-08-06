/*
 * Copyright (C) 2015 The NamelessRom Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "hwcomposer.h"
#include "hwcomposer_vsync.h"

#include <pthread.h>
#include <unistd.h>
#include <utils/threads.h>

#include <sys/prctl.h>

/*****************************************************************************/

#define HWC_VSYNC_THREAD_NAME "hwcVsyncThread"

#define VSYNC_TIME_PATH "/sys/devices/platform/samsung-pd.2/s3cfb.0/vsync_time"

/*****************************************************************************/

static void *hwc_vsync_thread(void *data)
{
    static char buf[4096];
    fd_set exceptfds;
    int res;
    int64_t timestamp = 0;
    hwc_context_t* ctx = (hwc_context_t *)(data);

    // open the file descriptor for the vsync timestamp
    ctx->vsync_timestamp_fd = open(VSYNC_TIME_PATH, O_RDONLY);

    // set up the thread
    char thread_name[64] = HWC_VSYNC_THREAD_NAME;
    prctl(PR_SET_NAME, (unsigned long) &thread_name, 0, 0, 0);

    struct sched_param sched_param = {0};
    sched_param.sched_priority = 5;
    if (sched_setscheduler(gettid(), SCHED_FIFO, &sched_param) != 0) {
        ALOGE("Couldn't set SCHED_FIFO for hwc_vsync");
    }

    memset(buf, 0, sizeof(buf));

    int maxfd = ctx->vsync_timestamp_fd;
    if (ctx->vsync_stop_fd[0] > maxfd)
        maxfd = ctx->vsync_stop_fd[0];

    while (ctx->vsync_thread_running) {
        ssize_t len = read(ctx->vsync_timestamp_fd, buf, sizeof(buf));
        timestamp = strtoull(buf, NULL, 0);
        if (ctx->procs) {
            if (DEBUG_VSYNC) {
                ALOGD("%s: handling vsync, timestamp:%lld", __FUNCTION__, timestamp);
            }
            ctx->procs->vsync(ctx->procs, 0, timestamp);
        }

        FD_ZERO(&exceptfds);
        FD_SET(ctx->vsync_timestamp_fd, &exceptfds);
        FD_SET(ctx->vsync_stop_fd[0], &exceptfds);
        res = select(maxfd + 1, NULL, NULL, &exceptfds, NULL);

        if (!ctx->vsync_thread_running || FD_ISSET(ctx->vsync_stop_fd[0], &exceptfds)) {
            // clean shutdown requested via close_vsync_thread()
            break;
        }

        lseek(ctx->vsync_timestamp_fd, 0, SEEK_SET);
    }

    return NULL;
}

void init_vsync_thread(hwc_context_t* ctx)
{
    int ret;

    ALOGD("Initializing VSYNC Thread: " HWC_VSYNC_THREAD_NAME);

    // Not opened yet -- hwc_vsync_thread() opens the real fd once it starts.
    // Must not stay at the memset()-default of 0 (stdin), or a failed
    // pthread_create() below would make close_vsync_thread() close(0).
    ctx->vsync_timestamp_fd = -1;

    if (pipe(ctx->vsync_stop_fd) != 0) {
        ALOGE("%s: failed to create stop pipe: %s", __FUNCTION__, strerror(errno));
        ctx->vsync_stop_fd[0] = -1;
        ctx->vsync_stop_fd[1] = -1;
    }

    ctx->vsync_thread_running = true;

    ret = pthread_create(&ctx->vsync_thread, NULL, hwc_vsync_thread, (void*) ctx);
    if (ret) {
        ALOGE("%s: failed to create %s: %s", __FUNCTION__,
              HWC_VSYNC_THREAD_NAME, strerror(ret));
        // pthread_create failed: no live thread was started, make sure we
        // never try to join/kill a bogus handle later in close_vsync_thread()
        ctx->vsync_thread_running = false;
        ctx->vsync_thread = 0;
    }
}

void close_vsync_thread(hwc_context_t* ctx)
{
    if (!ctx->vsync_thread_running && ctx->vsync_thread == 0) {
        // init_vsync_thread() never successfully started a thread,
        // nothing to stop/join here.
        if (ctx->vsync_stop_fd[0] >= 0) close(ctx->vsync_stop_fd[0]);
        if (ctx->vsync_stop_fd[1] >= 0) close(ctx->vsync_stop_fd[1]);
        if (ctx->vsync_timestamp_fd >= 0) close(ctx->vsync_timestamp_fd);
        return;
    }

    // signal clean shutdown instead of pthread_kill(SIGTERM), which can
    // terminate the whole process by default disposition rather than
    // just this thread.
    ctx->vsync_thread_running = false;
    if (ctx->vsync_stop_fd[1] >= 0) {
        char c = 'x';
        write(ctx->vsync_stop_fd[1], &c, 1);
    }

    pthread_join(ctx->vsync_thread, NULL);

    if (ctx->vsync_stop_fd[0] >= 0) close(ctx->vsync_stop_fd[0]);
    if (ctx->vsync_stop_fd[1] >= 0) close(ctx->vsync_stop_fd[1]);
    if (ctx->vsync_timestamp_fd >= 0) close(ctx->vsync_timestamp_fd);
}
