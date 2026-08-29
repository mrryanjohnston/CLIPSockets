# CLIPSockets

The missing networking library for
[CLIPS](https://www.clipsrules.net/).
Write CLIPS code that can talk to the internet!

## Forward

* This repository is for educational purposes only :)
* I have only tested this codebase in Ubuntu 23.10 and 24.04 so far
* the library is only built for linux-based systems for now
* Don't know CLIPS? Try the
  [Tour of CLIPS](https://ryjo.codes/tour-of-clips.html)
  I wrote to learn!

## What the Library Does

This library gives low-level network functions to
[CLIPS](https://www.clipsrules.net/)
environments. The functions operate on socket file descriptors. A CLIPS
application can thus send messages to other computers on a network, and
receive messages from them. All the reads and the writes go through an I/O
router[^1].

[^1]: See **Section 9: I/O Routers** of the [CLIPS Advanced Programming Guide](https://www.clipsrules.net/documentation/v641/apg641.pdf)

With these functions you can write network servers and network clients. That
is, a CLIPS application can *receive* network requests and *make* network
requests.

```clips
         CLIPS (6.4.2 1/14/25)
CLIPS> (create-socket AF_INET SOCK_STREAM)
3
CLIPS> (bind-socket 3 127.0.0.1 8889)
127.0.0.1:8889
CLIPS> (listen 3) ; NOTE: 127.0.0.1:8889 is also correct here
TRUE
CLIPS> (accept 3) ; NOTE: 127.0.0.1:8889 is also correct here
4
CLIPS> (get-socket-logical-name 4)
127.0.0.1:42616#4
CLIPS> (readline 127.0.0.1:42616#4)

```

A client for that server looks like this:

```clips
         CLIPS (6.4.2 1/14/25)
CLIPS> (create-socket AF_INET SOCK_STREAM)
3
CLIPS> (connect 3 127.0.0.1 8889)
127.0.0.1:8889#3
CLIPS> (printout 127.0.0.1:8889#3 "Hello, server :)" crlf)
CLIPS> (flush-connection 3) ; NOTE: 127.0.0.1:8889#3 is also correct here
TRUE
```

When the client flushes the connection, the message shows in the output
of the server.

The library also finds addresses in the DNS. To find the IP addresses of a
domain name, use `(resolve-domain-name ryjo.codes)`. If the result contains
the same address more than one time, use
`(intersection$ (resolve-domain-name ryjo.codes))`.

## Long-Term Goals

I would like the quality of this code to be sufficient for upstream CLIPS itself someday.

## Installation

Build the project from the root directory:

```
make
```

That fetches the CLIPS source, copies the files this project adds over it, and
builds. The binary is `./clips` in the root of the project.

### Which CLIPS

The build works against three sources of CLIPS, and `CLIPS_VERSION` selects
one:

| Value | What it is |
| --- | --- |
| `6.4.2` | The released tarball from SourceForge. This is the default. |
| `svn-6x` | `branches/64x/core` of the CLIPS Subversion repository, the 6.4 maintenance branch. |
| `svn-7x` | `branches/70x/core`, the 7.0 development branch. This is the branch that adds the deftable construct and goal-driven facts. |

```
make                          # CLIPS 6.4.2
make CLIPS_VERSION=svn-6x
make CLIPS_VERSION=svn-7x
```

The two `svn-` legs need `svn` installed. The package is `subversion` on
Debian, Ubuntu and Fedora. The `6.4.2` leg needs only `curl` or `wget`, and
the tarball is checked against the SHA-256 published for it.

Build a branch at another
revision, or at whatever is there now, with `CLIPS_SVN_REV`:

```
make CLIPS_VERSION=svn-7x CLIPS_SVN_REV=1002
make CLIPS_VERSION=svn-7x CLIPS_SVN_REV=HEAD
```

`HEAD` is how a change upstream is found while it is still on a branch. The
directory a tree is fetched into is named after the revision asked for, so
`CLIPS_SVN_REV=HEAD` reuses whatever `HEAD` meant the first time; remove that
directory to fetch it again.

### Where it all goes

```
vendor/clips-source/<tag>   the tree as it was fetched, never written to
vendor/clips-build/<tag>    a copy of it, with this project's files over it
vendor/clips                a symlink to the version built last
./clips                     the binary
```

`<tag>` is `6.4.2`, `svn-6x-r967` or `svn-7x-r978`.
`make print-clips` says which version a build uses and
where each of those is.

`make clean` removes the build trees and the binary and keeps the fetched
sources, which are the slow half to get back. `make distclean` removes those
too, along with the release tarball beside the repository.

`make test-clips` builds against all three in turn and runs the whole suite on
each:

```
6.4.2    6.4.2            files: 61 passed, 0 failed, 1 skipped  PASSED
svn-6x   svn-6x-r967      files: 61 passed, 0 failed, 1 skipped  PASSED
svn-7x   svn-7x-r978      files: 61 passed, 0 failed, 1 skipped  PASSED
```

### TLS

This build has TLS. To build with no TLS,
which needs no library more than the C standard library:

```
make no-tls
# or
make TLS=0
```

TLS needs the development headers of one of seven libraries: OpenSSL,
LibreSSL, BoringSSL, wolfSSL, mbedTLS, GnuTLS and s2n-tls.
The build looks for four of them with pkg-config, in this order: `libssl` and
`libcrypto` together, then `wolfssl`, then `mbedtls`, `mbedx509` and
`mbedcrypto` together, then `gnutls`. It builds against the first it finds, and
says which one that was.

OpenSSL, LibreSSL and BoringSSL all install `libssl.pc` and `libcrypto.pc`
under those names, so the first of those searches matches any of the three. The
build tells them apart by their headers and reports the one it found. A
LibreSSL installation in the usual directories thus needs no arguments.

To select a library yourself or to use an installation in an unusual
directory, specify `TLS_BACKEND` and/or `TLS_PREFIX`:

```
make TLS_BACKEND=libressl
make TLS_BACKEND=gnutls
make TLS_BACKEND=wolfssl TLS_PREFIX=/opt/wolfssl
make TLS_BACKEND=boringssl TLS_PREFIX=/opt/boringssl
make TLS_BACKEND=s2n TLS_PREFIX=$HOME/opt/s2n
```

`(tls-backend)` gives the name of the library in a binary, and
`(tls-backend-version)` gives its version. The protocol versions read the same
for all the libraries: `(tls-version)` gives `TLSv1.2` or `TLSv1.3`.

## `make`

`make help` shows the targets. The makefile makes this list from its own
contents, so the list always agrees with the file:

```
  all            Build clips with TLS (the default)
  asan           Rebuild under AddressSanitizer and run the suite
  clean          Remove the binary and every build tree
  clips-source   Fetch the selected CLIPS source and stop
  coverage-all   Line coverage merged across every TLS library
  coverage       Line coverage for the current build's TLS backend
  debug          Build with debugging symbols and no optimisation
  distclean      Also remove the fetched CLIPS source and the archive
  help           List these targets
  magic          Build with the optional libmagic dependency, enabling the (mimetype) function
  matrix         Build and test every configuration this machine can build
  no-tls         Build with no TLS
  print-clips    Say which CLIPS this build uses and where it is
  provision      Report which TLS libraries are present and how to get the rest
  release        Synonym for the default build
  test-%         Build and test one configuration, e.g. "make test-gnutls"
  test-clips     Build and test against 6.4.2, branches/64x and branches/70x
  test-list      Name the configurations this machine can build
  test           Run the test suite against the current build
  tls            Synonym for the default build
```

`make help` also prints which CLIPS the build uses and the revision each
branch is pinned at.

`make matrix` builds all the configurations that the machine can build. This
is each TLS library that it finds, with libmagic and without libmagic, and
also a build with no TLS. The suite then runs against each configuration. If a
library cannot do what a check asks, the suite skips that check and prints the
reason. A skipped check is not a failure. On a machine that has all the
libraries, `make matrix` prints this:

```
CONFIGURATION      BACKEND    VERSION                      RESULT
-------------      -------    -------                      ------
no-tls             -          -                            files: 37 passed, 0 failed, 25 skipped
no-tls+magic       -          -                            files: 38 passed, 0 failed, 24 skipped
system             openssl    OpenSSL 3.0.13 30 Jan 2024   files: 61 passed, 0 failed, 1 skipped
system+magic       openssl    OpenSSL 3.0.13 30 Jan 2024   files: 62 passed, 0 failed, 0 skipped
wolfssl            wolfssl    5.9.2                        files: 61 passed, 0 failed, 1 skipped
wolfssl+magic      wolfssl    5.9.2                        files: 62 passed, 0 failed, 0 skipped
mbedtls            mbedtls    2.28.10                      files: 61 passed, 0 failed, 1 skipped; checks: 4 skipped
mbedtls+magic      mbedtls    2.28.10                      files: 62 passed, 0 failed, 0 skipped; checks: 4 skipped
gnutls             gnutls     3.8.3                        files: 61 passed, 0 failed, 1 skipped
gnutls+magic       gnutls     3.8.3                        files: 62 passed, 0 failed, 0 skipped
openssl            openssl    OpenSSL 3.5.7 9 Jun 2026     files: 61 passed, 0 failed, 1 skipped
openssl+magic      openssl    OpenSSL 3.5.7 9 Jun 2026     files: 62 passed, 0 failed, 0 skipped
libressl           libressl   LibreSSL 4.3.2               files: 61 passed, 0 failed, 1 skipped
libressl+magic     libressl   LibreSSL 4.3.2               files: 62 passed, 0 failed, 0 skipped
mbedtls3           mbedtls    3.6.7                        files: 61 passed, 0 failed, 1 skipped
mbedtls3+magic     mbedtls    3.6.7                        files: 62 passed, 0 failed, 0 skipped
mbedtls4           mbedtls    4.1.1                        files: 61 passed, 0 failed, 1 skipped
mbedtls4+magic     mbedtls    4.1.1                        files: 62 passed, 0 failed, 0 skipped
s2n                s2n        s2n-tls                      files: 57 passed, 0 failed, 5 skipped; checks: 9 skipped
s2n+magic          s2n        s2n-tls                      files: 58 passed, 0 failed, 4 skipped; checks: 9 skipped
boringssl          boringssl  OpenSSL 1.1.1 (compatible; BoringSSL) files: 57 passed, 0 failed, 5 skipped; checks: 6 skipped
boringssl+magic    boringssl  OpenSSL 1.1.1 (compatible; BoringSSL) files: 58 passed, 0 failed, 4 skipped; checks: 6 skipped

matrix: all 22 configurations passed
```

Some libraries skip files because they do not support DTLS or TLS 1.3.

The `+magic` rows are builds with libmagic. Each of them passes one
more test file `mimetype.clp`. The function `(mimetype)`
is optional and needs libmagic. libmagic is the
library of the `file` command. The package is `libmagic-dev` on Debian and
Ubuntu, and `file-devel` on Fedora. Install the headers, then build:

```
make magic
# or
make MAGIC=1
```

| | |
| --- | --- |
| `make` | Build with TLS. The build selects a library. |
| `make magic` | The same build, with libmagic. `(mimetype)` needs libmagic. |
| `make no-tls` | Build with no TLS. This needs no library more than the C standard library. |
| `make clean` | Remove the binary and the build trees, for all the backends and all the CLIPS versions, not for the selected one only. The fetched CLIPS source stays. |
| `make distclean` | The same, and also remove `vendor/` and the release tarball. The next build fetches CLIPS again. |

These variables control a build. The root makefile sends them to `clips.mk`,
the makefile it copies into the build tree, so you can use more than one
together: `make magic TLS_BACKEND=gnutls CLIPS_VERSION=svn-7x`.
Switching any of them rebuilds from clean.

| Variable | Values |
| --- | --- |
| `CLIPS_VERSION` | `6.4.2`, `svn-6x` or `svn-7x`. The default is `6.4.2`. See [Which CLIPS](#which-clips). |
| `CLIPS_SVN_REV` | A Subversion revision, or `HEAD`. It applies to `svn-6x` and `svn-7x` only, and each is pinned to a revision without it. |
| `TLS_BACKEND` | `openssl`, `libressl`, `wolfssl`, `mbedtls`, `gnutls`, `s2n`, `boringssl`, or `auto`. The default is `auto`. |
| `TLS_PREFIX` | The directory of the installation, when the compiler does not find the library itself. `boringssl` and `s2n` always need this, because they have no pkg-config files. |
| `MAGIC` | `1` to build `(mimetype)`. `make magic` sets this. |

### Run the Tests

| | |
| --- | --- |
| `make test` | Run the suite against the current build. This takes seconds. |
| `make test-list` | Show the configurations that this machine can build. |
| `make test-<name>` | Build one of those configurations from clean and run the suite against it: `make test-gnutls`, `make test-no-tls`. |
| `make matrix` | All the configurations, each one with libmagic and without libmagic. This takes minutes, because each build starts from clean. |
| `make asan` | Build again with AddressSanitizer and run the suite. This takes minutes. |
| `make provision` | Show which TLS libraries are installed, which ones this machine can build, and which ones the script can remove. This changes nothing. |

`make test-<name>` is `./tests/backend.sh <name>`. The script has two options
that the make target does not use. `--magic` adds libmagic to the build.
`--coverage DIR` makes an instrumented build and puts its line data in `DIR`,
for `tests/coverage-all.sh` to add up later. `make matrix`, `make
coverage-all` and the CI jobs are all loops around this one script. A
configuration thus behaves the same in each of them.

`make asan` is a test that finds "read after the end of allocation" faults.

The script takes a configuration name, like the other scripts. After the name,
it takes the names of test files:

```
./tests/asan.sh                  the default build
./tests/asan.sh gnutls           one configuration
./tests/asan.sh gnutls tests/integration/tls-loopback.clp
```

`make provision` is used to download and make available tls libraries:

```
./tests/provision.sh --build        build the libraries that are missing
./tests/provision.sh --build s2n    build one of them
./tests/provision.sh --remove       delete all the libraries that it built
./tests/provision.sh --remove s2n   delete one of them
```

`--build` puts each library in `$HOME/opt`. The version of each library
is in `tests/provision.sh`, and the script compares each download against a
digest or a commit.

`--remove` deletes only the directories that the script made.
The report shows which directories these are. It shows the
libraries of the system in a different list, because they are not the property
of this script. Neither option uses `sudo`. For a library that comes from a
package, the script prints the `apt` command, and you run it yourself.

`make matrix` is a loop around `make test-<name>`. One configuration thus
behaves the same alone and in the group. The matrix also reports the
configurations that it could not build.

### Measure the Coverage

| | |
| --- | --- |
| `make coverage` | An instrumented build, the suite, and the line coverage of the TLS backend in that build. |
| `make coverage-all` | The same for each configuration, added together. A line is covered if one build covered it. |

A binary contains one TLS backend only. `make coverage` thus speaks for one
backend, and `make coverage-all` measures all of them.

### Continuous Integration

`.github/workflows/tests.yml` runs on each push and each pull request. It runs
the same scripts as this section, against the same libraries. A failure in CI
is thus a failure that one command can repeat on your machine.

| Job | What it runs | Rows |
| --- | --- | --- |
| `suite` | `make`, then `make test` | 1, the build that `make` gives you |
| `clips-versions` | `make CLIPS_VERSION=<v>`, then `make test` | 3: `6.4.2`, `svn-6x` and `svn-7x` |
| `backends` | `./tests/backend.sh <name>`, then the same with `--magic` | 11 |
| `asan` | `./tests/asan.sh <name>` | 11 |
| `coverage` | `./tests/backend.sh <name> --magic --coverage`, then an upload | 11 |
| `coverage-report` | the merge of those 11 sets of data | 1 |

The eleven configurations are `no-tls`, `system`, and nine TLS builds:
`openssl`, `libressl`, `wolfssl`, `mbedtls`, `mbedtls3`, `mbedtls4`, `gnutls`,
`s2n` and `boringssl`. The workflow builds each library from source, at the
version in `tests/provision.sh`, and compares it against a digest or a commit.
It keeps the result in a cache in `~/opt`, with `tests/provision.sh` as the
key.

`clips-versions` is the one job whose rows are not TLS libraries. All three
are fixed -- the release by its digest, each branch by the revision the
makefile pins it at.

## Tests

```
make test
```

To run only some of the files, give their names:

```
./tests/run.sh tests/unit/errno.clp tests/integration/tls-loopback.clp
```

Each file runs in its own `clips` process and prints its own counts. A test
that needs a peer makes both ends in one process on the loopback interface.
A file passes only if it exits with 0 *and* prints its summary line.
A file that stops in the middle is a failure.

If a build does not have what a file needs, the suite skips the file and gives
the reason. The markers `requires: libmagic`, `requires: tls` and
`requires: dtls` in the header of a file are what `run.sh` reads. The suite
skips single checks in the same manner and prints the reason when the TLS
library cannot do what the check asks.

The TLS tests need a certificate authority and a server certificate.
Use `./tests/fixtures/regenerate.sh` to create them.
If the machine has no `openssl` command, the TLS test files skip.

To write a test, put the body in a `deffunction`. A top-level `(bind ?x ...)`
does not stay from one batch command to the next one. Then give the number of
checks in the file:

```
(load* "tests/lib/expect.clp")
(test-suite "my-suite")
(test-plan 2)

(deffunction run-tests ()
   (expect-eq   "two plus two" 4 (+ 2 2))
   (expect-true "sockets can be created" (create-socket AF_INET SOCK_STREAM)))

(run-tests)
(test-summary)
```

`expect.clp` contains assertions like `expect-eq`, `expect-true`,
`expect-false`, `expect-contains`, and `expect-errno`. It also
contains `test-skip`, which records a check that the backend cannot answer.
Two more files are adjacent to it, and only the tests that need them load
them. `tests/lib/socket.clp` makes pairs of endpoints. `tests/lib/tls.clp`
asks the backend what it can do, and drives the two halves of a DTLS handshake
in one process.

### Coverage

```
make coverage
```

This builds with instruments, runs the suite, and reports the line coverage of
the code that this library adds to CLIPS.

A build contains one TLS backend only, so one run can measure one backend
only. To build all possible backends and determine coverage on them:

```
make coverage-all
```

This builds each configuration in sequence, keeps the line data of each one,
and adds them together. A line is covered if one build covered it. This is the
only number that speaks for all the backends. The build with no TLS is in the
loop also.

## Examples

The simple server receives one TCP connection and then exits. The complex
server holds more than one connection at the same time. It continues until you
push `ctrl+z` and then kill it.

```
./clips -f2 examples/server-simple.bat
```

or

```
./clips -f2 examples/server-complex.bat
```

`server-simple-ipv6.bat` and `server-simple-unix.bat` are the same server on
IPv6 and on a UNIX domain socket. `server-udp.bat` and
`server-udp-sendto.bat` send one datagram back to the client. The first one
replies with `connect` and `printout`. The second one replies with `sendto`.

`server-http-file.bat` sends the files of the working directory on HTTP. It
gives the content type of each file with `(mimetype)`, so it needs the
optional libmagic build:

```
make magic
./clips -f2 examples/server-http-file.bat
```

### The Example Clients

```
./clips -f2 examples/client.bat
```

`client-udp.bat` and `client-udp-sendto.bat` speak to the UDP servers.
`ab-clone.bat` is a small client for measurements. It makes many requests to a
server on port 8888.

### The Examples with TLS

These examples need a build with TLS, and a certificate. The repository does
not contain the certificates. Make them one time in each checkout:

```
./tests/fixtures/regenerate.sh
```

Then start a server in one terminal, and its client in a second terminal:

| Server | Client | Notes |
| --- | --- | --- |
| `server-simple-tls.bat` | `client-tls.bat` | `server-simple.bat` with a context at the start, and one `(tls-accept)` between the accept and the first read. |
| `server-complex-tls.bat` | `client-tls.bat` | The concurrent server. It does the handshake in the rules engine, and not in a loop of its own. It loads `server-complex.clp` without a change and adds the handshake to it. |
| `server-http-file-tls.bat` | `curl --cacert tests/fixtures/ca.pem https://localhost:8888/` | HTTPS, with no new CLIPS code. The three files behind it are the files of the plaintext examples. |
| `server-dtls.bat` | `client-dtls.bat` | The same echo server on DTLS, on a UDP socket, on port 9443. |

Ask for `localhost`, and not for `127.0.0.1`. Also tell the client where the
certificate authority is. The certificate has the name localhost in it. Its
authority is in the `tests/fixtures` directory, and no other computer trusts
that authority. A browser will give you a warning for the HTTPS example.

`client-https.bat` needs no server and no certificate of its own. It reads a
page from a real site, and it checks that site against the trust store of the
operating system.

## Notes for Developers

### API

#### `(resolve-domain-name ?domainName)`

Finds the IP addresses of a domain name. I tested this with IPv4 and with
IPv6 only.

#### `(accept ?socketfdOrLogicalName)`

Accepts a connection on a socket. Gives an integer, which is the file
descriptor of the client. Gives FALSE if it fails.

The new socket also receives a logical name. The name is the address of the
client, then `#`, then the file descriptor of the new socket. Read the name
with `(get-socket-logical-name)`.

A UNIX client usually binds no path. It thus sends no address for the name.
The new socket then takes the path of the listening socket:

```
CLIPS> (accept 3)   ; a socket bound to 127.0.0.1:8889
4
CLIPS> (get-socket-logical-name 4)
127.0.0.1:42616#4
CLIPS> (accept 5)   ; a socket bound to /tmp/CLIPSockets-tmp-socket
6
CLIPS> (get-socket-logical-name 6)
/tmp/CLIPSockets-tmp-socket#6
```

The file descriptor at the end of the name is necessary. Two clients can have
the same address. A UNIX client has no address at all. Two IPv4 clients can
also use one source port, if they connect to two different servers in this
process. But the file descriptor is different for each socket in the process.
The name of each connection is thus also different.

Do not write this name yourself. Keep the name that
`(get-socket-logical-name)` gives you, and use it.

#### `(bind-socket ?socketfd ?ipOrDir <?port>)`

Binds a socket to an IP address and a port. For a UNIX socket, it binds the
socket to a path. Gives the logical name of the socket, for example
`127.0.0.1:8889`. Gives FALSE if it fails.

This name has no file descriptor in it. The names from `(accept)` and from
`(connect)` have one. A process binds one socket to one address. But it can
hold many connections to one address.

#### `(connect ?socketfd ?ipOrDir <?port>)`

Connects a socket to an IP address and a port. For a UNIX socket, it connects
the socket to a path. Gives the logical name of the connection, which you can
read and write. Gives FALSE if it fails.

The name is the peer, then `#`, then the file descriptor of this socket. The
name thus shows the destination and the connection:

```
CLIPS> (connect 4 127.0.0.1 8888)
127.0.0.1:8888#4
CLIPS> (connect 5 127.0.0.1 8888)
127.0.0.1:8888#5
CLIPS> (connect 6 ::1 8888)
[::1]:8888#6
CLIPS> (connect 7 /tmp/CLIPSockets-tmp-socket)
/tmp/CLIPSockets-tmp-socket#7
```

`printout` and `readline` find a socket by its logical name only. The file
descriptor in the name is thus necessary. With it, a program can hold more
than one connection to one server. Without it, all these connections have one
name. All the data then goes to the first socket that the router finds.

Do not write this name yourself. Keep the name that `connect` gives you, and
use it.

#### `(close-connection ?socketfdOrLogicalName)`

Closes a socket. Gives TRUE if the socket closed correctly, and FALSE if it
fails.

#### `(create-socket ?domain ?type <?protocol>)`

Makes a socket. Gives an integer, which is its file descriptor. Gives FALSE if
it fails. The socket has no logical name until you bind it, connect it, or
accept a client on it.

These domains are available:

* `AF_UNIX`: UNIX sockets
* `AF_INET`: IPv4
* `AF_INET6`: IPv6

These types are available:

* `SOCK_STREAM`: usually TCP
* `SOCK_DGRAM`: usually UDP

The protocol is optional. Usually you do not give one.

#### `(empty-connection ?socketfdOrLogicalName)`

Removes the data that came from the client from the buffer.
**WARNING: On a blocking socket, this function can wait for an unlimited
time**.

Gives TRUE, or FALSE for a socket that is not connected and not accepted.
`(dtls-recv)` refuses to operate while characters stay in the buffer, and this
function removes them.

#### `(errno)` and `(errno-sym)`

Gives `errno`, the global variable that some socket functions set when an
error occurs. For example, bind a socket two times:

```
$ ./clips
         CLIPS (6.4.2 1/14/25)
CLIPS> (errno)
0
CLIPS> (errno-sym)
CLIPS> (create-socket AF_INET SOCK_STREAM)
3
CLIPS> (bind-socket 3 127.0.0.1 8889)
127.0.0.1:8889
CLIPS> (bind-socket 3 127.0.0.1 8887)
Could not bind 127.0.0.1
perror: Invalid argument
FALSE
CLIPS> (errno)
22
CLIPS> (errno-sym)
EINVAL
```

`(errno-sym)` also accepts an optional integer. Then it gives the name of that
value, and it does not read the current `errno`. This is of use when a program
keeps an error number and translates it later. Other calls can change `errno`
before that moment:

```
CLIPS> (errno-sym 13)
EACCES
CLIPS> (errno-sym 111)
ECONNREFUSED
```

A value with no known error, and also `0`, gives nothing.

#### `(fcntl-add-status-flags ?socketfdOrLogicalName $?flags)`
#### `(fcntl-remove-status-flags ?socketfdOrLogicalName $?flags)`

Adds flags to a socket, or removes flags from a socket. Give the flags after
the file descriptor or the logical name.

These flags are available:

* `O_NONBLOCK`
* `O_APPEND`
* `O_ASYNC`

#### `(flush-connection ?socketfdOrLogicalName)`

This function sends the data in the buffer to the peer.

It gives TRUE if it sent all of the data. It gives FALSE in these two conditions:

* This library does not know the socket.
* The socket did not accept all of the data.

The send buffer of a socket has a maximum size. If the peer is slow to read, the
buffer becomes full. A write to a full buffer waits on a blocking socket. On a
non-blocking socket, the write sends some of the data and refuses the other data.
The second FALSE reports this condition, and `(errno-sym)` gives `EAGAIN`.

A FALSE does not mean that the data is lost. This library keeps the data that the
socket refused. Flush the connection again after the peer reads enough data. The
library then sends the data that is left, in the correct sequence:

```clips
(printout ?name ?reply crlf)
(while (not (flush-connection ?sock)) do
   (poll ?sock 1000 POLLOUT))
```

The library keeps the data for a write of any size. It also keeps the data for all
of the writes that the program does. It discards no data that it accepted.

The data stays in memory until a flush sends it. Therefore a program that
continues to write after a FALSE makes this memory larger.
`(get-retained-bytes ?socketfd)` gives the number of bytes in this memory.
`(set-retained-limit ?socketfd ?bytes)` puts a limit on it.

If the socket has a TLS session, the data goes through that session. The rules in
this section are also correct for that socket. This function does not change if a
program uses TLS.

#### `(get-retained-bytes ?socketfdOrLogicalName)`

This function gives the number of bytes that this library keeps for the socket. It
gives FALSE if this library does not know the socket.

A peer that does not read makes the program use this memory. A server can use
this function for two purposes:

* To find the difference between a slow peer and a peer that stopped.
* To make the decision to stop the writes to a peer that is too slow.

`set-retained-limit` puts a limit on this same number.

A value of zero does not mean that the peer received all of the data. It means
only that the buffer of this library is empty. Data can be in a different buffer:

* On a plaintext socket, the number is the data that the socket refused. Data
  that waits for a usual flush is in the buffer of the C library. This function
  does not count that data. Therefore a `printout` with no flush after it gives
  zero.
* On a socket with a TLS session, the number is the write buffer of the session.
  That buffer holds both types of data. Therefore a `printout` with no flush
  after it gives a value that is more than zero.

Use `flush-connection` to find if the peer received all of the data. That function
includes both types of data.

#### `(get-retained-limit ?socketfdOrLogicalName)`
#### `(set-retained-limit ?socketfdOrLogicalName ?bytes)`

These two functions control the maximum number of bytes that the socket keeps for
a peer that does not read. A limit of `0` means no limit. Each new socket starts
with no limit.

`(set-retained-limit)` gives TRUE. It gives FALSE in these two conditions: this
library does not know the socket, or the limit is a negative number.

`(get-retained-limit)` gives the limit. It also gives FALSE if this library does
not know the socket.

With no limit, the library discards no data. The memory becomes larger with each
write that the program does not flush. With a limit, the memory has a maximum
size, and the library discards the data that is more than the limit.

Select the correct behavior for your peers:

* A server on a public network has peers that can stop the reads at any time. If
  many peers do this at the same time, the program can use all of the memory. A
  limit changes this failure into the loss of data for one peer. That loss is
  better than a failure of the program.
* Use no limit if you trust the peers. Also use no limit if the program sends a
  large file in one `printout`. The library discards the data without a message,
  and the program cannot prevent this loss.

If you set a limit, you must examine the result of `flush-connection`.

A new limit can be less than the number of bytes in memory. That new limit does
not discard those bytes. It prevents only the storage of more data.

These two functions control a stream socket. This includes a stream socket with
a TLS session. A datagram socket with a DTLS session is different. That session
holds one record. A record is complete or the library does not send it, so the
library cannot hold a part of one. Use `dtls-send` for a datagram session. That
function gives FALSE when the socket does not accept the record.

```clips
(set-retained-limit ?sock 262144)   ; hold at most 256 KB for this client
(get-retained-limit ?sock)          ; 262144
(set-retained-limit ?sock 0)        ; back to holding everything
```

#### `(get-socket-logical-name ?socketfd)`

Changes an integer file descriptor into the symbol that is the name of the I/O
router. Use this name to read the socket and to write to it.

The name depends on how the socket received one. `(bind-socket)` gives the
local address, and no file descriptor. `(accept)` gives the address of the
client, and the file descriptor of the new socket. `(connect)` gives the
address of the peer, and the file descriptor of the local socket:

```
CLIPS> (get-socket-logical-name 3)   ; a socket bound to 127.0.0.1:8889
127.0.0.1:8889
CLIPS> (get-socket-logical-name 4)   ; a client that this socket accepted
127.0.0.1:42616#4
CLIPS> (get-socket-logical-name 5)   ; a connection to 127.0.0.1:8889
127.0.0.1:8889#5
```

Gives FALSE for a descriptor that is not a socket. Also gives FALSE for a
socket with no bind, no connect and no accept. Such a socket has no name.

#### `(get-timeout ?socketfdOrLogicalName)`
#### `(set-timeout ?socketfdOrLogicalName ?microseconds)`
#### `(set-timeout ?socketfdOrLogicalName ?seconds ?microseconds)`

Reads or sets the timeout of the socket. This is the timeout for receipt
(`SO_RCVTIMEO`). It has no effect on transmission. A new socket gives `0`,
which is no timeout.

`(set-timeout)` takes the time in one of two forms. One number is
microseconds. Two numbers are seconds and then microseconds, in the sequence
of the `timeval` structure of the system. The two forms give the same result:

```
CLIPS> (set-timeout 4 2500000)    ; 2.5 seconds
TRUE
CLIPS> (get-timeout 4)
2500000
CLIPS> (set-timeout 4 2 500000)   ; also 2.5 seconds
TRUE
CLIPS> (get-timeout 4)
2500000
```

The system keeps the seconds and the microseconds in two different fields. It
refuses a microseconds field of 1000000 or more. `(set-timeout)` thus moves
the whole seconds into the field for seconds before it calls the system. A
value of `2000000` is therefore two seconds, and not an error. A microseconds
argument of 1000000 or more moves in the same manner, thus
`(set-timeout 4 1 1500000)` is 2.5 seconds.

Give `0` to remove the timeout.

`(set-timeout)` gives TRUE. It gives FALSE for a socket that this library does
not know, for a negative number, and if the system refuses the call. Use
`(errno-sym)` for the reason.

`(get-timeout)` gives the full timeout in microseconds. This number includes
the seconds. It gives FALSE for a descriptor that this library does not know.

#### `(listen ?socketfdOrLogicalName <?backlog>)`

Listens for connections on a socket. After this call, you can `(accept)` the
clients that connect to your server.

`?backlog` is the number of connections that the kernel keeps in a queue for
you. The default is 15.

#### `(poll ?socketfdOrLogicalName ?milliseconds $?optionalFlags)`

Examines the socket for the given number of milliseconds, and looks for the
given flags.

Give `0` for `?milliseconds` for an immediate answer.

If you give no flags, the call looks for all the flags below at the same time.
The answer is then TRUE for many different conditions, and it tells you very
little. Give the flag that you need.

These flags are available:

* `POLLIN`
* `POLLOUT`
* `POLLERR`
* `POLLHUP`
* `POLLNVAL`
* `POLLPRI`

Gives TRUE if the event occurred. Gives FALSE if the timeout ended first. An
unknown flag name causes a message, and the call gives FALSE.

On a TLS socket, a poll for `POLLIN` also gives TRUE when full records are
already decrypted in this process. `poll(2)` alone cannot see those bytes.
`(tls-pending)` asks the same question directly.

#### `(getsockopt ?socketfdOrLogicalName ?level ?optionName)`
#### `(setsockopt ?socketfdOrLogicalName ?level ?optionName ?value)`

Reads or sets an option of the socket.

These values of `?level` are available:

* `SOL_SOCKET`
* `IPPROTO_TCP`

These values of `?optionName` are available:

* `SO_REUSEADDR`
* `SO_SNDBUF`
* `SO_RCVBUF`
* `TCP_NODELAY`

`?value` is the integer value for the option.

`SO_SNDBUF` and `SO_RCVBUF` are the sizes in bytes of the send buffer and the
receive buffer of the kernel. You must know three facts before you use them. The
kernel causes all three, not this library:

* Linux stores two times the value that you give. It uses the second half for
  internal data. Therefore `getsockopt` gives approximately two times the value
  that you gave to `setsockopt`.
* The kernel limits the result to `net.core.wmem_max` and `net.core.rmem_max`. If
  these limits are 208 KB, a request for 1 MB and a request for 64 MB give the
  same result. You must have root permission to make these limits larger.
* If you set `SO_SNDBUF` on a TCP socket, the kernel stops the automatic control
  of that buffer size. The automatic control can make the buffer much larger than
  `wmem_max`. Therefore a large value can give a buffer that is smaller than the
  buffer before the change.

A decrease of the buffer size has no limit, and it is the useful direction. A
smaller send buffer makes the socket become full more quickly. Let the kernel
control an increase.

These buffers and the data that `set-retained-limit` controls are different:

* The buffer of the kernel holds data that goes to the peer. The kernel removes
  this data from the buffer when the peer reads it.
* The retained data is data that the kernel refused. This data stays in memory
  until the program calls `flush-connection` again.

#### `(set-fully-buffered ?socketfdOrLogicalName)`
#### `(set-not-buffered ?socketfdOrLogicalName)`
#### `(set-line-buffered ?socketfdOrLogicalName)`

Changes the type of buffer of a connection. For example, stderr has no buffer,
stdout in your terminal usually has a line buffer, and files usually have a
block buffer.

#### `(signal ?signal ?disposition)`

Sets the reaction of this process to a signal. Gives `TRUE`. Gives `FALSE`,
with `(errno)` set, if the system refuses.

`?disposition` is `SIG_IGN` to ignore the signal, or `SIG_DFL` for the default
behavior. There is no third value. A signal handler operates asynchronously
and can call async-signal-safe functions only. A CLIPS deffunction thus cannot
be a handler.

Each server needs this for `SIGPIPE`. A write to a socket after the peer
closed it causes `SIGPIPE`, and the default reaction stops the process. So a
client that closes a connection in the middle of a reply can stop your server:

```
(signal SIGPIPE SIG_IGN)
```

After this line, the write fails with `EPIPE`, and `(errno-sym)` shows it. The
example servers start with this line.

These signals are available: `SIGHUP`, `SIGINT`, `SIGQUIT`, `SIGKILL`,
`SIGPIPE`, `SIGALRM`, `SIGTERM`, `SIGUSR1`, `SIGUSR2`, `SIGCHLD`, `SIGCONT`,
`SIGSTOP`, `SIGTSTP` and `SIGWINCH`. `SIGKILL` and `SIGSTOP` are in the list
for one reason only: a request for one of them must fail with the `EINVAL` of
the system, and not with an unknown-name message. No process can catch these
two signals or ignore them. The signals for faults (`SIGSEGV`, `SIGBUS`,
`SIGFPE`, `SIGILL`) are not available. If you ignore one of those, the
instruction that caused the fault operates again for an unlimited time.

Two more points. If you ignore `SIGINT`, `ctrl+c` stops to operate. And
`(signal SIGINT SIG_DFL)` gives you the default behavior of the *system*,
which stops the process. It does not give you the handler of CLIPS. Also,
usual output uses the same path. So, if you ignore `SIGPIPE`, a command such
as `./clips -f2 foo.bat | head -1` no longer stops at the correct moment.

#### `(shutdown-connection ?socketfdOrLogicalName ?optionalHow)`

Stops one direction of a full-duplex connection, or both directions.

These values of `?optionalHow` are available:

* `SHUT_RD`: the socket receives no more data
* `SHUT_WR`: the socket transmits no more data
* `SHUT_RDWR`: the socket receives and transmits no more data

#### `(rcvfrom ?socketfdOrLogicalName <?flags> <?maxlen>)`

Receives one datagram from a socket. The name of the function is `rcvfrom`,
and not `recvfrom`. This function is primarily for UDP sockets
(`SOCK_DGRAM`). It also operates with other datagram sockets, such as
`AF_UNIX`.

Arguments:

`?socketfdOrLogicalName`: the socket to read.

`?flags` (optional): one symbol, one integer, or a multifield of symbols.
These are the flags for the `recvfrom(2)` call. These symbols are available:

- `MSG_PEEK`
- `MSG_OOB`
- `MSG_WAITALL`

`?maxlen` (optional): the maximum number of bytes to read. The default is
65535. The value must be more than 0 and not more than 65536.

Return value:

A multifield. It gives the source of the datagram, and then the datagram. The
number of fields depends on the address family, because a UNIX socket has a
path and no port:

| Family | Fields |
| --- | --- |
| `AF_INET`, `AF_INET6` | family, address, port, number of bytes, data: 5 fields |
| `AF_UNIX` | family, path, number of bytes, data: 4 fields |
| other | `AF_UNSPEC`, number of bytes, data: 3 fields |

The data is thus always the last field, and the number of bytes is the field
before it. If you do not know the family, read the fields from the end with
`(nth$ (length$ ?mf) ?mf)`.

The data comes to CLIPS as a string, so it stops at the first NUL byte. You
cannot use this function for binary datagrams.

Example:

```clips
; A simple UDP receive
(bind ?mf (rcvfrom ?sock))
(printout t "Got " (nth$ 4 ?mf) " bytes: " (nth$ 5 ?mf) crlf)

; With flags
(bind ?mf (rcvfrom ?sock (create$ MSG_PEEK MSG_WAITALL) 512))
```

#### `(sendto ?socketfdOrLogicalName ?family ?address <?port> ?data <?flags>)`

Sends one datagram to a destination. This function is primarily for UDP
sockets (`SOCK_DGRAM`).

Arguments:

`?socketfdOrLogicalName`: the socket to send from.

`?family`: the address family of the destination:

- `AF_UNIX`: UNIX domain sockets
- `AF_INET`: IPv4
- `AF_INET6`: IPv6

The family has to be the one the socket was created with. A socket can only
send to a destination in its own family, so naming a different one gives
`FALSE` and a message saying which two disagreed. The family is still written
out rather than taken from the socket because it says how many arguments
follow: `AF_UNIX` takes a path and no port, and the other two take an address
and a port.

`?address`: the address of the destination:

- a path, for `AF_UNIX`
- an IP address, for `AF_INET` and `AF_INET6`

An address that is not the family's own form gives `FALSE`. For `AF_UNIX` that
includes a path longer than the system holds, which is 108 characters on
Linux. Such a path is refused rather than shortened, because a shortened path
names a different socket and the send would report success.

`?port`: the port of the destination. `AF_INET` and `AF_INET6` need it.
`AF_UNIX` takes no port at all: give it the path and then the data.

`?data`: the string to send. An empty string ("") is permitted, for a signal
or for a keepalive packet.

`?flags` (optional): one symbol, one integer, or a multifield of symbols.
These are the flags for the `sendto(2)` call. These symbols are available:

- `MSG_CONFIRM`
- `MSG_DONTROUTE`
- `MSG_DONTWAIT`
- `MSG_EOR`
- `MSG_MORE`
- `MSG_NOSIGNAL`
- `MSG_OOB`

Return value:

The number of bytes that the socket sent, as an integer. Gives `FALSE` if an
error occurred. Use `(errno)` and `(errno-sym)` for the details.

Examples:

```clips
; Send a message to a UNIX domain socket
(sendto ?sock AF_UNIX "/tmp/socket" "hello")

; Send a UDP datagram to an IPv4 address
(sendto ?sock AF_INET "127.0.0.1" 9999 "ping")

; Send with more than one flag
(sendto ?sock AF_INET "192.168.1.100" 12345 "payload" (create$ MSG_NOSIGNAL MSG_DONTWAIT))
```

### Functions That Are Not About Sockets

Three functions here are not network functions. They are here because a server
in CLIPS needs them, and CLIPS does not have them.

#### `(scandir ?directory)`

Gives the entries of a directory, in alphabetical sequence, as a multifield of
symbols. The list contains `.` and `..`, because `scandir(3)` gives them.
Gives FALSE if the function cannot read the directory. `(errno)` then gives
the reason.

`server-http-file.clp` makes its directory list with this function.

#### `(sleep ?seconds)`

Stops the process for the given time. The argument is an integer or a float,
thus `(sleep 1)` and `(sleep 0.25)` are both correct. Gives `0`. Gives FALSE
for a negative number.

No other part of the environment operates during this time. No rule fires, and
the program reads no socket. Use this function in a client that has no other
work. Do not use it to control the speed of a server. A server uses `(poll)`,
which uses the same clock but stops early when there is work.

#### `(mimetype ?path)`

Gives the MIME type of a file, as a symbol: `text/html`, `image/png`. libmagic
reads the contents of the file, and not its name.

This function is in a `make magic` build only. In all other builds, the name
of this function is a parse error, and not a failure at run time. The TLS
functions below operate in the same manner in a `make no-tls` build.

If libmagic cannot read a file, it gives its own default type. It does not
give FALSE. So compare the result against the type that you expect. Do not
compare it against FALSE.

### TLS

A TLS connection is a usual socket with a session on top of it. First connect
the socket, or accept a client. Then give the socket to `(tls-connect)` or to
`(tls-accept)`. After that, `printout`, `readline` and `get-char` use the same
logical name as before. No other part of a program changes.

There are two objects. A *context* holds the data that is the same for each
connection: the certificate, the key, and the authorities to trust. A
*session* is the part for one connection. The session stays on the socket, and
not in a variable.

```clips
(deffunction fetch-page ()
   (bind ?ctx (tls-create-context TLS_CLIENT))
   (tls-context-load-verify-locations ?ctx "tests/fixtures/ca.pem")

   (bind ?sock (create-socket AF_INET SOCK_STREAM))
   (bind ?name (connect ?sock 127.0.0.1 8888))
   (tls-connect ?ctx ?sock localhost)

   (printout ?name "GET / HTTP/1.1" cr lf cr lf)
   (flush-connection ?sock)
   (println (readline ?name))

   (tls-shutdown ?sock)
   (close-connection ?sock))

(fetch-page)
```

The body is a `deffunction`, for the same reason as the tests. A top-level
`(bind ?x ...)` does not stay to the next batch command. If you write these
lines as separate commands, `?ctx` and `?sock` are immediately empty. The
examples below are parts of a function body. They are not commands for the
prompt.

These functions are in a TLS build only. `make no-tls` does not register them.
In that build, the name of one of these functions is a parse error, and not a
failure at run time.

#### `(tls-create-context ?role)`

Makes a context. `?role` is `TLS_CLIENT` or `TLS_SERVER`. Gives an integer
handle, or FALSE.

A client context verifies the peer by default. So a program that forgets to
ask for verification cannot make a connection without authentication. A server
context does not ask the client for a certificate, until you tell it to.

#### `(tls-free-context ?context)`

Releases a context. The sessions that are already on sockets continue. Gives
TRUE. Gives FALSE for a handle that this library never made.

#### `(tls-context-load-verify-locations ?context ?file <?directory>)`

Trusts the certificate authorities in a PEM file, in a directory of PEM files,
or in both. Gives FALSE if it can read neither of them. This call replaces the
authorities that the context had before. That is the purpose of a call that
names your own authority.

#### `(tls-context-set-default-verify-paths ?context)`

Trusts the authorities that the operating system trusts. This is the start
point of a browser. Gives FALSE if it finds no authorities.

The location of those authorities is a property of the build of the TLS
library, and not of the machine. A library in a prefix of its own can look in
that prefix and find nothing. Then it trusts no certificate. If this occurs,
load a bundle with `(tls-context-load-verify-locations)`.

#### `(tls-context-use-certificate-file ?context ?path)`
#### `(tls-context-use-private-key-file ?context ?path)`

Gives a server its certificate and the related key. Both files are PEM files.
The sequence of the two calls is not important. Each call reads its file and
examines it immediately. A path that does not exist, or a certificate in the
place of a key, so causes an error in the call that is incorrect. A later
call does not report it.

#### `(tls-context-set-verify ?context ?mode)`

`SSL_VERIFY_PEER` needs a certificate that is correct. `SSL_VERIFY_NONE`
accepts all certificates.

On a server, `SSL_VERIFY_PEER` asks the client for a certificate. The server
refuses the handshake if the client sends no certificate, or a certificate
that the trust store does not accept. This is how you make client
authentication operate. The refusal is important: a setting that asks for a
certificate and then accepts a client without one gives a false impression of
a check.

If you stop the verification, the connection is private but not authenticated.
The data is still encrypted, but you do not know who is at the other end.

Gives FALSE if the library cannot do what the call asks. It does not report
success and then leave the connection without authentication.

#### `(tls-context-set-min-proto-version ?context ?version)`

`TLS1_2_VERSION` or `TLS1_3_VERSION`. Gives FALSE if the library has no
support for that version. It does not accept the request and then use a
different version. You can thus use this call to ask what is available:

```clips
(bind ?ctx (tls-create-context TLS_CLIENT))
(bind ?have13 (tls-context-set-min-proto-version ?ctx TLS1_3_VERSION))
```

#### `(tls-connect ?context ?socketfd ?hostname)`

Starts a client handshake on a connected socket.

`?hostname` has two functions. The socket sends it to the server, and the
server thus knows which certificate to offer. It is also the name that the
library compares against the certificate that comes back. The name must
therefore be a name in that certificate. An address that resolves correctly is
not sufficient. There is no way to send the name and not compare it.

#### `(tls-accept ?context ?socketfd)`

Starts a server handshake on an accepted socket.

These two calls refuse a socket that a program already read or wrote. The
buffered data would be on the two sides of the handshake. Do the handshake
first.

On a blocking socket, they do the full handshake and give TRUE. On a
non-blocking socket, they start the handshake and give FALSE, and
`(errno-sym)` gives `EAGAIN`. `(tls-handshake)` then continues the handshake.

#### `(tls-handshake ?socketfdOrLogicalName)`

Continues a handshake. Gives TRUE when the handshake is complete, and TRUE
again if it was already complete. FALSE has two meanings: "not yet" and "this
connection is finished". `(errno-sym)` shows the difference. `EAGAIN` is "not
yet".

The important part is the wait between the attempts, and `(poll)` gives two
different answers. A timeout of `0` reports the condition of the socket and
gives control back immediately. A different number waits in `poll(2)` for that
time, and no other part of the environment operates. This is also true for
plaintext sockets. TLS changes none of it.

A client with no other work can wait:

```clips
(while (not (tls-handshake ?sock)) do
   (if (neq (errno-sym) EAGAIN) then (return FALSE))
   (poll ?sock 50 POLLIN))
```

A server cannot wait, because that loop stops all its other work for 50
milliseconds each time. On a server, put the retry in a rule. The engine then
receives control after each attempt, and it matches all its other rules
between the attempts:

```clips
(defrule carry-on-with-handshake
   ?h <- (handshaking ?sock ?deadline ?tries)
   (test (<= (time) ?deadline))
   =>
   (retract ?h)
   (if (tls-handshake ?sock)
      then (assert (session-ready ?sock))
      else (assert (handshaking ?sock ?deadline (+ ?tries 1)))))
```

What must the server do between the attempts? It must do the work that it does
between all its other tasks. A server has two useful tasks during a wait. It
can accept a connection, if it has fewer clients than its limit. Or it can
sleep until one of its clients needs attention. A spin loop is neither of
these.

`examples/server-complex-tls.clp` is the full form of this idea, and it does
no wait of its own. It sets the `delayed-until` value of the client to a
future time. The rule above thus does not match until the clock of the server
gets to that time.

`server-complex.clp` moves that clock: it polls the *listening* socket for
exactly that time. A new connection thus wakes the server early, and it
accepts the client. If nothing arrives, the server slept for a known time. The
handshake uses the clock of the server, and does not add a clock of its own.

Note what the rule gives up: the clock, and not a number of attempts. The
count is there for one purpose only. Each fact must be different from the last
one, because CLIPS does not assert a fact that it already holds. Without the
count, the rule stops to fire.

Poll for `POLLIN`, and not for the two directions. A connected socket can
almost always write. A poll for `POLLOUT` thus gives TRUE each time, and tells
you nothing about the reply of the peer.

#### `(tls-shutdown ?socketfdOrLogicalName)`

Sends the TLS close message. It does not wait for the message of the peer,
which can be absent. Two calls cause no damage. Use this function, and not
`(shutdown-connection)`. `(shutdown-connection)` closes the socket below the
session, and the peer thus sees a broken connection in the place of a correct
end.

#### `(tls-pending ?socketfdOrLogicalName)`

Gives the number of decrypted bytes in this process. Gives FALSE for a socket
with no session.

You seldom need this function, because `(poll)` includes these bytes. The
library decrypts a full record at one time. Bytes can thus wait in this
process while the descriptor has nothing for `poll(2)`. A poll of the kernel
alone would wait for data that is already here.

#### `(tls-cipher ?socketfdOrLogicalName)`
#### `(tls-version ?socketfdOrLogicalName)`

The cipher suite and the protocol of the connection, after the handshake.

`(tls-version)` gives `TLSv1.2` or `TLSv1.3` for each library. This includes
GnuTLS, which writes these names differently itself. `(tls-cipher)` gives the
name of the library without a change, and the libraries are different. The
`AES-256-GCM` of GnuTLS is the connection that OpenSSL calls
`TLS_AES_256_GCM_SHA384`.

#### `(tls-verify-result ?socketfdOrLogicalName)`

Gives TRUE if the library accepted the certificate of the peer. If not, it
gives a string with the reason. Gives FALSE for a socket with no session.

A failed check fails the handshake. Use this function to find the reason after
the event. Do not use it to decide if you trust the connection. The library
made that decision before you can ask.

#### `(tls-peer-subject ?socketfdOrLogicalName)`

Gives the subject name in the certificate of the peer. Gives FALSE if the peer
sent no certificate. A server receives a certificate only if it asked for one
with `(tls-context-set-verify)`.

This is of use for a check of your own after the handshake. Such a check can
make the result of the library more strict, but it cannot make it less strict:

```clips
(if (tls-connect ?ctx ?sock example.com) then
   (if (neq (tls-peer-subject ?sock) "/CN=example.com") then
      (close-connection ?sock)))
```

#### `(tls-backend)`
#### `(tls-backend-version)`

The TLS library of this binary, as a symbol, and its version, as a string:

```
CLIPS> (tls-backend)
openssl
CLIPS> (tls-backend-version)
"OpenSSL 3.0.13 30 Jan 2024"
```

### DTLS

DTLS is TLS on datagrams. A UDP socket holds a session in the same manner as a
stream socket. The logical name continues to operate. The same contexts and
the same functions for information serve the two protocols.
`(tls-create-context)` takes `DTLS_CLIENT` or `DTLS_SERVER` in the place of
`TLS_CLIENT` or `TLS_SERVER`. On the side of the initial setup, that is the
full difference.

```clips
(deffunction dtls-echo-once ()
   (bind ?ctx (tls-create-context DTLS_CLIENT))
   (tls-context-load-verify-locations ?ctx "tests/fixtures/ca.pem")

   ;; Connect first: a datagram socket with no peer has no destination for a
   ;; ClientHello.
   (bind ?sock (create-socket AF_INET SOCK_DGRAM))
   (connect ?sock 127.0.0.1 9443)
   (dtls-connect ?ctx ?sock localhost)

   (dtls-send ?sock "hello over dtls")
   (if (poll ?sock 5000 POLLIN) then
      (println (nth$ 2 (dtls-recv ?sock))))

   (tls-shutdown ?sock)
   (close-connection ?sock))

(dtls-echo-once)
```

`examples/server-dtls.bat` and `examples/client-dtls.bat` are the same
procedure, as two programs.

Some TLS libraries have no DTLS. Ask before you write the code:

```
CLIPS> (tls-supports-dtls)
TRUE
CLIPS> (tls-supports-dtls DTLS_SERVER)
TRUE
```

s2n-tls and BoringSSL give FALSE, because they have no DTLS. wolfSSL depends
on its build options: a default build has no DTLS, and the binary knows what
its build gave it. All the other libraries here do the two halves. Ask the
binary, and do not read this list. The list is the part that becomes
incorrect.

#### The Differences, and Why They Are Important

A DTLS socket answers to `printout`, `readline` and `get-char` through its
logical name, in the same manner as a plaintext UDP socket. That is
convenient, but it is not the same as a read of a stream:

**A record has no end.** A read after the last character of a datagram waits
on a blocking socket. On a non-blocking socket, it gives EOF with `EAGAIN`.
Nothing says "that was the full message".

**`readline` joins records.** If a record has no newline in it, the next
record continues it, and you cannot see the join. Two messages can come back
as one line.

**A lost datagram makes a gap, and a datagram in the wrong sequence makes a
mixture.** DTLS does not send application data again. Bytes that go away never
arrive. The library gives the records in the sequence of their arrival. The
anti-replay window removes duplicates, but it does not correct the sequence. A
reader sees none of this.

So, use `(dtls-send)` and `(dtls-recv)` when the limits of a message are
important. For most protocols on UDP, they are. Use `printout` and `readline`
for traffic in lines, when a lost line is acceptable. Set a line buffer also,
which is what a datagram session starts with. One line then goes out as one
record.

#### `(dtls-connect ?context ?socketfd ?hostname)`

Does the client half of a handshake on a **connected** datagram socket. The
library compares `?hostname` against the certificate of the peer, and the
argument is necessary, as in `(tls-connect)`. Gives TRUE. On a non-blocking
socket it gives FALSE with `EAGAIN`, and `(tls-handshake)` then continues.

#### `(dtls-accept ?context ?socketfd)`

Does the server half, on a **bound** datagram socket. There is no `(accept)`
for UDP, so this call does that task also. It answers the first ClientHello
with a cookie. It does no other work until a ClientHello comes back with that
cookie, from the same address. A false source address thus costs the server
almost nothing. Then the call does the handshake.

After success, the socket is connected to that peer. The kernel thus removes
the datagrams of all other senders. One socket serves one client.

The call gives FALSE until all of this is complete. Each new call continues
from the last condition: the cookie exchange, or the handshake after it. A
full loop for a non-blocking server is:

```clips
(while (not (dtls-accept ?ctx ?sock)) do (poll ?sock 20 POLLIN))
```

Do not use `(tls-handshake)` here. Until the exchange is complete, there is no
session for it to find. Nothing tells you which of the two conditions the
socket is in.

#### `(dtls-send ?socketfdOrLogicalName ?data)`

Sends one record. Gives the number of bytes, or FALSE. If the message is
larger than one record at the current MTU, the call fails. It does not divide
the message.

#### `(dtls-recv ?socketfdOrLogicalName <?maxlen>)`

Receives one record, as a multifield of two fields: the number of bytes and
the data. This is the shape of `(rcvfrom)` without the address fields, because
there is one peer only. Gives FALSE if no record is available.

`?maxlen` is the limit for one call. The session keeps the remainder of the
record and gives it to the next call. It does not discard the remainder. A
plaintext datagram read does the opposite: data that is larger than the buffer
is lost.

The call refuses to operate while characters from an earlier record are still
in the router. It does not discard them without a message.
`(empty-connection)` removes them.

The data comes to CLIPS as a string, as in `(rcvfrom)`, so it stops at the
first NUL byte. You cannot use this function for binary records.

#### `(dtls-timeout ?socketfdOrLogicalName)`

Gives the number of milliseconds until the library must send the last flight
again. Gives FALSE when no flight is in the queue. Use this value for
information only. Its meaning is not the same in each library. Use the next
function to control the retransmission. Do not calculate a time limit from
this value.

#### `(dtls-handle-timeout ?socketfdOrLogicalName)`

Sends the last handshake flight again, if one is necessary. If none is
necessary, the call does nothing. You can thus call it on each turn of a poll
loop. Some function must call it. A handshake with no retransmission operates
correctly on the loopback interface, where nothing is lost. On the first real
network that loses a packet, that handshake stops.

#### `(dtls-set-mtu ?socketfdOrLogicalName ?bytes)`

Sets the MTU of the link. The library calculates the quantity of data for one
record from this value. A session starts at 1500. Gives FALSE on a backend
that does not accept an MTU. wolfSSL does not accept one, unless its build had
`WOLFSSL_DTLS_MTU`.

#### `(tls-supports-dtls <?role>)`

Tells you if the library has DTLS. The optional argument is one direction:
`DTLS_CLIENT` or `DTLS_SERVER`. Today no backend here gives different answers
for the two directions. But they can be different: a library without a
stateless cookie exchange can be a client and not a server. A program that
needs a server thus asks for a server, and continues to operate correctly when
this changes.

### Debugging

To see all the traffic on port 8888 of your computer, use `tcpdump`. The
example server and the example client use that port. You possibly must be
`root`, or use `sudo`:

```
tcpdump -nn -i any port 8888
```

### Notes on the Source Files

`make` fetches CLIPS into `vendor/clips-source/<tag>`,
copies that tree to `vendor/clips-build/<tag>`,
and copies the files in the root of this project
over the copy before building. Nothing under `vendor/`
is edited by hand, and `make distclean` removes all of it.

These are the files this library adds:

| File | Contents |
| --- | --- |
| `socketrtr.c`, `socketrtr.h` | The I/O router and each socket UDF. |
| `userfunctions.c` | The `AddUDF` table, and also `(scandir)`, `(sleep)`, `(signal)` and `(mimetype)`. |
| `socktls.c`, `socktls.h` | The TLS and DTLS UDFs. Also the parts of a session that do not depend on the library below. |
| `socktlsbe.h` | The interface of a backend. This is the full set of operations that `socktls.c` can ask for. |
| `socktls-openssl.c` | OpenSSL, and the three libraries that use its API: LibreSSL, BoringSSL, and wolfSSL through its compatibility layer. |
| `socktls-mbedtls.c`, `socktls-gnutls.c`, `socktls-s2n.c` | One file for each of those libraries. |
| `tlscheck.c` | A check at build time. It is not part of the binary. It asks the library at the link step if it is the same implementation as the headers at the compile step. The compiler and the linker use different search sequences, so the two can be different. |
| `clips.mk` | The makefile the build uses. `make` copies it into the build tree over the one CLIPS ships. |
| `makefile` | The targets you run. It fetches CLIPS, stages the files above, and calls `clips.mk`. |
| `scripts/fetch-clips.sh` | Fetches one CLIPS source tree, from the release tarball or from Subversion. |
| `scripts/test-clips-versions.sh` | Builds and runs the suite against each supported CLIPS version. `make test-clips`. |
