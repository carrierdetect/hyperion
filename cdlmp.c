/* cdlmp.c -- the counters themselves, and the only thing that reads them.
 *
 * Compiled ONCE. control.c, esame.c and general1/2.c are each re-included
 * per architecture, so counters defined in any of them would become three
 * sets with the totals split across them and no sign that it had happened --
 * the mistake cdlring.c records avoiding for the same reason.
 */
#include "hstdinc.h"
#include "hercules.h"
#include "cdlmp.h"

#ifdef CDLMP

volatile U64 cdlmp_cnt[ CDLMP_CPUS ][ CDLMP_SLOTS ];

/* How often to report. Seconds of the timer thread's own one-second tick,
   so it needs no clock of its own. Ten is small against the jobs this
   measures (50 to 460 s) and large against the log's capacity. */
#ifndef CDLMP_SECS
#define CDLMP_SECS  10
#endif

/*-------------------------------------------------------------------*/
/* Report, from the timer thread, if anything moved                  */
/*-------------------------------------------------------------------*/
/*                                                                   */
/* ONE LINE PER PROCESSOR, NOT ONE TOTAL. "Which processor pays" is  */
/* a question this port already has evidence for -- the processor     */
/* that lands on host core 0 shares it with the console crosslink,    */
/* the Timer driver and ten of the partition's nineteen threads --    */
/* and a summed line throws that away for nothing.                   */
/*                                                                   */
/* QUIET AT REST. A line is printed only when something changed, so  */
/* a partition doing nothing of interest adds nothing to the log;     */
/* the figures are cumulative, so a reader differences the first and  */
/* last line inside a window and a counter that did not move          */
/* correctly reads as zero.                                          */
/*                                                                   */
/* AND IT NEVER READS A COUNTER TWICE. Each row is copied once into   */
/* a local, then printed and compared from that copy. Reading the     */
/* live counter for the test and again for the print would let a      */
/* value change between them and report a line whose numbers no       */
/* arithmetic reproduces.                                            */
/*-------------------------------------------------------------------*/
void cdlmp_tick( void )
{
    static U64 last[ CDLMP_CPUS ][ CDLMP_SLOTS ];
    static int secs = 0;
    static int banner = 0;

    U64  now[ CDLMP_SLOTS ];
    int  cpu, i, moved;

    if (++secs < CDLMP_SECS)
        return;
    secs = 0;

    if (!banner)
    {
        banner = 1;
        logmsg( "CDLMP counters built in; cs/ts/plo/csp are yield sites,"
                " sk/ski/sks the storage-key broadcast, sync/syncw the"
                " CPU serialisation. Cumulative.\n" );
    }

    for (cpu = 0; cpu < CDLMP_CPUS; cpu++)
    {
        moved = 0;

        for (i = 0; i < CDLMP_SLOTS; i++)
        {
            now[ i ] = cdlmp_cnt[ cpu ][ i ];

            if (now[ i ] != last[ cpu ][ i ])
                moved = 1;
        }

        if (!moved)
            continue;

        logmsg( "CDLMP cpu=%d cs=%"PRIu64" ts=%"PRIu64" plo=%"PRIu64
                " csp=%"PRIu64" sk=%"PRIu64" ski=%"PRIu64" sks=%"PRIu64
                " sync=%"PRIu64" syncw=%"PRIu64"\n",
                cpu,
                now[ CDLMP_CS    ], now[ CDLMP_TS   ], now[ CDLMP_PLO ],
                now[ CDLMP_CSP   ], now[ CDLMP_SK   ], now[ CDLMP_SKI ],
                now[ CDLMP_SKS   ], now[ CDLMP_SYNC ], now[ CDLMP_SYNCW ] );

        for (i = 0; i < CDLMP_SLOTS; i++)
            last[ cpu ][ i ] = now[ i ];
    }
}

#endif /* CDLMP */
