/*
 * Read-Copy Update mechanism for mutual exclusion (tree-based version)
 * Internal non-public definitions that provide either classic
 * or preemptable semantics.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright Red Hat, 2009
 * Copyright IBM Corporation, 2009
 *
 * Author: Ingo Molnar <mingo@elte.hu>
 *	   Paul E. McKenney <paulmck@linux.vnet.ibm.com>
 */


#ifdef CONFIG_TREE_PREEMPT_RCU

struct rcu_state rcu_preempt_state = RCU_STATE_INITIALIZER(rcu_preempt_state);
DEFINE_PER_CPU(struct rcu_data, rcu_preempt_data);

/*
 * Tell them what RCU they are running.
 */
static inline void rcu_bootup_announce(void)
{
	printk(KERN_INFO
	       "Experimental preemptable hierarchical RCU implementation.\n");
}

/*
 * Return the number of RCU-preempt batches processed thus far
 * for debug and statistics.
 */
long rcu_batches_completed_preempt(void)
{
	return rcu_preempt_state.completed;
}
EXPORT_SYMBOL_GPL(rcu_batches_completed_preempt);

/*
 * Return the number of RCU batches processed thus far for debug & stats.
 */
long rcu_batches_completed(void)
{
	return rcu_batches_completed_preempt();
}
EXPORT_SYMBOL_GPL(rcu_batches_completed);

/*
 * Record a preemptable-RCU quiescent state for the specified CPU.  Note
 * that this just means that the task currently running on the CPU is
 * not in a quiescent state.  There might be any number of tasks blocked
 * while in an RCU read-side critical section.
 */
static void rcu_preempt_qs_record(int cpu)
{
	struct rcu_data *rdp = &per_cpu(rcu_preempt_data, cpu);
	rdp->passed_quiesc = 1;
	rdp->passed_quiesc_completed = rdp->completed;
}

/*
 * We have entered the scheduler or are between softirqs in ksoftirqd.
 * If we are in an RCU read-side critical section, we need to reflect
 * that in the state of the rcu_node structure corresponding to this CPU.
 * Caller must disable hardirqs.
 */
static void rcu_preempt_qs(int cpu)
{
	struct task_struct *t = current;
	int phase;
	struct rcu_data *rdp;
	struct rcu_node *rnp;

	if (t->rcu_read_lock_nesting &&
	    (t->rcu_read_unlock_special & RCU_READ_UNLOCK_BLOCKED) == 0) {

		/* Possibly blocking in an RCU read-side critical section. */
		rdp = rcu_preempt_state.rda[cpu];
		rnp = rdp->mynode;
		spin_lock(&rnp->lock);
		t->rcu_read_unlock_special |= RCU_READ_UNLOCK_BLOCKED;
		t->rcu_blocked_cpu = cpu;

		/*
		 * If this CPU has already checked in, then this task
		 * will hold up the next grace period rather than the
		 * current grace period.  Queue the task accordingly.
		 * If the task is queued for the current grace period
		 * (i.e., this CPU has not yet passed through a quiescent
		 * state for the current grace period), then as long
		 * as that task remains queued, the current grace period
		 * cannot end.
		 */
		phase = !(rnp->qsmask & rdp->grpmask) ^ (rnp->gpnum & 0x1);
		list_add(&t->rcu_node_entry, &rnp->blocked_tasks[phase]);
		smp_mb();  /* Ensure later ctxt swtch seen after above. */
		spin_unlock(&rnp->lock);
	}

	/*
	 * Either we were not in an RCU read-side critical section to
	 * begin with, or we have now recorded that critical section
	 * globally.  Either way, we can now note a quiescent state
	 * for this CPU.  Again, if we were in an RCU read-side critical
	 * section, and if that critical section was blocking the current
	 * grace period, then the fact that the task has been enqueued
	 * means that we continue to block the current grace period.
	 */
	rcu_preempt_qs_record(cpu);
	t->rcu_read_unlock_special &= ~(RCU_READ_UNLOCK_NEED_QS |
					RCU_READ_UNLOCK_GOT_QS);
}

/*
 * Tree-preemptable RCU implementation for rcu_read_lock().
 * Just increment ->rcu_read_lock_nesting, shared state will be updated
 * if we block.
 */
void __rcu_read_lock(void)
{
	ACCESS_ONCE(current->rcu_read_lock_nesting)++;
	barrier();  /* needed if we ever invoke rcu_read_lock in rcutree.c */
}
EXPORT_SYMBOL_GPL(__rcu_read_lock);

