#define _LGPL_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <poll.h>

#include "urcu/wfcqueue.h"
#include "urcu/map/urcu-rseq.h"
#include "urcu/static/urcu-rseq.h"
#include "urcu-pointer.h"
#include "urcu/tls-compat.h"

#include "urcu-die.h"
#include "urcu-wait.h"

#undef _LGPL_SOURCE
#include "urcu-rseq.h"
#define _LGPL_SOURCE

struct srcu_struct global_srcu;
DEFINE_URCU_TLS(struct rcu_reader, rcu_reader);

static void mutex_lock(pthread_mutex_t *mutex)
{
	int ret;

	ret = pthread_mutex_lock(mutex);
	if (ret)
		urcu_die(ret);
}

static void mutex_unlock(pthread_mutex_t *mutex)
{
	int ret;

	ret = pthread_mutex_unlock(mutex);
	if (ret)
		urcu_die(ret);
}

struct srcu_gp_waiters {
	struct urcu_wait_queue queue;
	struct urcu_waiters check;
};

void srcu_init(struct srcu_struct *sp)
{
	sp->completed = 0UL;
	pthread_mutex_init(&sp->lock, NULL);
	sp->gp_waiters = calloc(1, sizeof(struct srcu_gp_waiters));
	sp->gp_waiters->queue.stack.head = (void *) 0x1UL;
       	pthread_mutex_init(&sp->gp_waiters->queue.stack.lock, NULL);

	if (rseq_init_lock(&sp->rseq_lock))
		abort();

	/* TODO: Runtime cpu number detection */
	sp->percpu_data = calloc(256, sizeof(struct srcu_percpu));
	sp->nr_cpus = 256;	

}

void srcu_destroy(struct srcu_struct *sp)
{
	free(sp->percpu_data);
	free(sp->gp_waiters);
}

/* Must be called with sp->lock held */
void srcu_advance(struct srcu_struct *sp, struct urcu_waiters *wp)
{
	unsigned long idx, seq, ctr;

	idx = (CMM_ACCESS_ONCE(sp->completed) & 0x1) ^ 0x1;


	do {
		seq = srcu_read_percpu_all(sp, idx, seq);

		cmm_smp_mb();

		ctr = srcu_read_percpu_all(sp, idx, ctr);

		cmm_smp_mb();

	} while (ctr != 0 || seq != srcu_read_percpu_all(sp, idx, seq));

	*wp = sp->gp_waiters->check;

	urcu_move_waiters(&sp->gp_waiters->check, &sp->gp_waiters->queue);
}

void synchronize_srcu(struct srcu_struct *sp)
{
	DEFINE_URCU_WAIT_NODE(wait, URCU_WAIT_WAITING);
	struct urcu_waiters waiters;
	unsigned long completed;

	if (pthread_mutex_trylock(&sp->lock)) {
		urcu_wait_add(&sp->gp_waiters->queue, &wait);
		urcu_adaptative_busy_wait(&wait);
		goto gp_end;

	}

	srcu_advance(sp, &waiters);

	completed = ++CMM_ACCESS_ONCE(sp->completed);

	pthread_mutex_unlock(&sp->lock);

	urcu_wake_all_waiters(&waiters);

	if (completed == CMM_ACCESS_ONCE(sp->completed)) {
		pthread_mutex_lock(&sp->lock);
		
		if (completed == CMM_ACCESS_ONCE(sp->completed)) {

			srcu_advance(sp, &waiters);

			CMM_ACCESS_ONCE(sp->completed)++;

			pthread_mutex_unlock(&sp->lock);

			urcu_wake_all_waiters(&waiters);

			goto gp_end;
		}

		pthread_mutex_unlock(&sp->lock);
	}
gp_end:
	cmm_smp_mb();
	return;
}

void rcu_read_lock(void)
{
	_rcu_read_lock();
}

void rcu_read_unlock(void)
{
	_rcu_read_unlock();
}

int rcu_read_ongoing(void)
{
	return _rcu_read_ongoing();
}

void rcu_init(void)
{
	srcu_init(&global_srcu);
}

void rcu_destroy(void)
{
	srcu_destroy(&global_srcu);
}

void synchronize_rcu(void)
{
	synchronize_srcu(&global_srcu);
}

void __attribute__((constructor)) rcu_init(void);
void __attribute__((destructor)) rcu_destroy(void);

DEFINE_RCU_FLAVOR(rcu_flavor);

#include "urcu-call-rcu-impl.h"
#include "urcu-defer-impl.h"
