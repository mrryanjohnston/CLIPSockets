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

## High-Level Functionality Provided

Create a server that listens, accepts, and reads a message from a client:

```clips
         CLIPS (Forge Alpha 5/12/24)
CLIPS> (create-socket AF_INET SOCK_STREAM)
3
CLIPS> (bind-socket 3 127.0.0.1 8889)
127.0.0.1:8889
CLIPS> (listen 3) ; NOTE: 127.0.0.1:8889 would work here, too
TRUE
CLIPS> (accept 3) ; NOTE: 127.0.0.1:8889 would work here, too
4
CLIPS> (get-socket-logical-name 4)
127.0.0.1:42616
CLIPS> (readline 127.0.0.1:42616)

```

A client that would connect to that server would look like this:

```clips
         CLIPS (Forge Alpha 5/12/24)
CLIPS> (create-socket AF_INET SOCK_STREAM)
3
CLIPS> (connect 3 127.0.0.1 8889)
127.0.0.1:8889
CLIPS> (printout 127.0.0.1:8889 "Hello, server :)" crlf)
CLIPS> (flush-connection 3) ; NOTE: 127.0.0.1:8889 would work here, too
TRUE
```

You should see the message from the client in the previously mentioned server's rules engine
when the client flushes their connection!

## Immediate Purpose

