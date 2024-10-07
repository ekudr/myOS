#ifndef __USER_H__
#define __USER_H__

// syscalls
int exit(int) __attribute__((noreturn));
int putc(char);
int exec(char*, char**);

// ulib
void lib_puts(char *s);

#endif /* __USER_H__ */