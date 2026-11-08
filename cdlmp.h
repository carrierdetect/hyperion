/* cdlmp.h -- counters for what a SECOND emulated processor costs.
 *
 * WHY THIS EXISTS. OPEN-ITEMS item 22 of the Genode port measured a second
 * emulated processor costing an I/O-bound MVS job about 50% of its wall-clock
 * where it costs 0.0% on an arithmetic one, and named two candidate links:
 * the compare-and-swap sites, which call sched_yield() -- a no-op stub on
 * Genode -- when a lock is contended, and the broadcast storage-key purge to
 * every other online processor. The cell tried to eliminate the first by
 * counting `sched_yield not implemented` log lines and got zero in all eight
 * measured windows. That elimination was WRONG: the libc issues that warning
 * ONCE PER COMPONENT, so the count saturates at one and every window after
 * the first reads zero whatever happens. A log line cannot count calls.
 *
 * So: count the calls. Nine counters per processor, and the split is the
 * point -- a total cannot distinguish a cost paid because a neighbour EXISTS
 * from one paid because a neighbour is RUNNING:
 *
 *   cs/ts/plo/csp  the yield sites, by instruction. Each fires only when the
 *                  lock was already taken AND sysblk.cpus > 1, so each one is
 *                  both a failed lock acquisition and a sched_yield() call.
 *   sk             STORKEY_INVALIDATE_LOCKED entered with more than one CPU.
 *   ski            ...and another processor's TLB entry was invalidated
 *                  INLINE, which is the cheap branch: that processor was in
 *                  sysblk.waiting_mask, i.e. idle.
 *   sks            ...or the invalidate was SCHEDULED with ON_IC_INTERRUPT,
 *                  which forces a RUNNING processor out of its instruction
 *                  loop. ski against sks is "the neighbour was idle" against
 *                  "the neighbour was working", and that is exactly the
 *                  difference between a per-job cost and a contention cost.
 *   sync           synchronize_cpus() calls (IPTE, PTLB, SSKE, xstore...).
 *   syncw          ...that actually BLOCKED, waiting for every other active
 *                  processor to reach its syncpoint. sync-minus-syncw is free;
 *                  syncw is the real serialisation.
 *
 * Inert unless built with -DCDLMP. The shipping binary carries none of it,
 * and "does the binary contain the CDLMP strings" is the deploy check -- the
 * same discipline cdlring.h records, for the same reason.
 *
 * AND THE PER-REGS MACRO IS CDLMP_HIT, NOT CDLMP, WHICH IS NOT COSMETIC.
 * It was `CDLMP( regs, slot )` for a few hours, and that made this header
 * define a macro with the same name as the BUILD SWITCH -- so `#ifdef CDLMP`
 * in cdlmp.c was true because the header had been included, not because the
 * switch was set. The instrumented build worked by luck; the SHIPPING build
 * did not compile, because the inert branch below defines `cdlmp_tick()` as
 * a macro and cdlmp.c then tried to define it as a function. A switch and a
 * macro must not share a name.
 *
 * COUNTING IS PER PROCESSOR AND UNLOCKED, ON PURPOSE. Each processor
 * increments its own row and no other, so there is no shared counter to
 * serialise on -- an instrument that took a lock to measure lock contention
 * would be measuring itself. Rows are padded apart so two processors do not
 * share a cache line. The reader may see a torn 64-bit value; at these
 * magnitudes that cannot change a conclusion, and a lock here could.
 */
#ifndef _CDLMP_H
#define _CDLMP_H

#define CDLMP_CS     0               /* CS/CDS/CSG/CDSG/CSY/CDSY failed      */
#define CDLMP_TS     1               /* TS found the bit already set         */
#define CDLMP_PLO    2               /* PLO failed                           */
#define CDLMP_CSP    3               /* CSP failed                           */
#define CDLMP_SK     4               /* STORKEY_INVALIDATE_LOCKED, cpus > 1  */
#define CDLMP_SKI    5               /* ...other CPU invalidated inline      */
#define CDLMP_SKS    6               /* ...other CPU's invalidate scheduled  */
#define CDLMP_SYNC   7               /* synchronize_cpus() called            */
#define CDLMP_SYNCW  8               /* ...and it blocked                    */
#define CDLMP_SLOTS  16              /* 128 bytes a row: two cache lines, so
                                        no two processors share one          */
#define CDLMP_CPUS   16              /* rows; index is cpuad masked to this  */

#ifdef CDLMP

extern volatile U64 cdlmp_cnt[ CDLMP_CPUS ][ CDLMP_SLOTS ];

/* The increment. No function call, no lock, no clock read. */
#define CDLMP_BUMP( _cpuad, _slot )                                         \
    do {                                                                    \
        cdlmp_cnt[ (_cpuad) & (CDLMP_CPUS - 1) ][ (_slot) ]++;              \
    } while (0)

#define CDLMP_HIT( _regs, _slot )  CDLMP_BUMP( (_regs)->cpuad, (_slot) )

extern void cdlmp_tick ( void );     /* once a second from timer_thread */

#else /* !CDLMP */

#define CDLMP_BUMP( _cpuad, _slot )  do { } while (0)
#define CDLMP_HIT( _regs, _slot )    do { } while (0)
#define cdlmp_tick()                 do { } while (0)

#endif /* CDLMP */
#endif /* _CDLMP_H */
