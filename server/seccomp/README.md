# Seccomp profile for the game server

`docker-default.json` is Docker's default seccomp profile, copied unchanged
from [moby/profiles](https://github.com/moby/profiles) at commit
`4b8214fc4971e06172938a1f1554ccff6647ee80` (`seccomp/default.json`).

## Why it's pinned

The game server and its MySQL plugin are 32-bit (i386). The plugin opens its
database connection through Debian's i386 glibc, which uses the
`socketcall(2)` multiplexer.

Some Docker releases ship a default profile (moby/profiles `seccomp/v0.2.2`
onward) that refuses `socketcall` with ENOSYS. The gamemode then logs
`[db] connection failed (2004): Can't create TCP/IP socket (38)` and can't
reach the database. Upstream reverted that block on 2026-05-06 (commit
`3c28324`) because it broke x86 binaries. Hosts that still have the old
Docker refuse it anyway, as GitHub's CI runners did.

Pinning the current upstream profile keeps the game server exactly as
confined as Docker intends today, whatever Docker version the host runs.
Only the `server` service uses it; the others keep the host's default.

To update: copy `seccomp/default.json` from a newer moby/profiles commit,
and change the commit above.
