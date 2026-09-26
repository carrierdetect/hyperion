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
| `49152ec668` | **LCS loses a Halt Subchannel** that arrives between reads, so the device stops responding. |
| `3e57da7f5c` | **The 1052/3215 console loses a `/command`**, and loses the attention interrupt too, when input arrives while it is busy. A one-slot buffer. |
| `d6f6ea2d91` | **`ckddasd` dereferences `dev->ckdcu` without checking it.** |

None of these mention `__GENODE__`; they were found here because this port
stresses those paths, but the fault is in the common code.

### Genode platform support — guarded on `__GENODE__`, inert elsewhere

`b4518693fe` `hostopts.h` · `eefe5b106b` `hscutl.c` · `9180be94cc` static HDL
module support for a platform with no `dlopen` · `8a51ef8cd9` read operator
commands from stdin only when there is one · `7fec54d9ca` derive the TOD clock
from Genode's interpolated clock rather than the libc's · `a91c41d5c5`
`socket_is_socket()` · `1f90e663a5` make the Nic session's MAC authoritative
for LCS · `1537951ef2` exclude `struct rtentry` from `CTLREQ`.

`9180be94cc` may be of wider interest: it is static HDL module linking for any
platform without `dlopen`, not only this one.

### New device forms for this deployment — guarded

| commit | what |
|---|---|
| `755bb620c6` | `0600 CTCT lpar=b` — a CTC between two emulated machines on one host, over a Genode crosslink instead of TCP. |
| `0785e84ab4` | `0090 tcpnje 2703 lnode=A rnode=B lpar=b` — the same for NJE's BSC line. |

Both are **additional** forms, not replacements: `CTC_TCP=1` / `NJE_TCP=1` at
build time restore the TCP path, which remains the only way to reach a Hercules
on another host. Between two partitions of one machine there is no network to
cross, and removing it removes the addressing and the connection state with it.

## What this fork is not

It is **not** a proposed upstream series and has not been submitted. The three
platform-independent fixes above are the only part written with that in mind;
everything else is specific to running under Genode. Issues and pull requests
here are welcome but this is not upstream — file Hercules bugs with
[SDL Hyperion](https://github.com/SDL-Hercules-390/hyperion).
