
#pragma once

#include <types.h>

struct cpuinfo;

struct spinlock {
	/* Is the lock held? */
	volatile unsigned locked;

	/* The rank of the lock, for use in fine-grained locking. To prevent
	 * deadlocks, it is recommended to use a static rank for each lock, allowing
	 * us to force taking locks in a specifi corder. This way, various deadlock
	 * issues that result from bad lock-taking practice can be prevented. This
	 * concept is inspired by SerenityOS.
	 *
	 * The rank of a lock is always a power of 2: at most 1 out of 64 bits can
	 * be a 1. A lock can only be acquired if the lock rank > current CPU rank;
	 * acquiring a lock will update the current CPU rank. A lock can only be
	 * released if it is the most significant bit set in the current rank. This
	 * way, acquiring and releasing locks must be done in reverse order.
	 *
	 * Using ranked locks is optional, but strongly recommended.
	 */
	uint64_t rank;

#define DEBUG_SPINLOCK
	/* The name of the lock. */
	const char *name;

	/* The CPU that is holding the lock. */
	struct cpuinfo *cpu;

	/* The filename and line at which the last successful lock took
	 * place.
	 */
	const char *file;
	int line;
};

#define spin_lock(lock) __spin_lock(lock, __FILE__, __LINE__)
#define spin_trylock(lock) __spin_trylock(lock, __FILE__, __LINE__)
#define spin_unlock(lock) __spin_unlock(lock, __FILE__, __LINE__)

void spin_init(struct spinlock *lock, const char *name, uint64_t rank);
bool spin_haslock(struct spinlock *lock);
void __spin_lock(struct spinlock *lock, const char *file, int line);
int __spin_trylock(struct spinlock *lock, const char *file, int line);
void __spin_unlock(struct spinlock *lock, const char *file, int line);

/**
 * These are some helper macros to prevent you from having to write conditional
 * macros all the time when switching from the BKL to fine-grained locks. The
 * big_spin_* macros are only active when BKL is enabled, the fine_spin_* macros
 * are only active when fine-grained locking is active.
 */
#ifdef USE_BIG_KERNEL_LOCK

#define big_spin_lock(lock) spin_lock(lock)
#define big_spin_trylock(lock) spin_trylock(lock)
#define big_spin_unlock(lock) spin_unlock(lock)
#define big_spin_haslock(lock) spin_haslock(lock)

#define fine_spin_lock(lock) do {} while(0)
#define fine_spin_trylock(lock) 1
#define fine_spin_unlock(lock) do {} while(0)
#define fine_spin_haslock(lock) 1

#else

#define big_spin_lock(lock) do {} while(0)
#define big_spin_trylock(lock) 1
#define big_spin_unlock(lock) do {} while(0)
#define big_spin_haslock(lock) 1

#define fine_spin_lock(lock) spin_lock(lock)
#define fine_spin_trylock(lock) spin_trylock(lock)
#define fine_spin_unlock(lock) spin_unlock(lock)
#define fine_spin_haslock(lock) spin_haslock(lock)

#endif

