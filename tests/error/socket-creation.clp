;;; create-socket, listen, accept and shutdown-connection argument forms,
;;; including the optional arguments and the failures the kernel reports.

(load* "tests/lib/expect.clp")
(test-suite "socket-creation")
(test-plan 13)

(defglobal ?*port* = 18921)
(defglobal ?*shutdown-port* = 18926)

(deffunction run-tests ()
   ;; The third argument is the protocol. It was declared as a symbol while
   ;; being read as an integer, which made it impossible to pass.
   (bind ?fd (create-socket AF_INET SOCK_STREAM 0))
   (expect-gte "explicit protocol 0 is accepted" 0 ?fd)
   (close-connection ?fd)

   (expect-false "an unsupported protocol number fails"
                 (create-socket AF_INET SOCK_STREAM 99))

   ;; Every supported domain and type.
   (bind ?a (create-socket AF_INET6 SOCK_DGRAM))
   (expect-gte "AF_INET6 datagram socket" 0 ?a)
   (bind ?b (create-socket AF_UNIX SOCK_DGRAM))
   (expect-gte "AF_UNIX datagram socket" 0 ?b)
   (close-connection ?a)
   (close-connection ?b))

(deffunction run-listen-tests ()
   (bind ?srv (create-socket AF_INET SOCK_STREAM))
   (setsockopt ?srv SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?srv 127.0.0.1 ?*port*)

   ;; The optional second argument is the backlog.
   (expect-true "listen with an explicit backlog" (listen ?srv 5))

   ;; listen is meaningless on a datagram socket, so the syscall refuses it.
   (bind ?udp (create-socket AF_INET SOCK_DGRAM))
   (expect-false "listen on a datagram socket fails" (listen ?udp))
   (expect-eq "which the kernel reports as EOPNOTSUPP" EOPNOTSUPP (errno-sym))

   ;; accept on a socket that was never listening.
   (bind ?lone (create-socket AF_INET SOCK_STREAM))
   (expect-false "accept without listen fails" (accept ?lone))

   (close-connection ?udp)
   (close-connection ?lone)
   (close-connection ?srv))

(deffunction run-shutdown-tests ()
   ;; shutdown-connection takes an optional direction. It was registered as
   ;; taking exactly one argument, so none of these could be passed.
   ;;
   ;; shutdown() only applies to a connected socket -- on a freshly created one
   ;; it fails with ENOTCONN -- so this needs a real connection.
   (bind ?srv (create-socket AF_INET SOCK_STREAM))
   (setsockopt ?srv SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?srv 127.0.0.1 ?*shutdown-port*)
   (listen ?srv)
   (bind ?cli (create-socket AF_INET SOCK_STREAM))
   (connect ?cli 127.0.0.1 ?*shutdown-port*)
   (bind ?acc (accept ?srv))

   (expect-true "SHUT_RD" (shutdown-connection ?cli SHUT_RD))
   (expect-true "SHUT_WR" (shutdown-connection ?cli SHUT_WR))
   (expect-true "SHUT_RDWR" (shutdown-connection ?cli SHUT_RDWR))
   (expect-true "an unrecognised direction falls back to SHUT_RDWR"
                (shutdown-connection ?cli NOT_A_DIRECTION))
   (expect-true "no direction defaults to SHUT_RDWR" (shutdown-connection ?cli))

   (close-connection ?acc)
   (close-connection ?cli)
   (close-connection ?srv))

(run-tests)
(run-listen-tests)
(run-shutdown-tests)
(test-summary)
