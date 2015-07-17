#ifndef _FBR_SYSCALLS_H_
#define _FBR_SYSCALLS_H_

#define connect sys_connect
#define accept sys_accept
#define sleep sys_sleep
#define read sys_read
#define write sys_write
#define poll sys_poll

extern int (*sys_connect)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
extern int (*sys_accept)(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
extern unsigned int (*sys_sleep)(unsigned int seconds);
extern ssize_t (*sys_read)(int fd, const void *buf, size_t count);
extern ssize_t (*sys_write)(int fd, const void *buf, size_t count);
extern int (*sys_poll)(struct pollfd fds[], nfds_t nfds, int timeout);

#endif /*_FBR_SYSCALLS_H_*/

