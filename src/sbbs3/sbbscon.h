#ifndef _SBBSCON_H_
#define _SBBSCON_H_

#ifdef _THREAD_SUID_BROKEN
extern int	thread_suid_broken;			/* NPTL is no longer broken */
#else
#define thread_suid_broken FALSE
#endif

#endif
