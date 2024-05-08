# CLIPSockets

## Forward

* This repository is for educational purposes only :)
* I have only tested this codebase in Ubuntu 23.10 so far
* the library is only built for linux-based systems for now
* Don't know CLIPS? Try the
  [Tour of CLIPS](https://ryjo.codes/tour-of-clips.html)
  I wrote to learn!
  

## Purpose

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

It does *not* currently support DNS resolution: you'll need to know
the IP/PORT combination or location of unix socket
instead of the URL you use to get to a site in your browser for now.

## Goals

Educational purposes for now. I would like to get the quality of code in this repo
ready for upstream merging into CLIPS someday. For now, it's so I can make
some fun web applications using CLIPS!

## Installation

Navigate into the `src` directory and run `make`:

```
make
```

You should now have a `clips` executable file in your `src` directory.
Use this to run the example server and client network applications
provided by the files `src/server-simple.bat`, `src/server-complex.bat`,
and `src/client.bat`.

### Example Servers

There are two example servers provided in this repository. One simply
receives a single tcp connection and then exits.
The other can be used to serve multiple concurrent requests until it is
`ctrl+z` and `kill`ed.

```
./clips -f2 server-simple.bat
```

or

```
./clips -f2 server-complex.bat
```

### Example Client

```
./clips -f2 client.bat
```

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
Returns an integer representing the client's file descriptor
or FALSE if it fails.

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

#### `(shutdown-connection ?socketfdOrLogicalName ?optionalHow)`

Shut down part or all of a full-duplex connection.

Possible values of `?optionalHow`:

* `SHUT_RD`: further receptions will be disallowed
* `SHUT_WR`: further transmissions will be disallowed
* `SHUT_RDWR`: further receptions and transmissions will be disallowed

### Debugging

In order to watch all activity on your computer's port 8888
(which the example server and client use by default),
use `tcpdump`. You may need to run as `root` or with `sudo`:

```
tcpdump -nn -i any port 8888
```

### Small technical braindump

This codebase is based on
[CLIPS version 6.4.1](https://sourceforge.net/projects/clipsrules/files/CLIPS/6.4.1/)
released on 4/8/23. I added a `socketrtr.h` and `socketrtr.c` to support reading/writing to sockets.
I add user defined functions (UDFs) to CLIPS environments compiled with this source code
in `userfunctions.c`. I initialize the socket router in `router.c`
inside of the function `InitializeDefaultRouters`. Finally, I updated the makefile
to use gnu99 as well as to include the new `socketrtr.c` files.