static void rcu_read_unlock_special(struct task_struct *t)
{
	int empty;
	unsigned long flags;
	unsigned long mask;
	struct rcu_node *rnp;
	int special;

	/* NMI handlers cannot block and cannot safely manipulate state. */
	if (in_nmi())
		return;

	local_irq_save(flags);

	/*
	 * If RCU core is waiting for this CPU to exit critical section,
	 * let it know that we have done so.
	 */
	special = t->rcu_read_unlock_special;
	if (special & RCU_READ_UNLOCK_NEED_QS) {
		t->rcu_read_unlock_special &= ~RCU_READ_UNLOCK_NEED_QS;
		t->rcu_read_unlock_special |= RCU_READ_UNLOCK_GOT_QS;
	}

	/* Hardware IRQ handlers cannot block. */
	if (in_irq()) {
		local_irq_restore(flags);
		return;
	}

	/* Clean up if blocked during RCU read-side critical section. */
	if (special & RCU_READ_UNLOCK_BLOCKED) {
		t->rcu_read_unlock_special &= ~RCU_READ_UNLOCK_BLOCKED;

		/* Remove this task from the list it blocked on. */
		rnp = rcu_preempt_state.rda[t->rcu_blocked_cpu]->mynode;
		spin_lock(&rnp->lock);
		empty = list_empty(&rnp->blocked_tasks[rnp->gpnum & 0x1]);
		list_del_init(&t->rcu_node_entry);
		t->rcu_blocked_cpu = -1;

		/*
		 * If this was the last task on the current list, and if
		 * we aren't waiting on any CPUs, report the quiescent state.
		 * Note that both cpu_quiet_msk_finish() and cpu_quiet_msk()
		 * drop rnp->lock and restore irq.
		 */
		if (!empty && rnp->qsmask == 0 &&
		    list_empty(&rnp->blocked_tasks[rnp->gpnum & 0x1])) {
			t->rcu_read_unlock_special &=
				~(RCU_READ_UNLOCK_NEED_QS |
				  RCU_READ_UNLOCK_GOT_QS);
			if (rnp->parent == NULL) {
				/* Only one rcu_node in the tree. */
				cpu_quiet_msk_finish(&rcu_preempt_state, flags);
				return;
			}
			/* Report up the rest of the hierarchy. */
			mask = rnp->grpmask;
			spin_unlock_irqrestore(&rnp->lock, flags);
			rnp = rnp->parent;
			spin_lock_irqsave(&rnp->lock, flags);
			cpu_quiet_msk(mask, &rcu_preempt_state, rnp, flags);
			return;
		}
		spin_unlock(&rnp->lock);
	}
	local_irq_restore(flags);
}

/*
 * Tree-preemptable RCU implementation for rcu_read_unlock().
 * Decrement ->rcu_read_lock_nesting.  If the result is zero (outermost
 * rcu_read_unlock()) and ->rcu_read_unlock_special is non-zero, then
 * invoke rcu_read_unlock_special() to clean up after a context switch
 * in an RCU read-side critical section and other special cases.
 */
void __rcu_read_unlock(void)
{
	struct task_struct *t = current;

	barrier();  /* needed if we ever invoke rcu_read_unlock in rcutree.c */
	if (--ACCESS_ONCE(t->rcu_read_lock_nesting) == 0 &&
	    unlikely(ACCESS_ONCE(t->rcu_read_unlock_special)))
		rcu_read_unlock_special(t);
}
EXPORT_SYMBOL_GPL(__rcu_read_unlock);

#ifdef CONFIG_RCU_CPU_STALL_DETECTOR

/*
 * Scan the current list of tasks blocked within RCU read-side critical
 * sections, printing out the tid of each.
 */
static void rcu_print_task_stall(struct rcu_node *rnp)
{
	unsigned long flags;
	struct list_head *lp;
	int phase = rnp->gpnum & 0x1;
	struct task_struct *t;

	if (!list_empty(&rnp->blocked_tasks[phase])) {
		spin_lock_irqsave(&rnp->lock, flags);
		phase = rnp->gpnum & 0x1; /* re-read under lock. */
		lp = &rnp->blocked_tasks[phase];
		list_for_each_entry(t, lp, rcu_node_entry)
			printk(" P%d", t->pid);
		spin_unlock_irqrestore(&rnp->lock, flags);
	}
}

#endif /* #ifdef CONFIG_RCU_CPU_STALL_DETECTOR */

/*
 * Check for preempted RCU readers for the specified rcu_node structure.
 * If the caller needs a reliable answer, it must hold the rcu_node's
 * >lock.
 */
static int rcu_preempted_readers(struct rcu_node *rnp)
{
	return !list_empty(&rnp->blocked_tasks[rnp->gpnum & 0x1]);
}

#ifdef CONFIG_HOTPLUG_CPU

/*
 * Do CPU-offline processing for preemptable RCU.
 */
static void rcu_preempt_offline_cpu(int cpu)
{
	__rcu_offline_cpu(cpu, &rcu_preempt_state);
}

#endif /* #ifdef CONFIG_HOTPLUG_CPU */

/*
 * Check for a quiescent state from the current CPU.  When a task blocks,
 * the task is recorded in the corresponding CPU's rcu_node structure,
 * which is checked elsewhere.
 *
 * Caller must disable hard irqs.
 */
