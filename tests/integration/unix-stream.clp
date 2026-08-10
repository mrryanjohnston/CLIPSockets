;;; Stream sockets in the AF_UNIX domain.
;;;
;;; bind-socket takes a filesystem path and no port here, and the path becomes
;;; the logical name verbatim. The socket file is removed before binding so a
;;; rerun does not fail with EADDRINUSE, and again afterwards so the test
;;; leaves nothing behind.

(load* "tests/lib/expect.clp")
(test-suite "unix-stream")
(test-plan 11)

(defglobal ?*path* = "/tmp/clipsockets-test-unix.sock")

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

(run-tests)
(test-summary)
