;;; UDP datagrams via (sendto) and (rcvfrom).
;;;
;;; rcvfrom answers a 5-field multifield:
;;;
;;;   1 family   2 address   3 port   4 bytes read   5 payload
;;;
;;; The sending socket is never bound -- (sendto) on a fresh socket is the
;;; pattern examples/client-udp-sendto.bat uses.

(load* "tests/lib/expect.clp")
(test-suite "udp-datagram")
(test-plan 13)

(defglobal ?*port* = 18903)

(deffunction run-tests ()
   (bind ?srv (create-socket AF_INET SOCK_DGRAM))
   (expect-gte "server socket created" 0 ?srv)
   (setsockopt ?srv SOL_SOCKET SO_REUSEADDR 1)
   (expect-eq "server bound" (sym-cat "127.0.0.1:" ?*port*)
              (bind-socket ?srv 127.0.0.1 ?*port*))

   (bind ?cli (create-socket AF_INET SOCK_DGRAM))
   (bind ?sent (sendto ?cli AF_INET "127.0.0.1" ?*port* "udp hello"))
   (expect-eq "sendto reports the payload length" 9 ?sent)

   (expect-true "datagram is readable" (poll ?srv 5000 POLLIN))
   (bind ?mf (rcvfrom ?srv))
   (expect-length "rcvfrom returns five fields" 5 ?mf)
   (expect-eq "field 1 is the address family" AF_INET   (nth$ 1 ?mf))
   (expect-eq "field 2 is the sender address" 127.0.0.1 (nth$ 2 ?mf))
   (expect-gte "field 3 is the sender port" 1           (nth$ 3 ?mf))
   (expect-eq "field 4 is the byte count" 9             (nth$ 4 ?mf))
   (expect-eq "field 5 is the payload" "udp hello"      (nth$ 5 ?mf))

   ;; Replying means connecting the bound socket back to the sender, which is
   ;; what gives it a logical name usable with printout.
   (connect ?srv (nth$ 2 ?mf) (nth$ 3 ?mf))
   (bind ?reply-name (get-socket-logical-name ?srv))
   (expect-true "server socket has a name after connect" ?reply-name)

   (expect-true "close client" (close-connection ?cli))
   (expect-true "close server" (close-connection ?srv)))

(run-tests)
(test-summary)
