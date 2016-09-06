#ifndef _URCU_RSEQ_H
#define _URCU_RSEQ_H

/*
 * urcu-rseq-static.h
 *
 * Userspace RCU RSEQ header.
 *
 */

#include <stdlib.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <urcu/map/urcu-rseq.h>

#ifdef _LGPL_SOURCE
#include "urcu/static/urcu-rseq.h"

#define rcu_read_lock_rseq		_rcu_read_lock
#define rcu_read_unlock_rseq		_rcu_read_unlock
#define rcu_read_ongoing_rseq		_rcu_read_ongoing

#else /* !_LGPL_SOURCE */
extern void rcu_read_lock(void);
extern void rcu_read_unlock(void);
extern int rcu_read_ongoing(void);
#endif

static inline void rcu_register_thread(void)
{
	rseq_register_current_thread();
}

static inline void rcu_unregister_thread(void)
{
}

static inline void rcu_thread_online(void)
{
}

static inline void rcu_thread_offline(void)
{
}

static inline void rcu_quiescent_state(void)
{
}

extern void srcu_init(struct srcu_struct *sp);
extern void srcu_destroy(struct srcu_struct *sp);
extern void synchronize_srcu(struct srcu_struct *sp);
extern void rcu_init(void);
extern void rcu_destroy(void);
extern void synchronize_rcu(void);

#ifdef __cplusplus
}
#endif

#include <urcu-call-rcu.h>
#include <urcu-defer.h>
#include <urcu-flavor.h>

#endif /* _URCU_RSEQ_H */
