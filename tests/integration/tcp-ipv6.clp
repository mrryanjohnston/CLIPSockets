;;; TCP over IPv6 loopback.
;;;
;;; IPv6 logical names are bracketed -- [::1]:port -- which is the one thing
;;; that genuinely differs from the IPv4 path.

(load* "tests/lib/expect.clp")
(test-suite "tcp-ipv6")
(test-plan 11)

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
