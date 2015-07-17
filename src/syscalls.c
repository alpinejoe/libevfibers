#define _GNU_SOURCE //Allow usage of RTLD_NEXT
#include <dlfcn.h>
#include <evfibers/config.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <libgen.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <err.h>

#ifdef FBR_EIO_ENABLED
#include <evfibers/eio.h>
#endif
#include <evfibers_private/fiber.h>

#define CONFIRM(a) puts("Loading " #a)
struct fbr_context gFbrCtx;

int (*sys_connect)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
  CONFIRM(connect);
  return fbr_connect(&gFbrCtx, sockfd, addr, addrlen);
}

int (*sys_accept)(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  CONFIRM(accept);
  return fbr_accept(&gFbrCtx, sockfd, addr, addrlen);
}

unsigned int (*sys_sleep)(unsigned int seconds);
unsigned int sleep(unsigned int seconds) {
  CONFIRM(sleep);
  return fbr_sleep(&gFbrCtx, seconds);
}

ssize_t (*sys_read)(int fd, const void *buf, size_t count);
ssize_t read(int fd, void *buf, size_t count) {
  CONFIRM(read);
  return fbr_read(&gFbrCtx, fd, buf, count);
}

ssize_t (*sys_write)(int fd, const void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count) {
  CONFIRM(write);
  return fbr_write(&gFbrCtx, fd, buf, count);
}

int (*sys_poll)(struct pollfd fds[], nfds_t nfds, int timeout);
int poll(struct pollfd fds[], nfds_t nfds, int timeout)	{
  CONFIRM(poll);
  return fbr_poll(&gFbrCtx, fds, nfds, timeout/1000.0);
}


//NSPR overrides
void fbr_main(struct fbr_context *fctx, void *arg) {
  void (*func)(void *) = fbr_get_user_data(fctx, fbr_self(fctx));
  func(arg);
  fbr_set_reclaim(fctx, fbr_self(fctx));
}

void *PR_CreateThread(_unused_ int type, void(*start)(void *arg), void *arg,
    _unused_ int priority, _unused_ int scope, _unused_ int state, _unused_ unsigned int stack) {
  fbr_id_t f = fbr_create(&gFbrCtx, "", fbr_main, arg, stack);
  fbr_set_user_data(&gFbrCtx, f, start);
  fbr_transfer(&gFbrCtx, f);

  return f.p;
}

int PR_JoinThread(struct fbr_fiber *thread) {
  fbr_id_t id;
  id.p = thread;
  id.g = thread->id;
  return fbr_reclaim(&gFbrCtx, id);
}

//Locks
struct fbr_mutex *PR_NewLock() {
  struct fbr_mutex *mutex = malloc(sizeof(struct fbr_mutex));
  fbr_mutex_init(&gFbrCtx, mutex);
  return mutex;
}

void PR_DestroyLock(struct fbr_mutex *mutex) {
  fbr_mutex_destroy(&gFbrCtx, mutex);
  free(mutex);
}

void PR_Lock(struct fbr_mutex *mutex) {
  if (gFbrCtx.__p)
  fbr_mutex_lock(&gFbrCtx, mutex);
}

int PR_Unlock(struct fbr_mutex *mutex) {
  if (gFbrCtx.__p)
  fbr_mutex_unlock(&gFbrCtx, mutex);
  return 0;
}

//CV
struct cv {
  struct fbr_cond_var cond;
  struct fbr_mutex *mutex;
};

struct cv *PR_NewCondVar(struct fbr_mutex *mutex) {
  struct cv *cond = malloc(sizeof(struct cv));
  cond->mutex = mutex;
  fbr_cond_init(&gFbrCtx, &cond->cond);
  return cond;
}

void PR_DestroyCondVar(struct cv *cond) {
  fbr_cond_destroy(&gFbrCtx, &cond->cond);
  free(cond);
}

int PR_WaitCondVar(struct cv *cond, unsigned int ticks) {
  /* _PR_UNIX_GetInterval in https://github.com/servo/nspr/blob/8219c5c5137b94ae93cfb4afc1dc10a94384e1c6/pr/src/md/unix/unix.c */
  return fbr_cond_wait_wto(&gFbrCtx, &cond->cond, cond->mutex, ticks/1000.0);
}

int PR_NotifyCondVar(struct cv *cond) {
  fbr_cond_signal(&gFbrCtx, &cond->cond);
  return 0;
}

int PR_NotifyAllCondVar(struct cv *cond) {
  fbr_cond_broadcast(&gFbrCtx, &cond->cond);
  return 0;
}

static void __attribute__ ((constructor(101))) override_sysfns() {
  sys_connect = dlsym(RTLD_NEXT, "connect");
  sys_accept = dlsym(RTLD_NEXT, "accept");
  sys_sleep = dlsym(RTLD_NEXT, "sleep");
  sys_read = dlsym(RTLD_NEXT, "read");
  sys_write = dlsym(RTLD_NEXT, "write");
  sys_poll = dlsym(RTLD_NEXT, "poll");

  fbr_init(&gFbrCtx, EV_DEFAULT);
  signal(SIGPIPE, SIG_IGN);
}
