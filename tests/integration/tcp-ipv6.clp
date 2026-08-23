;;; TCP over IPv6 loopback.
;;;
;;; IPv6 logical names are bracketed -- [::1]:port -- which is the one thing
;;; that genuinely differs from the IPv4 path.

(load* "tests/lib/expect.clp")
(test-suite "tcp-ipv6")
(test-plan 14)

(defglobal ?*port* = 18902)

(deffunction run-tests ()
   (bind ?srv (create-socket AF_INET6 SOCK_STREAM))
   (expect-gte "server socket created" 0 ?srv)
   (setsockopt ?srv SOL_SOCKET SO_REUSEADDR 1)

   (bind ?sname (bind-socket ?srv ::1 ?*port*))
   (expect-eq   "IPv6 logical names bracket the address"
                (sym-cat "[::1]:" ?*port*) ?sname)
   (expect-true "server listening" (listen ?srv))

   (bind ?cli (create-socket AF_INET6 SOCK_STREAM))
   (bind ?conn (connect ?cli ::1 ?*port*))
   (expect-eq "connect names the connection after the bracketed peer"
              1 (str-index (sym-cat "[::1]:" ?*port* "#") ?conn))

   (bind ?cfd (accept ?srv))
   (expect-gte "accept returns a descriptor" 0 ?cfd)
   (bind ?cname (get-socket-logical-name ?cfd))

   ;; accept makes its name from the peer address in the same manner as
   ;; bind-socket and connect. As a result, it must use the brackets in the
   ;; same manner: the address inside the brackets and the port after them. A
   ;; port inside the brackets makes the full text one address. [::1:33108] is
   ;; such a text, and it is a correct and different IPv6 address.
   ;;
   ;; The port is not known before the test, because the kernel gives the
   ;; client a port. As a result, the test checks the start and the end of the
   ;; name and not the middle.
   (expect-eq   "accept brackets the address and leaves the port outside"
                1 (str-index "[::1]:" ?cname))
   (expect-true "and appends the descriptor to tell connections apart"
                (str-index (str-cat "#" ?cfd) ?cname))
   (expect-false "the address is closed once and not again"
                 (str-index "]" (sub-string (+ 1 (str-index "]" ?cname))
                                            (str-length ?cname) ?cname)))

   (printout ?conn "ping over v6" crlf)
   (flush-connection ?cli)
   (expect-true "server sees readable data" (poll ?cfd 5000 POLLIN))
   (expect-eq "server reads what the client wrote" "ping over v6" (readline ?cname))

   (printout ?cname "pong over v6" crlf)
   (flush-connection ?cfd)
   (expect-eq "client reads the reply" "pong over v6" (readline ?conn))

   (expect-true "close accepted socket" (close-connection ?cfd))
   (expect-true "close client" (close-connection ?cli))
   (expect-true "close server" (close-connection ?srv)))

(run-tests)
(test-summary)
