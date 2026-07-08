#ifndef IDACME_LOG_H
#define IDACME_LOG_H

extern int log_file_open(const char *filename);
extern void log_file_close(void);

extern void log_message_direct(const char *msg);
extern void log_warning_direct(const char *msg);
extern void log_error_direct(const char *msg);

extern void log_message(const char *format, ...);
extern void log_warning(const char *format, ...);
extern void log_error(const char *format, ...);
extern void log_fatal_and_die(const char *format, ...) __attribute__((noreturn));

#ifdef FEATURE_MODEBUG
extern void log_debug(int level, const char *format, ...);
#define LOG_DEBUG(x) log_debug x
#define IF_DEBUG(x) x
#else
#define LOG_DEBUG(x)
#define IF_DEBUG(x)
#endif

#endif
