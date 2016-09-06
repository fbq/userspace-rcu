#ifndef _URCU_RSEQ_STATIC_H
#define _URCU_RSEQ_STATIC_H

#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include <urcu/compiler.h>
#include <urcu/arch.h>
#include <urcu/system.h>
#include <urcu/uatomic.h>
#include <urcu/tls-compat.h>
#include <urcu/list.h>
#include <urcu/debug.h>
#include <urcu/futex.h>
#include <rseq.h>

#ifdef __cplusplus
extern "C" {
#endif

struct srcu_percpu {
	unsigned long ctr[2];
	unsigned long seq[2];
} __attribute__((aligned(CAA_CACHE_LINE_SIZE)));

struct srcu_gp_waiters;

struct srcu_struct {
	/* writer modifiy mostly */
	unsigned long completed __attribute__((aligned(CAA_CACHE_LINE_SIZE)));
	pthread_mutex_t lock;

	/* reader access mostly */
	struct rseq_lock rseq_lock __attribute((aligned(CAA_CACHE_LINE_SIZE)));

	/* read-only */
	struct srcu_percpu *percpu_data __attribute((aligned(CAA_CACHE_LINE_SIZE)));
	int nr_cpus;

	struct srcu_gp_waiters *gp_waiters;
};

struct rcu_reader {
	unsigned long idx;
	unsigned long nested;
};

extern struct srcu_struct global_srcu;
extern DECLARE_URCU_TLS(struct rcu_reader, rcu_reader);

#define srcu_inc_percpu(sp, idx, member)				\
do {									\
	struct rseq_state state;					\
	int cpu;							\
	bool result;							\
	unsigned long *ptr, val;					\
									\
	do_rseq(&(sp)->rseq_lock, state, cpu, result, ptr, val,		\
		{							\
			ptr = &(sp)->percpu_data[cpu].member[idx];	\
			val = (sp)->percpu_data[cpu].member[idx] + 1;	\
		});							\
} while (0)

#define srcu_dec_percpu(sp, idx, member)				\
do {									\
	struct rseq_state state;					\
	int cpu;							\
	bool result;							\
	unsigned long *ptr, val;					\
									\
	do_rseq(&(sp)->rseq_lock, state, cpu, result, ptr, val,		\
		{							\
			ptr = &(sp)->percpu_data[cpu].member[idx];	\
			val = (sp)->percpu_data[cpu].member[idx] - 1;	\
		});							\
} while (0)


#define srcu_read_lock(sp)						\
({									\
	unsigned long idx = CMM_ACCESS_ONCE((sp)->completed) & 0x1;	\
									\
	srcu_inc_percpu(sp, idx, ctr);					\
	cmm_smp_mb();							\
	srcu_inc_percpu(sp, idx, seq);					\
									\
	idx;								\
});

#define srcu_read_unlock(sp, idx)					\
({									\
	cmm_smp_mb();							\
	srcu_dec_percpu(sp, idx, ctr);					\
})

#define srcu_read_percpu_all(sp, idx, member)				\
({									\
	int cpu;							\
	unsigned long ____result = 0ul;					\
									\
	for (cpu = 0; cpu < sp->nr_cpus; cpu++)				\
		____result += sp->percpu_data[cpu].member[idx];		\
									\
	____result;							\
})

static inline void _rcu_read_lock(void)
{
	if (URCU_TLS(rcu_reader).nested == 0)
		URCU_TLS(rcu_reader).idx =
			srcu_read_lock(&global_srcu);
	URCU_TLS(rcu_reader).nested++;
}

static inline void _rcu_read_unlock(void)
{
	URCU_TLS(rcu_reader).nested--;
	if (URCU_TLS(rcu_reader).nested == 0)
		srcu_read_unlock(&global_srcu,
				 URCU_TLS(rcu_reader).idx);
}

static inline int _rcu_read_ongoing(void)
{
	return URCU_TLS(rcu_reader).nested;
}


#ifdef __cplusplus
}
#endif

#endif /* _URCU_RSEQ_STATIC_H */
