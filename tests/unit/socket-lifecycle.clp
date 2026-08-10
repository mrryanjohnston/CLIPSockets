;;; The single-socket half of the API: create, configure, bind, listen, close.
;;; Anything needing a peer on the other end belongs in tests/integration.

(load* "tests/lib/expect.clp")
(test-suite "socket-lifecycle")
(test-plan 14)

(defglobal ?*port* = 18888)

(deffunction run-tests ()
   (bind ?fd (create-socket AF_INET SOCK_STREAM))
   (expect-gte "create-socket returns a file descriptor" 0 ?fd)

   ;; SO_REUSEADDR keeps a rerun from tripping over its own TIME_WAIT sockets.
   (expect-true "setsockopt SO_REUSEADDR"
                (setsockopt ?fd SOL_SOCKET SO_REUSEADDR 1))
   (expect-eq   "getsockopt reads SO_REUSEADDR back" 1
                (getsockopt ?fd SOL_SOCKET SO_REUSEADDR))

   ;; bind-socket answers with the logical name used for I/O on that socket.
   (bind ?name (bind-socket ?fd 127.0.0.1 ?*port*))
   (expect-eq "bind-socket returns host:port as the logical name"
              (sym-cat "127.0.0.1:" ?*port*) ?name)
   (expect-eq "get-socket-logical-name agrees with bind-socket"
              ?name (get-socket-logical-name ?fd))

   (expect-true "listen succeeds on a bound socket" (listen ?fd))

   ;; Timeouts are in microseconds at both ends. The kernel rounds SO_RCVTIMEO
   ;; up to its timer granularity (1ms), so use a whole number of milliseconds
   ;; here -- asking for 5us reads back as 1000us.
   (expect-eq "timeout defaults to 0" 0 (get-timeout ?fd))
   (set-timeout ?fd 250000)
   (expect-eq "set-timeout is readable afterwards" 250000 (get-timeout ?fd))

   (expect-true "close-connection succeeds" (close-connection ?fd)))

(deffunction run-rejection-tests ()
   ;; Unknown enum symbols are reported, not crashed on.
   (expect-false "create-socket rejects an unknown domain"
                 (create-socket NOT_A_DOMAIN SOCK_STREAM))
   (expect-false "create-socket rejects an unknown type"
                 (create-socket AF_INET NOT_A_TYPE))

   ;; Descriptors that were never sockets must fail rather than take effect.
   (expect-false "close-connection rejects an unknown descriptor"
                 (close-connection 9999))
   (expect-false "flush-connection rejects an unknown descriptor"
                 (flush-connection 9999))
   (expect-false "get-timeout rejects an unknown descriptor"
                 (get-timeout 9999)))

(run-tests)
(run-rejection-tests)
(test-summary)
