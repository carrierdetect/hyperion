# The `cdl/main` branch

**If you arrived here from a binary or a source offer, this is the branch that
binary was built from.**

This is a fork of [SDL Hyperion](https://github.com/SDL-Hercules-390/hyperion),
the System/370, ESA/390 and z/Architecture emulator. `cdl/main` carries changes
made by **Carrier Detect Labs** to run Hercules on
**[Genode](https://genode.org)**, an OS framework in which every program is a
component with no ambient authority — it can reach only what its parent
explicitly routed to it, and runs on a microkernel rather than a
general-purpose OS. The aim is to give each emulated mainframe the kind of
resource isolation that real hardware partitioning provides.

`master` is upstream, untouched. The base of this branch is `ae9b497`.

Licence is unchanged: **QPL 1.0**, as upstream. QPL §4c requires modifications
to be available under QPL, which this branch is.

## Reading a commit here

Each commit message carries the reasoning, the measurements, and usually the
approaches that failed first. They were written for people working on this
port, so a few terms recur:

* **Genode session** — a connection to a service, granted by policy. The
  substitute for opening a socket.
* **`terminal_crosslink`** — a Genode component joining exactly two clients so
  each reads what the other writes. A cable, not a connection.
* **"over IPC"** — carried over a Genode session instead of a TCP socket.
* **LPAR** — here, one emulated machine among several on one host, labelled
  `a`, `b`, `c`, `d`. Not System z firmware partitioning.
* **CTC** — Channel-To-Channel Adapter: a cable between the I/O channels of two
  mainframes. **NJE** — Network Job Entry: the protocol by which two mainframes
  send each other batch jobs. Both normally run over TCP in Hercules.
* **genherc** — the target machine these changes were measured on; a small
  x86-64 box booting Genode/NOVA with no general-purpose OS.

## What is on this branch, and which parts might interest you

Three kinds of change. The distinction matters: most of this is only meaningful
on Genode, but a few are defects that affect **every** platform, and those are
the ones worth a look regardless of what you run on.

### Bugs that affect every platform — unguarded, and upstream candidates

| commit | what |
|---|---|
| `c37ce847` | **LCS loses a Halt Subchannel** that arrives between reads, so the device stops responding. |
| `92dac634` | **The 1052/3215 console loses a `/command`**, and loses the attention interrupt too, when input arrives while it is busy. A one-slot buffer. |
| `3a61e5f2` | **`ckddasd` dereferences `dev->ckdcu` without checking it.** |
| `1abfbac0` | **CLEAR SUBCHANNEL does not stop a channel program parked on a suspend CCW**, so a device thread queued before the clear resumes it afterwards and the subchannel goes active under a guest that has just cleared and disabled it. |
| `3b8ef260` | **The chain executor's per-CCW test for a clear is dead code**: `clear_subchan()` sets the signalling bit and `perform_clear_subchan()` clears it again in the same critical section. |
| `48af8982` | **A halted channel program publishes a second ending status.** A handler blocking in a read returns with no unit status, the halt test finds its flag already cleared, and the normal chain end synthesises alert status onto what may already be the guest's next program. |

None of these mention `__GENODE__`; they were found here because this port
stresses those paths, but the fault is in the common code.

The last three are one defect at three sites: a flag used to signal the device
thread that its channel program has been terminated, erased by the code that
completes the termination before the thread can observe it. Written up for that
project's maintainers, prose and no diff, in the companion repository under
`upstream/hercules/03-clear-and-halt-do-not-stop-the-chain.md`. Affects any guest
that suspends a channel program or halts a device handler that blocks in a read,
which in-tree is LCS, CTCI, QETH and the 3270.

### Genode platform support — guarded on `__GENODE__`, inert elsewhere

`665f6d3c` `hostopts.h` · `7c0b3bab` `hscutl.c` · `1631976f` static HDL
module support for a platform with no `dlopen` · `8cb1bd0a` read operator
commands from stdin only when there is one · `0ba6fc61` derive the TOD clock
from Genode's interpolated clock rather than the libc's · `85b2b6a7`
`socket_is_socket()` · `c082c94d` make the Nic session's MAC authoritative
for LCS · `a31b9117` exclude `struct rtentry` from `CTLREQ` · `a44f8a04` a
weak `herc_log_sink()` hook in `flog_write()`.

`1631976f` may be of wider interest: it is static HDL module linking for any
platform without `dlopen`, not only this one.

`a44f8a04` is worth a sentence because the obvious alternative does not work.
On this platform Hercules' message stream IS the machine's one shared serial
log, so a channel trace has to be taken OUT of it -- 1,277,645 bytes in one
measured window, 87% of what that line can carry. `registerLogCallback()`
cannot do that: it is driven from `log_read()` on the logger's circular
buffer, downstream of the logger thread that has already written those bytes
to the log, it hands over byte ranges on no line boundary, and registering one
makes `impl()` return ahead of the No-UI path. The hook is instead one weak
symbol offered each formatted message in `flog_write()`, returning how many
bytes it took from the front; a build without the symbol is unchanged.

### New device forms for this deployment — guarded

| commit | what |
|---|---|
| `87421f7c` | `0600 CTCT lpar=b` — a CTC between two emulated machines on one host, over a Genode crosslink instead of TCP. |
| `0e5be0ed` | `0090 tcpnje 2703 lnode=A rnode=B lpar=b` — the same for NJE's BSC line. |

Both are **additional** forms, not replacements: `CTC_TCP=1` / `NJE_TCP=1` at
build time restore the TCP path, which remains the only way to reach a Hercules
on another host. Between two partitions of one machine there is no network to
cross, and removing it removes the addressing and the connection state with it.

### Diagnostics, compiled out of the shipping binary

Two instruments live in this tree and **neither is in a binary unless it was
asked for at build time**. Both exist because a `WRMSG` probe, or a log line,
could not answer the question:

| file | built by | what it answers |
|---|---|---|
| `cdlring.c/h` | `-DCDLRING` | a fixed-size in-memory ring of channel events, dumped once by the `cdlring` command. Formatting a probe through the logging path cost enough to MOVE the race being diagnosed: the failure rate of one channel defect ran 1, 4, 5, 6 and 7 in ten purely by where the probes were placed, and one probe set made every run pass. |
| `cdlmp.c/h` | `-DCDLMP` | nine counters per emulated processor for what a SECOND processor costs: the four failed-lock instructions that call `sched_yield()` (`CS`/`CDS`, `TS`, `PLO`, `CSP`), the storage-key broadcast purge split by whether the other processor was idle or running, and `synchronize_cpus()` split by whether it actually blocked. |

`cdlmp` was added because counting log lines gave a wrong answer with every
appearance of a right one. Genode's C library warns `sched_yield not
implemented` **once per component**, so a count of those lines saturates at
one: four measured windows all read zero, and that was taken as evidence the
yield sites had stayed cold. They may not have. A saturating counter cannot
distinguish "it never happened" from "the first one happened earlier", and
the distinction was the whole question.

The counters are per processor and unlocked, each processor incrementing only
its own row, padded so that no two share a cache line — an instrument that
took a lock to measure lock contention would be measuring itself.

## What this fork is not

It is **not** a proposed upstream series and has not been submitted. The six
platform-independent fixes above are the only part written with that in mind;
everything else is specific to running under Genode. Issues and pull requests
here are welcome but this is not upstream — file Hercules bugs with
[SDL Hyperion](https://github.com/SDL-Hercules-390/hyperion).
