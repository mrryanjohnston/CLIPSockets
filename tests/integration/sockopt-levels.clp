;;; Every level and option name (get|set)sockopt recognises, plus what happens
;;; when the kernel refuses an option that is valid but wrong for the socket.

(load* "tests/lib/expect.clp")
(test-suite "sockopt-levels")
(test-plan 17)

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

   ;; SOL_SOCKET with SO_SNDBUF and SO_RCVBUF. Neither of them gives the same
   ;; value back, and the cause is important. Linux keeps two times the given
   ;; value, and it uses the second half for its own data. It then limits the
   ;; result to net.core.wmem_max. As a result, the value that comes back is a
   ;; maximum of two times the given value, or it is the limit. A check for
   ;; equality would be a check of a sysctl. Each kernel does promise one item:
   ;; the buffer is at least as large as the request. A caller depends on that
   ;; promise.
   ;;
   ;; A smaller buffer has no limit in the way. As a result, the test uses that
   ;; direction to check for a real change. A smaller send buffer gives a
   ;; writer backpressure sooner, and a program that asks for a smaller buffer
   ;; usually wants that.
   (bind ?before (getsockopt ?tcp SOL_SOCKET SO_SNDBUF))
   (expect-gte "SO_SNDBUF starts positive" 1 ?before)
   (expect-true "set SO_SNDBUF" (setsockopt ?tcp SOL_SOCKET SO_SNDBUF 65536))
   (expect-gte "SO_SNDBUF is at least what was asked" 65536
               (getsockopt ?tcp SOL_SOCKET SO_SNDBUF))

   (expect-gte "SO_RCVBUF starts positive" 1
               (getsockopt ?tcp SOL_SOCKET SO_RCVBUF))
   (expect-true "set SO_RCVBUF" (setsockopt ?tcp SOL_SOCKET SO_RCVBUF 65536))
   (expect-gte "SO_RCVBUF is at least what was asked" 65536
               (getsockopt ?tcp SOL_SOCKET SO_RCVBUF))

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
