extern int vsprintf(char *__buffer, const char *__format, void *__arglist);

#define __size(x) ((sizeof(x)+sizeof(int)-1) & ~(sizeof(int)-1))

#define va_start(ap, parmN) ((void)((ap) = (void *)((char *)(&parmN)+__size(parmN))))

#define va_arg(ap, type) (*(type *)(((*(char **)&(ap))+=__size(type))-(__size(type))))
#define va_end(ap)          ((void)0)
