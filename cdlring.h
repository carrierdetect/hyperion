/* cdlring.h -- a fixed-size in-memory trace ring for channel diagnosis.
 *
 * WHY THIS EXISTS. Diagnosing a race in the channel code with WRMSG probes
 * does not work: a formatted write through the logging path costs enough to
 * move the race being watched. Measured on the Linux/390 LCS scenario, the
 * rate of one failure mode ran 1, 4, 5, 6 and 7 in ten purely by where probes
 * were placed; one probe set made every run pass; and two probe rounds could
 * not answer their own question because the failure they targeted stopped
 * happening while they watched. The observation was changing the outcome.
 *
 * So: record a few stores per event into a ring, and pay the formatting cost
 * once, later, when the race has already been decided.
 *
 * Inert unless built with -DCDLRING. The shipping binary carries none of it,
 * and "does this binary contain the cdlring command" is the deploy check.
 */
#ifndef _CDLRING_H
#define _CDLRING_H

#define CDLRING_SIZE  32768          /* entries, power of two */

/* Event codes. Keep them stable: the dump prints the number, and logs from
   different builds get compared. */
#define CDLR_MSCH_OK      1          /* MODIFY SUBCHANNEL accepted           */
#define CDLR_MSCH_REFUSE  2          /* ...refused, cc in the cc field       */
#define CDLR_TSCH_STORE   3          /* TEST SUBCHANNEL stored a status      */
#define CDLR_TSCH_EMPTY   4          /* ...stored nothing                    */
#define CDLR_HSCH         5          /* HALT SUBCHANNEL entered              */
#define CDLR_HALT_QUEUE   6          /* halt completion queued               */
#define CDLR_SUSPEND      7          /* suspension recorded                  */
#define CDLR_RESUME       8          /* resume path reached                  */
#define CDLR_RESUME_KEEP  9          /* ...and it kept a pending halt status */
#define CDLR_RSCH        10          /* RESUME SUBCHANNEL, cc in the cc field*/
#define CDLR_CSCH        11          /* CLEAR SUBCHANNEL                     */
#define CDLR_STALE       12          /* parked program disowned as stale     */
#define CDLR_LCS_START   13          /* LCS STARTUP: buffer state on entry   */
#define CDLR_LCS_HALT    14          /* LCS halt/clear: buffer state         */
#define CDLR_LCS_READ    15          /* LCS read entry: buffer state         */
#define CDLR_LCS_CCW     16          /* LCS CCW dispatched, aux = opcode     */

#ifdef CDLRING

typedef struct
{
    U64   tsc;                       /* raw timestamp counter, not wall time */
    U16   devnum;
    BYTE  evt;
    BYTE  cc;
    BYTE  flag2;                     /* dev->scsw.flag2 as found             */
    BYTE  flag3;                     /* dev->scsw.flag3 as found             */
    BYTE  pmcw5;                     /* dev->pmcw.flag5, for the enable bit  */
    BYTE  aux;                       /* event-specific                       */
}
CDLRING_ENT;

extern void cdlring_init ( void );
extern void cdlring_note ( U16 devnum, BYTE evt, BYTE cc,
                           BYTE flag2, BYTE flag3, BYTE pmcw5, BYTE aux );
extern void cdlring_dump ( int last_n );
extern int  cdlring_watching ( U16 devnum );
extern void cdlring_watch    ( U16 devnum, int slot );

/* Record only for the subchannels under investigation. The LCS write channel
   cycles its CCW ring continuously, so recording every device would fill the
   ring with routine traffic in seconds and lose the part that matters. */
#define CDLR( dev, evt, cc, aux )                                       \
    do {                                                                \
        if (cdlring_watching( (dev)->devnum ))                          \
            cdlring_note( (dev)->devnum, (evt), (cc),                   \
                          (dev)->scsw.flag2, (dev)->scsw.flag3,         \
                          (dev)->pmcw.flag5, (aux) );                   \
    } while (0)

#else /* !CDLRING */

#define CDLR( dev, evt, cc, aux )   do {} while (0)

#endif /* CDLRING */
#endif /* _CDLRING_H */
