;;; Stream sockets in the AF_UNIX domain.
;;;
;;; bind-socket takes a filesystem path and no port here, and the path becomes
;;; the logical name verbatim. The socket file is removed before binding so a
;;; rerun does not fail with EADDRINUSE, and again afterwards so the test
;;; leaves nothing behind.

(load* "tests/lib/expect.clp")
(test-suite "unix-stream")
(test-plan 24)

(defglobal ?*path* = "/tmp/clipsockets-test-unix.sock")
(defglobal ?*client-path* = "/tmp/clipsockets-test-unix-client.sock")

(deffunction run-tests ()
   (remove ?*path*)

   (bind ?srv (create-socket AF_UNIX SOCK_STREAM))
   (expect-gte "server socket created" 0 ?srv)

   (bind ?sname (bind-socket ?srv /tmp/clipsockets-test-unix.sock))
   (expect-eq   "the path is the logical name"
                /tmp/clipsockets-test-unix.sock ?sname)
   (expect-true "server listening" (listen ?srv))

   (bind ?cli (create-socket AF_UNIX SOCK_STREAM))
   (bind ?conn (connect ?cli /tmp/clipsockets-test-unix.sock))
   (expect-eq "connect names the connection after the path it reached"
              1 (str-index "/tmp/clipsockets-test-unix.sock#" ?conn))

   (bind ?cfd (accept ?srv))
   (expect-gte "accept returns a descriptor" 0 ?cfd)
   (bind ?cname (get-socket-logical-name ?cfd))

   (printout ?conn "ping over unix" crlf)
   (flush-connection ?cli)
   (expect-true "server sees readable data" (poll ?cfd 5000 POLLIN))
   (expect-eq "server reads what the client wrote" "ping over unix" (readline ?cname))

   (printout ?cname "pong over unix" crlf)
   (flush-connection ?cfd)
   (expect-eq "client reads the reply" "pong over unix" (readline ?conn))

   (expect-true "close accepted socket" (close-connection ?cfd))
   (expect-true "close client" (close-connection ?cli))
   (expect-true "close server" (close-connection ?srv))

   (remove ?*path*))

;;; Two clients on one path at the same time.
;;;
;;; A unix client binds no path of its own. As a result, accept has no peer
;;; address for the name of the connection. It uses the path of the connection
;;; instead, and it puts the descriptor after that path. Each client of this
;;; server arrives on the same path.
;;;
;;; Without the descriptor, the two accepted sockets have one name and no code
;;; reports a problem. The first read gives data from the socket that the
;;; router finds first. The second read then waits on that same socket for a
;;; message that went to the other socket. The section above cannot see this
;;; error, because one connection passes each naming scheme.
(deffunction run-two-client-tests ()
   (remove ?*path*)
   (bind ?srv (create-socket AF_UNIX SOCK_STREAM))
   (bind-socket ?srv /tmp/clipsockets-test-unix.sock)
   (listen ?srv)

   (bind ?c1 (create-socket AF_UNIX SOCK_STREAM))
   (bind ?n1 (connect ?c1 /tmp/clipsockets-test-unix.sock))
   (bind ?c2 (create-socket AF_UNIX SOCK_STREAM))
   (bind ?n2 (connect ?c2 /tmp/clipsockets-test-unix.sock))

   (bind ?a1 (accept ?srv))
   (bind ?a2 (accept ?srv))
   (bind ?an1 (get-socket-logical-name ?a1))
   (bind ?an2 (get-socket-logical-name ?a2))

   ;; Each read below has a time limit. Two sockets with one name do not fail
   ;; a read. They send it to the incorrect socket, and the next read then
   ;; waits on a socket whose message another read already took. Without a
   ;; timeout, the runner would stop this file and no check would fail. A stop
   ;; also says nothing about the incorrect name.
   (set-timeout ?a1 2 0)
   (set-timeout ?a2 2 0)
   (set-timeout ?c1 2 0)
   (set-timeout ?c2 2 0)

   ;; This is the defect. An accepted unix socket had no name, because the
   ;; code made the name from a peer address, and a unix client sends no such
   ;; address.
   ;; The check uses the length and does not compare with "". The name is a
   ;; symbol, and the empty symbol is not (eq) to the empty string. As a
   ;; result, a comparison would pass for a socket with no name.
   (expect-gte "an accepted socket has a name" 1 (str-length ?an1))
   (expect-eq "which is the path the connection arrived on"
              1 (str-index "/tmp/clipsockets-test-unix.sock#" ?an1))
   (expect-neq "and the two accepted sockets are named differently"
               ?an1 ?an2)

   ;; The four ends are in this one process, and the four names must be
   ;; different.
   (expect-neq "a client and its own accepted end differ" ?n1 ?an1)
   (expect-neq "and so do the two clients" ?n1 ?n2)

   ;; Each client sends a message of its own. As a result, a name that reaches
   ;; the incorrect socket gives an incorrect string and not an empty
   ;; result.
   (printout ?n1 "from unix client one" crlf)
   (printout ?n2 "from unix client two" crlf)
   (flush-connection ?c1)
   (flush-connection ?c2)

   ;; The test reads in the opposite sequence to the sends. As a result, a
   ;; router that answers with the first socket that it finds cannot pass this
   ;; check.
   (expect-eq "the second client's message reaches its own socket"
              "from unix client two" (readline ?an2))
   (expect-eq "and the first client's message reaches its own"
              "from unix client one" (readline ?an1))

   ;; The same test in the other direction. A server replies in that
   ;; direction.
   (printout ?an1 "reply to one" crlf)
   (printout ?an2 "reply to two" crlf)
   (flush-connection ?a1)
   (flush-connection ?a2)
   (expect-eq "the first client reads its own reply"
              "reply to one" (readline ?n1))
   (expect-eq "the second client reads its own reply"
              "reply to two" (readline ?n2))

   (close-connection ?c1)
   (close-connection ?c2)
   (close-connection ?a1)
   (close-connection ?a2)
   (close-connection ?srv)
   (remove ?*path*))

