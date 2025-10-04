
#include <atomic.h>
#include <cpu.h>
#include <spinlock.h>

#include <x86-64/asm.h>

#include <kernel/acpi.h>

#ifdef USE_BIG_KERNEL_LOCK
/* The big kernel lock */
struct spinlock kernel_lock = {
	.rank = 0,
#ifdef DEBUG_SPINLOCK
	.name = "kernel_lock"
#endif
};
#endif

static int holding(struct spinlock *lock)
{
	return lock->locked && lock->cpu == this_cpu;
}

bool spin_haslock(struct spinlock *lock)
{
	return holding(lock);
}

void spin_init(struct spinlock *lock, const char *name, uint64_t rank)
{
	// Validate the spinlock rank: this will fail if more than one bit is set
	if(rank & (rank - 1)) {
		panic("Spinlocks can only have ranks as powers of two!");
	}

	lock->locked = 0;
	lock->rank = rank;

#ifdef DEBUG_SPINLOCK
	lock->name = name;
	lock->cpu = NULL;
#endif
}

void __spin_lock(struct spinlock *lock, const char *file, int line)
{
#ifdef DEBUG_SPINLOCK
	/* Check if this is the second time the lock is being acquired by the
	 * same CPU.
	 */
	if (holding(lock)) {
		panic("\n"
		      "%s:%d: cpu %2d: attempt to lock %s twice\n"
		      "%s:%d: cpu %2d: currently locked here\n",
		      file, line,
		      lapic_cpunum(),
		      lock->name ? lock->name : "anonymous",
		      lock->file, lock->line,
		      lock->cpu->cpu_id);
	}
#endif

	// Validate rank
	if(lock->rank != 0) {
		if(lock->rank <= this_cpu->spinlock_rank) {
			panic("\n"
			      "%s:%d: cpu %2d: attempt to lock %s with rank %llu but the CPU currently has rank %llu\n",
				  file, line,
				  lapic_cpunum(),
				  lock->name ? lock->name : "anonymous",
				  lock->rank,
				  this_cpu->spinlock_rank);
		}
	}

	while (!atomic_cmpxchg(&lock->locked, 0, 1));
	this_cpu->spinlock_rank |= lock->rank;

	atomic_barrier();

#ifdef DEBUG_SPINLOCK
	lock->cpu = this_cpu;
	lock->file = file;
	lock->line = line;
#endif
}

int __spin_trylock(struct spinlock *lock, const char *file, int line)
{
#ifdef DEBUG_SPINLOCK
	/* Check if this is the second time the lock is being acquired by the
	 * same CPU.
	 */
	if (holding(lock)) {
		panic("\n"
		      "%s:%d: cpu %2d: attempt to lock %s twice\n"
		      "%s:%d: cpu %2d: currently locked here\n",
		      file, line,
		      lapic_cpunum(),
		      lock->name ? lock->name : "anonymous",
		      lock->file, lock->line,
		      lock->cpu->cpu_id);
	}
#endif

	// Validate rank
	if (lock->rank != 0) {
		if (lock->rank <= this_cpu->spinlock_rank) {
			panic("\n"
			      "%s:%d: cpu %2d: attempt to lock %s with rank %llu, but the CPU currently has rank %llu\n",
			      file, line,
			      lapic_cpunum(),
			      lock->name ? lock->name : "anonymous",
			      lock->rank,
			      this_cpu->spinlock_rank);
		}
	}

	if (!atomic_cmpxchg(&lock->locked, 0, 1)) {
		return 0;
	}
	this_cpu->spinlock_rank |= lock->rank;

	atomic_barrier();

#ifdef DEBUG_SPINLOCK
	lock->cpu = this_cpu;
	lock->file = file;
	lock->line = line;
#endif

	return 1;
}

void __spin_unlock(struct spinlock *lock, const char *file, int line)
{
#ifdef DEBUG_SPINLOCK
	/* Check if the lock is actually locked before unlocking. */
	if (!lock->locked) {
		panic("\n"
		      "%s:%d: cpu %2d:%s not locked\n",
		      file, line,
		      lapic_cpunum(),
		      lock->name ? lock->name : "anonymous");
	}

	/* Check if the lock that we are about to unlock is actually owned by
	 * another CPU.
	 */
	if (!holding(lock)) {
		panic("\n"
		      "%s:%d: cpu %2d: attempt to unlock %s\n"
		      "%s:%d: cpu %2d: currently locked here\n",
		      file, line,
		      lapic_cpunum(),
		      lock->name ? lock->name : "anonymous",
		      lock->file, lock->line, lock->cpu->cpu_id);
	}
#endif

	// Validate rank
	if (lock->rank != 0) {
		// If the lock rank is the most significant bit set in the current CPU
		// rank, it means that 2 * lock rank > current CPU rank,...
		if((lock->rank << 1) <= this_cpu->spinlock_rank) {
			panic("\n"
			      "%s:%d: cpu %2d: attempt to unlock %s with rank %llu, but the CPU currently has rank %llu\n",
			      file, line,
			      lapic_cpunum(),
			      lock->name ? lock->name : "anonymous",
			      lock->rank,
			      this_cpu->spinlock_rank);
		}

		// but at the same time we need to make sure the bit is actually set, so
		// lock rank <= current CPU rank
		if(lock->rank > this_cpu->spinlock_rank) {
			panic("\n"
			      "%s:%d: cpu %2d: attempt to unlock %s with rank %llu, but the CPU currently has rank %llu\n",
			      file, line,
			      lapic_cpunum(),
			      lock->name ? lock->name : "anonymous",
			      lock->rank,
			      this_cpu->spinlock_rank);
		}
	}

#ifdef DEBUG_SPINLOCK
	lock->cpu = NULL;
	lock->file = NULL;
	lock->line = 0;
#endif

	atomic_barrier();
	lock->locked = 0;
	this_cpu->spinlock_rank ^= lock->rank;
	atomic_barrier();
}

