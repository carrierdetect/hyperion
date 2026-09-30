/* cdlring.c -- the trace ring itself. See cdlring.h for why it exists.
 *
 * Compiled ONCE. channel.c is re-included per architecture through ARCH_DEP,
 * so a ring defined there would become three rings and the trace would be
 * split across them without any sign that it had happened.
 */
#include "hstdinc.h"
#include "hercules.h"
#include "cdlring.h"

#ifdef CDLRING

static CDLRING_ENT  cdlring[ CDLRING_SIZE ];
static volatile U32 cdlring_ix = 0;

/* One anchor pair, taken at init and again at dump: the ring stores raw
   timestamp-counter values because reading a real clock on the record path
   costs more than everything else put together. The reader converts with
   these two. */
static U64 cdlr_tsc0;
static TOD cdlr_tod0;

/* The subchannels under investigation. Two LCS devices by default; the
   command can change them at runtime so a rebuild is not needed to look
   somewhere else. */
static U16 cdlr_dev[4] = { 0x0E20, 0x0E21, 0, 0 };

int cdlring_watching( U16 devnum )
{
    int i;
    for (i=0; i < (int)(sizeof(cdlr_dev)/sizeof(cdlr_dev[0])); i++)
        if (cdlr_dev[i] && cdlr_dev[i] == devnum)
            return 1;
    return 0;
}

void cdlring_watch( U16 devnum, int slot )
{
    if (slot >= 0 && slot < (int)(sizeof(cdlr_dev)/sizeof(cdlr_dev[0])))
        cdlr_dev[ slot ] = devnum;
}

void cdlring_init( void )
{
    memset( cdlring, 0, sizeof( cdlring ));
    cdlring_ix = 0;
    cdlr_tsc0  = (U64) __builtin_ia32_rdtsc();
    cdlr_tod0  = host_tod();
}

/* The whole point: no lock, no formatting, no clock syscall. One relaxed
   atomic increment and a handful of stores. Entries are overwritten in place
   when the ring wraps, and a reader may catch one half-written -- which is
   accepted, and said so in the dump, because taking a lock here would put
   back the cost this exists to avoid. */
void cdlring_note( U16 devnum, BYTE evt, BYTE cc,
                   BYTE flag2, BYTE flag3, BYTE pmcw5, BYTE aux )
{
    U32 i;

    /* Anchor on first use. Nothing in Hercules' start-up calls cdlring_init(),
       and an uninitialised anchor makes every timestamp an absolute counter
       value that cannot be lined up against anything. One compare per event. */
    if (!cdlr_tsc0)
    {
        cdlr_tsc0 = (U64) __builtin_ia32_rdtsc();
        cdlr_tod0 = host_tod();
    }

    i = __atomic_fetch_add( &cdlring_ix, 1, __ATOMIC_RELAXED )
        & (CDLRING_SIZE - 1);
    CDLRING_ENT* e = &cdlring[i];

    e->tsc    = (U64) __builtin_ia32_rdtsc();
    e->devnum = devnum;
    e->evt    = evt;
    e->cc     = cc;
    e->flag2  = flag2;
    e->flag3  = flag3;
    e->pmcw5  = pmcw5;
    e->aux    = aux;
}

static const char* cdlr_evtname( BYTE evt )
{
    switch (evt)
    {
        case CDLR_MSCH_OK:     return "msch-ok  ";
        case CDLR_MSCH_REFUSE: return "msch-ref ";
        case CDLR_TSCH_STORE:  return "tsch-str ";
        case CDLR_TSCH_EMPTY:  return "tsch-nil ";
        case CDLR_HSCH:        return "hsch     ";
        case CDLR_HALT_QUEUE:  return "halt-q   ";
        case CDLR_SUSPEND:     return "suspend  ";
        case CDLR_RESUME:      return "resume   ";
        case CDLR_RESUME_KEEP: return "resume-kp";
        case CDLR_RSCH:        return "rsch     ";
        case CDLR_CSCH:        return "csch     ";
        case CDLR_STALE:       return "stale    ";
        case CDLR_LCS_START:   return "lcs-start";
        case CDLR_LCS_HALT:    return "lcs-halt ";
        case CDLR_LCS_READ:    return "lcs-read ";
        default:               return "?        ";
    }
}

/* Expensive by design, and that is fine: it runs after the race has been
   decided. Paced, because the log path drops lines when it is pushed. */
void cdlring_dump( int last_n )
{
    U32 total = cdlring_ix;
    U32 have  = total < CDLRING_SIZE ? total : CDLRING_SIZE;
    U32 show  = (last_n > 0 && (U32)last_n < have) ? (U32)last_n : have;
    U32 start = total - show;
    U64 tsc1  = (U64) __builtin_ia32_rdtsc();
    TOD tod1  = host_tod();
    U32 k;

    // "CDLRING %s"
    WRMSG( HHC01374, "I", "dump begins; entries may be torn, no lock is taken" );
    logmsg( "CDLRING anchor tsc0=%16.16"PRIX64" tod0=%16.16"PRIX64"\n",
            cdlr_tsc0, (U64) cdlr_tod0 );
    logmsg( "CDLRING anchor tsc1=%16.16"PRIX64" tod1=%16.16"PRIX64"\n",
            tsc1, (U64) tod1 );
    logmsg( "CDLRING %u recorded, %u kept, showing %u\n", total, have, show );
    logmsg( "CDLRING     seq dev  event     cc fl2 fl3 pmcw5 aux tsc-delta\n" );

    for (k = start; k < total; k++)
    {
        CDLRING_ENT* e = &cdlring[ k & (CDLRING_SIZE - 1) ];

        if (!e->evt)
            continue;

        logmsg( "CDLRING %7u %04X %s %2d  %02X  %02X   %02X   %02X %"PRIu64"\n",
                k, e->devnum, cdlr_evtname( e->evt ), e->cc,
                e->flag2, e->flag3, e->pmcw5, e->aux,
                (U64)(e->tsc - cdlr_tsc0) );

        /* The log path drops lines when pushed; give it room. */
        if ((k & 0x1F) == 0x1F)
            USLEEP( 2000 );
    }
    WRMSG( HHC01374, "I", "dump ends" );
}

#endif /* CDLRING */