;;; A client that bound a path of its own before it connected.
;;;
;;; The sections above leave the client unbound, and almost every unix client
;;; is unbound. accept then reports a length for the family alone, there is no
;;; peer path to use, and the accepted socket takes its name from the path that
;;; the listen socket holds.
;;;
;;; A client that did bind a path is the other half of that rule, and the name
;;; must then come from the peer. Without this section the suite never sees a
;;; unix peer with an address, and the two halves of the naming rule read as
;;; one.
(deffunction run-bound-client-tests ()
   (remove ?*path*)
   (remove ?*client-path*)

   (bind ?srv (create-socket AF_UNIX SOCK_STREAM))
   (bind-socket ?srv /tmp/clipsockets-test-unix.sock)
   (listen ?srv)

   (bind ?cli (create-socket AF_UNIX SOCK_STREAM))
   (bind ?cname (bind-socket ?cli /tmp/clipsockets-test-unix-client.sock))
   (expect-eq "the client's own path is its logical name"
              /tmp/clipsockets-test-unix-client.sock ?cname)

   ;; The client keeps the name of the path it reached, whether it bound a
   ;; path of its own or not. Only the accepted end changes.
   (bind ?conn (connect ?cli /tmp/clipsockets-test-unix.sock))
   (expect-eq "connect still names the connection after the path it reached"
              1 (str-index "/tmp/clipsockets-test-unix.sock#" ?conn))

   (bind ?acc (accept ?srv))
   (expect-eq "accept names a bound client after the client's own path"
              1 (str-index "/tmp/clipsockets-test-unix-client.sock#"
                           (get-socket-logical-name ?acc)))

   ;; The name is a name and not only a string. The connection has to work
   ;; through it.
   (printout ?conn "from a bound client" crlf)
   (flush-connection ?cli)
   (expect-eq "the accepted socket reads through that name"
              "from a bound client" (readline (get-socket-logical-name ?acc)))

   (close-connection ?cli)
   (close-connection ?acc)
   (close-connection ?srv)
   (remove ?*path*)
   (remove ?*client-path*))

(run-tests)
(run-two-client-tests)
(run-bound-client-tests)
(test-summary)