static void rcu_preempt_check_callbacks(int cpu)
{
	struct task_struct *t = current;

	if (t->rcu_read_lock_nesting == 0) {
		t->rcu_read_unlock_special &=
			~(RCU_READ_UNLOCK_NEED_QS | RCU_READ_UNLOCK_GOT_QS);
		rcu_preempt_qs_record(cpu);
		return;
	}
	if (per_cpu(rcu_preempt_data, cpu).qs_pending) {
		if (t->rcu_read_unlock_special & RCU_READ_UNLOCK_GOT_QS) {
			rcu_preempt_qs_record(cpu);
			t->rcu_read_unlock_special &= ~RCU_READ_UNLOCK_GOT_QS;
		} else if (!(t->rcu_read_unlock_special &
			     RCU_READ_UNLOCK_NEED_QS)) {
			t->rcu_read_unlock_special |= RCU_READ_UNLOCK_NEED_QS;
		}
	}
}

/*
 * Process callbacks for preemptable RCU.
 */
static void rcu_preempt_process_callbacks(void)
{
	__rcu_process_callbacks(&rcu_preempt_state,
				&__get_cpu_var(rcu_preempt_data));
}

/*
 * Queue a preemptable-RCU callback for invocation after a grace period.
 */
void call_rcu(struct rcu_head *head, void (*func)(struct rcu_head *rcu))
{
	__call_rcu(head, func, &rcu_preempt_state);
}
EXPORT_SYMBOL_GPL(call_rcu);

/*
 * Check to see if there is any immediate preemptable-RCU-related work
 * to be done.
 */
static int rcu_preempt_pending(int cpu)
{
	return __rcu_pending(&rcu_preempt_state,
			     &per_cpu(rcu_preempt_data, cpu));
}

/*
 * Does preemptable RCU need the CPU to stay out of dynticks mode?
 */
static int rcu_preempt_needs_cpu(int cpu)
{
	return !!per_cpu(rcu_preempt_data, cpu).nxtlist;
}

/*
 * Initialize preemptable RCU's per-CPU data.
 */
static void __cpuinit rcu_preempt_init_percpu_data(int cpu)
{
	rcu_init_percpu_data(cpu, &rcu_preempt_state, 1);
}

/*
 * Check for a task exiting while in a preemptable-RCU read-side
 * critical section, clean up if so.  No need to issue warnings,
 * as debug_check_no_locks_held() already does this if lockdep
 * is enabled.
 */
void exit_rcu(void)
{
	struct task_struct *t = current;

	if (t->rcu_read_lock_nesting == 0)
		return;
	t->rcu_read_lock_nesting = 1;
	rcu_read_unlock();
}

#else /* #ifdef CONFIG_TREE_PREEMPT_RCU */

/*
 * Tell them what RCU they are running.
 */
static inline void rcu_bootup_announce(void)
{
	printk(KERN_INFO "Hierarchical RCU implementation.\n");
}

/*
 * Return the number of RCU batches processed thus far for debug & stats.
 */
long rcu_batches_completed(void)
{
	return rcu_batches_completed_sched();
}
EXPORT_SYMBOL_GPL(rcu_batches_completed);

/*
 * Because preemptable RCU does not exist, we never have to check for
 * CPUs being in quiescent states.
 */
static void rcu_preempt_qs(int cpu)
{
}

#ifdef CONFIG_RCU_CPU_STALL_DETECTOR

/*
 * Because preemptable RCU does not exist, we never have to check for
 * tasks blocked within RCU read-side critical sections.
 */
static void rcu_print_task_stall(struct rcu_node *rnp)
{
}

#endif /* #ifdef CONFIG_RCU_CPU_STALL_DETECTOR */

/*
 * Because preemptable RCU does not exist, there are never any preempted
 * RCU readers.
 */
static int rcu_preempted_readers(struct rcu_node *rnp)
{
	return 0;
}

#ifdef CONFIG_HOTPLUG_CPU

/*
 * Because preemptable RCU does not exist, it never needs CPU-offline
 * processing.
 */
static void rcu_preempt_offline_cpu(int cpu)
{
}

#endif /* #ifdef CONFIG_HOTPLUG_CPU */

/*
 * Because preemptable RCU does not exist, it never has any callbacks
 * to check.
 */
void rcu_preempt_check_callbacks(int cpu)
{
}

/*
 * Because preemptable RCU does not exist, it never has any callbacks
 * to process.
 */
void rcu_preempt_process_callbacks(void)
{
}

/*
 * In classic RCU, call_rcu() is just call_rcu_sched().
 */
void call_rcu(struct rcu_head *head, void (*func)(struct rcu_head *rcu))
{
	call_rcu_sched(head, func);
}
EXPORT_SYMBOL_GPL(call_rcu);

/*
 * Because preemptable RCU does not exist, it never has any work to do.
 */
static int rcu_preempt_pending(int cpu)
{
	return 0;
}

/*
 * Because preemptable RCU does not exist, it never needs any CPU.
 */
static int rcu_preempt_needs_cpu(int cpu)
{
	return 0;
}

/*
 * Because preemptable RCU does not exist, there is no per-CPU
 * data to initialize.
 */
static void __cpuinit rcu_preempt_init_percpu_data(int cpu)
{
}

#endif /* #else #ifdef CONFIG_TREE_PREEMPT_RCU */