Provide low-level networking functions to
[CLIPS](https://www.clipsrules.net/)
environments
via socket file descriptors. Allows CLIPS-based applications
to send and receive messages to/from other computers on a network
via I/O Router[^1]-based reading and writing operations.

[^1]: See **Section 9: I/O Routers** of the [CLIPS Advanced Programming Guide](https://www.clipsrules.net/documentation/v641/apg641.pdf)

This library adds functions that let you create network servers
and clients. That is: you can make CLIPS applications that
*receive* network requests and *make* network requests.

This library also supports DNS resolution. To discover IP addresses
for a given url, use `(resolve-domain-name ryjo.codes)` function.
*Hint*: Use `(intersection$ (resolve-domain-name ryjo.codes))`
if you see duplicate IP addresses returned.

## Long-Term Goals

Educational purposes for now. I would like to get the quality of code in this repo
ready for upstream merging into CLIPS someday. For now, it's so I can make
some fun web applications using CLIPS!

## Installation

The project can be built from the root directory with `make`:

```
make
```

This needs no libraries beyond the C standard library.

The `(mimetype)` function is optional and depends on libmagic (the library
behind the `file` command, packaged as `libmagic-dev` on Debian/Ubuntu and
`file-devel` on Fedora). If you want it, install those headers and run:

```
make magic
```

This will create the binary `clips` file in the root directory.
Use this to run the example server and client network applications
provided by the files in the `examples` directory.

## Tests

```
make test
```

Each file under `tests/` is run in its own `clips` process and reports its own
pass/fail counts. Tests needing a peer create both ends inside one process over
loopback, so nothing depends on timing between two invocations. Tests that
require libmagic are skipped automatically when the binary was built without it.

To write one, wrap the body in a `deffunction` -- top-level `(bind ?x ...)` does
not persist from one batch command to the next -- and declare how many checks
the file runs:

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

The plan matters: a CLIPS evaluation error halts the enclosing deffunction
without failing any assertion, so without a declared count the checks that never
ran would look like a pass.

### Coverage

```
make coverage
```

Builds instrumented, runs the suite, and reports line coverage for everything
in `socketrtr.c` and `userfunctions.c`, in two groups.

The UDFs are listed by their CLIPS name, read from the `AddUDF` table in
`userfunctions.c`, so a newly registered function appears in the report
automatically -- at 0% until someone tests it. The supporting functions follow:
the router callbacks, the syscall wrappers and the lookup helpers that are not
callable from CLIPS but are just as much a part of this library. Each group gets
a subtotal, and the combined total is what `MIN_COVERAGE` gates on (currently
95%).

### Example Servers

There are 7 example servers in this repository and 6 clients.

The simplest server
receives a single tcp connection and then exits.
The other can be used to serve multiple concurrent requests until it is
`ctrl+z` and `kill`ed.

```
./clips -f2 examples/server-simple.bat
```

or

```
./clips -f2 examples/server-complex.bat
```

`server-simple-ipv6.bat` and `server-simple-unix.bat` are the same server over
IPv6 and over a UNIX domain socket. `server-udp.bat` and `server-udp-sendto.bat`
echo a single datagram, the first replying through `connect` and `printout`, the
second replying with `sendto` directly.

`server-http-file.bat` serves the working directory over HTTP. It reports each
file's content type with `(mimetype)`, so it needs the optional libmagic build:

```
make magic
./clips -f2 examples/server-http-file.bat
```

Under a default build that rule fails to load and only the directory listing
works.

### Example Client

```
./clips -f2 examples/client.bat
```

`client-udp.bat` and `client-udp-sendto.bat` pair with the UDP servers, and
`ab-clone.bat` is a small benchmarking client that makes repeated requests
against a server on port 8888.

### Other ways to test things out

You should now also be able to connect to 127.0.0.1:8888
via telnet, curl, or a browser.

#### `curl`

```
curl --http0.9 http://localhost:8888/
```

#### `telnet`

```
telnet localhost 8888
```

#### Browser

Visit [http://localhost:8888](http://localhost:8888)
in the browser of your choice. The message displayed back
is the first line of text that your browser sends to the webserver
under the hood. This can be used to build web applications
that let the users navigate to different "pages" in your web app.
For example, if you try to go to
[http://localhost:8888/asdf-123](http://localhost:8888/asdf-123),
you'll see a slightly different message.

## Notes for Developers

### API

#### `(resolve-domain-name ?domainName)`

Looks up IP addresses given a domain name.
Only tested with IPv4 and IPv6.

#### `(accept ?socketfdOrLogicalName)`

Accepts a connection on a socket file descriptor.
Returns an integer representing the client's file descriptor
or FALSE if it fails.

#### `(bind-socket ?socketfd ?ipOrDir <?port>)`

Binds a socket to a given IP/PORT or directory (in case of unix sockets).
Returns an integer representing the client's file descriptor
or FALSE if it fails.

#### `(connect ?socketfd ?ipOrDir <?port>)`

Connects a socket to a given IP/PORT or directory (in case of unix sockets).
Returns the logical name of the connection that can be read/written,
or FALSE if it fails.

The name is the peer followed by `#` and this socket's file descriptor, so it
says both what the connection reached and which connection it is:

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

`printout` and `readline` address a socket only by its logical name, so the
descriptor is what lets a program hold several connections to the same server
at once. Without it every one of them would answer to the same name and all
traffic would land on whichever the router found first.

Do not build the name by hand: bind whatever `connect` returns and use that.

#### `(close-connection ?socketfdOrLogicalName)`

Closes a socket bound or connected on a given IP/PORT or directory (in case of unix sockets).
Returns TRUE if connection closed successfully, FALSE if it fails.

#### `(create-socket ?domain ?type <?protocol>)`

Binds a socket to a given IP/PORT or directory (in case of unix sockets).
Returns an integer representing the client's file descriptor
or FALSE if it fails.

The following domains are currently supported:

* `AF_UNIX`: used for unix sockets
* `AF_INET`: IPv4
* `AF_INET6`: IPv6

The following types are currently supported:

* `SOCK_STREAM`: typically used for TCP
* `SOCK_DGRAM`: typically used for UDP

Protocol is optional and can typically be left blank.

#### `(empty-connection ?socketfdOrLogicalName)`

Use this to "empty" the buffer of data received from the client.
**WARNING: If this is run on a blocking request, you may block indefinitely**.

#### `(errno)` and `(errno-sym)`

Returns `errno`, a global variable set when errors occur with some socket functions.
For example, if you try to bind twice on a socket:

```
$ ./clips                                                                                                  
         CLIPS (Forge Alpha 5/12/24)                                                                                                   
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

`(errno-sym)` also accepts an optional integer, in which case it names that
value instead of reading the current `errno`. This is useful when an error
number was stored earlier and is only translated later, after further calls
have overwritten `errno`:

```
CLIPS> (errno-sym 13)
EACCES
CLIPS> (errno-sym 111)
ECONNREFUSED
```

Values that do not correspond to a known error, `0` included, return nothing.

#### `(fcntl-add-status-flags ?socketfdOrLogicalName $?flags)`
#### `(fcntl-remove-status-flags ?socketfdOrLogicalName $?flags)`

Add/remove certain flags on a socket as specified after the socket fd or logical name.

The following flags are currently supported:

* `O_NONBLOCK`
* `O_APPEND`
* `O_ASYNC`

#### `(flush-connection ?socketfdOrLogicalName)`

Flushes the buffer to the recipient.

#### `(get-socket-logical-name ?socketfd)`

Converts an integer representing a socket file descriptor
to a symbol representing the name of the I/O router.
Use this name to read and write to the socket.

#### `(get-timeout ?socketfdOrLogicalName)`
#### `(set-timeout ?socketfdOrLogicalName ?microseconds)`

Gets or Sets the timeout on the socket in microseconds.

#### `(listen ?socketfdOrLogicalName ?backlog)`

Listens for connections on a socket. After running this,
you can now `(accept` clients connecting to your server
via the socket.

#### `(poll ?socketfdOrLogicalName ?milliseconds $?optionalFlags)`

Polls the socket for a number of milliseconds for any given number of flags.

Specify `0` as `?milliseconds` so that it returns immediately.

Possible flags:

* `POLLIN`
* `POLLOUT`
* `POLLERR`
* `POLLHUP`
* `POLLNVAL`
* `POLLPRI`
* `POLLRDNORM`
* `POLLRDBAND`
* `POLLWRNORM`
* `POLLWRBAND`

Returns TRUE if it got the FLAG, FALSE if it did not before timeout expires.

#### `(getsockopt ?socketfdOrLogicalName ?level ?optionName)`
#### `(setsockopt ?socketfdOrLogicalName ?level ?optionName ?value)`

Gets or Sets options on the socket.

Possible values for `?level` currently supported:

* `SOL_SOCKET`
* `IPPROTO_TCP`

Possible values for `?optionName` currently supported:

* `SO_REUSEADDR`
* `TCP_NODELAY`

`?value` is an integer to set the flag to.

#### `(set-fully-buffered ?socketfdOrLogicalName)`
#### `(set-not-buffered ?socketfdOrLogicalName)`
#### `(set-line-buffered ?socketfdOrLogicalName)`

Changes the buffering style for a connection.
As a means of example, stderr is "not buffered,"
stdout via your terminal is probably "line buffered," 
and files are normally "block buffered."

#### `(signal ?signal ?disposition)`

Sets what this process does with a signal. Returns `TRUE`, or `FALSE` with
`(errno)` set if the system refuses.

`?disposition` is either `SIG_IGN` to ignore the signal or `SIG_DFL` to restore
the default behaviour. There is no third option: a signal handler runs
asynchronously and may only call async-signal-safe functions, so a CLIPS
deffunction cannot be one.

The signal every server wants is `SIGPIPE`. Writing to a socket whose peer has
closed raises it, and its default action terminates the process, so a client
that hangs up mid-response would otherwise be able to kill your server:

```
(signal SIGPIPE SIG_IGN)
```

With that in place the write fails with `EPIPE` instead, readable through
`(errno-sym)`. The example servers begin with this line.

Recognised signals are `SIGHUP`, `SIGINT`, `SIGQUIT`, `SIGKILL`, `SIGPIPE`,
`SIGALRM`, `SIGTERM`, `SIGUSR1`, `SIGUSR2`, `SIGCHLD`, `SIGCONT`, `SIGSTOP`,
`SIGTSTP` and `SIGWINCH`. `SIGKILL` and `SIGSTOP` are listed so that asking for
them fails with the system's own `EINVAL` rather than being reported as unknown
names -- neither can be caught or ignored. The fault signals (`SIGSEGV`,
`SIGBUS`, `SIGFPE`, `SIGILL`) are not offered, because ignoring one of those
simply re-runs the faulting instruction forever.

Two things to keep in mind. Ignoring `SIGINT` disables `ctrl+c`, and
`(signal SIGINT SIG_DFL)` restores the *system* default of terminating rather
than the handler CLIPS installs at startup. And because ordinary output travels
the same path, ignoring `SIGPIPE` also means a command such as
`./clips -f2 foo.bat | head -1` no longer stops early.

#### `(shutdown-connection ?socketfdOrLogicalName ?optionalHow)`

Shut down part or all of a full-duplex connection.

Possible values of `?optionalHow`:

* `SHUT_RD`: further receptions will be disallowed
* `SHUT_WR`: further transmissions will be disallowed
* `SHUT_RDWR`: further receptions and transmissions will be disallowed

#### `(recvfrom ?socketfdOrLogicalName <?flags> <?maxlen>)`

Receives a single datagram from a socket.
This is primarily for UDP sockets (`SOCK_DGRAM`) but will also work with other datagram-style sockets like `AF_UNIX`.

Arguments:

`?socketfdOrLogicalName`: The socket to read from.

`?flags` (optional): Either a single symbol, integer, or a multifield of symbols
specifying flags for the underlying recvfrom call. Supported symbols:

- `MSG_PEEK`
- `MSG_OOB`
- `MSG_WAITALL`

`?maxlen` (optional): Maximum number of bytes to read (defaults to 65535).
Must be greater than 0 and no more than 65536.

Return Value:

A multifield with up to 6 elements, depending on the socket family:

- Address Family (ex `AF_INET`)
- Address / Path (ex `127.0.0.1` or `/tmp/foo`)
- Port (or 0 for UNIX) (ex `9999`)
- Bytes received (ex `12`)
- Data received (ex "Hello world")

Example:

```clips
; Simple UDP receive
(bind ?mf (recvfrom ?sock))
(printout t "Got " (nth$ 4 ?mf) " bytes: " (nth$ 5 ?mf) crlf)

; With flags
(bind ?mf (recvfrom ?sock (create$ MSG_PEEK MSG_WAITALL) 512))
```

#### `(sendto ?socketfdOrLogicalName ?family ?address <?port> ?data <?flags>)`

Sends a single datagram to a destination.
This is primarily for UDP sockets (`SOCK_DGRAM`).

Arguments:

`?socketfdOrLogicalName`:  The socket to send from.

`?family`: The address family of the destination:

- `AF_UNIX`: UNIX domain sockets
- `AF_INET`: IPv4
- `AF_INET6`: IPv6

`?address`: Destination address:

    - Path string for AF_UNIX
    - IP address string for AF_INET/AF_INET6

`?port`: Destination port (required for `AF_INET`/`AF_INET6`, ignored for `AF_UNIX`).

`?data`: The string data to send. Can be empty ("") for signaling or keepalive packets.

`?flags` (optional): Either a single symbol, integer, or a multifield of symbols
specifying flags for the underlying sendto call. Supported symbols:

- `MSG_CONFIRM`
- `MSG_DONTROUTE`
- `MSG_DONTWAIT`
- `MSG_EOR`
- `MSG_MORE`
- `MSG_NOSIGNAL`
- `MSG_OOB`

Return Value:

Returns the number of bytes successfully sent as an integer,
or `FALSE` if an error occurred.
Use (`errno`) and (`errno-sym`) to get details.

Examples:

```clips
; Send a message to a UNIX domain socket
(sendto ?sock AF_UNIX "/tmp/socket" "hello")

; Send a UDP datagram to IPv4 address
(sendto ?sock AF_INET "127.0.0.1" 9999 "ping")

; Send with multiple flags
(sendto ?sock AF_INET "192.168.1.100" 12345 "payload" (create$ MSG_NOSIGNAL MSG_DONTWAIT))
```

### Debugging

In order to watch all activity on your computer's port 8888
(which the example server and client use by default),
use `tcpdump`. You may need to run as `root` or with `sudo`:

```
tcpdump -nn -i any port 8888
```

### Small technical braindump

This codebase is based on
[CLIPS 7.0.x](https://sourceforge.net/p/clipsrules/code/HEAD/tree/branches/70x/core/)
released on 5/12/24. I added a `socketrtr.h` and `socketrtr.c` to support reading/writing to sockets.
I add user defined functions (UDFs) to CLIPS environments compiled with this source code
in `userfunctions.c`. I initialize the socket router in `router.c`
inside of the function `InitializeDefaultRouters`.
