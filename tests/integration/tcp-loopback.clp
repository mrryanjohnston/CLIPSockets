;;; TCP over IPv4 loopback: a full client/server round trip.
;;;
;;; Both ends live in this one process. connect() completes against the
;;; listen backlog without accept() having been called yet, so a single
;;; process can play both roles without deadlocking -- which avoids the
;;; port races and orphaned children of coordinating two clips invocations.

(load* "tests/lib/expect.clp")
(test-suite "tcp-loopback")
(test-plan 18)

(defglobal ?*port* = 18901)

(deffunction run-tests ()
   (bind ?srv (create-socket AF_INET SOCK_STREAM))
   (expect-gte "server socket created" 0 ?srv)
   (setsockopt ?srv SOL_SOCKET SO_REUSEADDR 1)

   (bind ?sname (bind-socket ?srv 127.0.0.1 ?*port*))
   (expect-eq   "server bound" (sym-cat "127.0.0.1:" ?*port*) ?sname)
   (expect-true "server listening" (listen ?srv))

   (bind ?cli (create-socket AF_INET SOCK_STREAM))
   ;; The connection is named for the peer plus this socket's descriptor, so
   ;; that several connections to one server stay distinguishable. The
   ;; descriptor number is not predictable, so only the peer part is pinned.
   (bind ?conn (connect ?cli 127.0.0.1 ?*port*))
   (expect-eq "connect names the connection after the peer it reached"
              1 (str-index (sym-cat "127.0.0.1:" ?*port* "#") ?conn))

   ;; The accepted socket is named for the client's ephemeral address, so it
   ;; is a different logical name from the listening socket.
   (bind ?cfd (accept ?srv))
   (expect-gte "accept returns a descriptor" 0 ?cfd)
   (bind ?cname (get-socket-logical-name ?cfd))
   (expect-true "accepted socket has a logical name" ?cname)
   (expect-neq  "accepted name differs from the listening name" ?sname ?cname)

   ;; Client -> server.
   (printout ?conn "ping from client" crlf)
   (expect-true "client flush" (flush-connection ?cli))
   (expect-true "server sees readable data" (poll ?cfd 5000 POLLIN))
   (expect-eq "server reads what the client wrote"
              "ping from client" (readline ?cname))

   ;; Server -> client, with the socket options the examples use.
   (expect-true "TCP_NODELAY on the accepted socket"
                (setsockopt ?cfd IPPROTO_TCP TCP_NODELAY 1))
   (expect-true "accepted socket can be made unbuffered" (set-not-buffered ?cfd))
   (printout ?cname "pong from server" crlf)
   (expect-true "server flush" (flush-connection ?cfd))
   (expect-eq "client reads the reply" "pong from server" (readline ?conn))

   ;; Teardown.
   (expect-true "shutdown accepted socket" (shutdown-connection ?cfd))
   (expect-true "close accepted socket" (close-connection ?cfd))
   (expect-true "close client" (close-connection ?cli))
   (expect-true "close server" (close-connection ?srv)))

(run-tests)
(test-summary)
