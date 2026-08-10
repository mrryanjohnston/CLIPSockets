;;; Every level and option name (get|set)sockopt recognises, plus what happens
;;; when the kernel refuses an option that is valid but wrong for the socket.

(load* "tests/lib/expect.clp")
(test-suite "sockopt-levels")
(test-plan 11)

(deffunction run-tests ()
   (bind ?tcp (create-socket AF_INET SOCK_STREAM))

   ;; SOL_SOCKET / SO_REUSEADDR round trip.
   (expect-true "set SO_REUSEADDR" (setsockopt ?tcp SOL_SOCKET SO_REUSEADDR 1))
   (expect-eq   "get SO_REUSEADDR" 1 (getsockopt ?tcp SOL_SOCKET SO_REUSEADDR))

   ;; IPPROTO_TCP / TCP_NODELAY round trip.
   (expect-eq   "TCP_NODELAY starts clear" 0
                (getsockopt ?tcp IPPROTO_TCP TCP_NODELAY))
   (expect-true "set TCP_NODELAY" (setsockopt ?tcp IPPROTO_TCP TCP_NODELAY 1))
   (expect-eq   "get TCP_NODELAY" 1 (getsockopt ?tcp IPPROTO_TCP TCP_NODELAY))

   ;; Names the UDF does not know.
   (expect-false "unknown level on get" (getsockopt ?tcp NOT_A_LEVEL SO_REUSEADDR))
   (expect-false "unknown option on get" (getsockopt ?tcp SOL_SOCKET NOT_AN_OPTION))
   (expect-false "unknown level on set" (setsockopt ?tcp NOT_A_LEVEL SO_REUSEADDR 1))
   (expect-false "unknown option on set" (setsockopt ?tcp SOL_SOCKET NOT_AN_OPTION 1))

   ;; Known names the kernel rejects for this socket type: TCP options do not
   ;; apply to a datagram socket.
   (bind ?udp (create-socket AF_INET SOCK_DGRAM))
   (expect-false "TCP_NODELAY on a UDP socket is refused"
                 (setsockopt ?udp IPPROTO_TCP TCP_NODELAY 1))
   (expect-false "reading TCP_NODELAY from a UDP socket is refused"
                 (getsockopt ?udp IPPROTO_TCP TCP_NODELAY))

   (close-connection ?tcp)
   (close-connection ?udp))

(run-tests)
(test-summary)
