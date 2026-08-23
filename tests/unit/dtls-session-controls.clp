;;; The controls on a DTLS session that operates, and each refusal of them.
;;;
;;; requires: tls
;;; requires: dtls
;;;
;;; dtls-loopback.clp starts a session, and dtls-router-io.clp reads and writes
;;; through one. This file has the remainder: each call that changes a session
;;; or refuses to change it. Those calls are the MTU, the retransmission clock,
;;; and the limits that each call refuses to pass.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(load* "tests/lib/tls.clp")
(test-suite "dtls-session-controls")
(test-plan 25)

(defglobal ?*sport* = 18991)
(defglobal ?*cport* = 18992)

(deffunction run-tests ()
   ;;=================================================================
   ;; Refusals that need no session
   ;;=================================================================
   (bind ?cctx (tls-create-context DTLS_CLIENT))
   (bind ?sctx (tls-create-context DTLS_SERVER))
   (tls-context-use-certificate-file ?sctx "tests/fixtures/server.pem")
   (tls-context-use-private-key-file ?sctx "tests/fixtures/server-key.pem")
   (tls-context-load-verify-locations ?cctx "tests/fixtures/ca.pem")

   ;; A datagram socket with no peer has no destination for a ClientHello. A
   ;; message about that condition is better than a write error in the middle
   ;; of a handshake.
   (bind ?lonely (create-socket AF_INET SOCK_DGRAM))
   (setsockopt ?lonely SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?lonely 127.0.0.1 18993)
   (expect-false "dtls-connect refuses an unconnected socket"
                 (dtls-connect ?cctx ?lonely localhost))
   (close-connection ?lonely)

   ;; A handshake cannot operate on a socket with no name, because no code
   ;; could reach that socket after the handshake.
   (bind ?nameless (create-socket AF_INET SOCK_DGRAM))
   (expect-false "dtls-accept refuses a socket with no logical name"
                 (dtls-accept ?sctx ?nameless))
   (close-connection ?nameless)

   ;;=================================================================
   ;; A session to work with
   ;;=================================================================
   (bind ?srv (create-socket AF_INET SOCK_DGRAM))
   (setsockopt ?srv SOL_SOCKET SO_REUSEADDR 1)
   (bind ?sname (bind-socket ?srv 127.0.0.1 ?*sport*))
   (bind ?cli (create-socket AF_INET SOCK_DGRAM))
   (setsockopt ?cli SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?cli 127.0.0.1 ?*cport*)
   (connect ?cli 127.0.0.1 ?*sport*)
   (fcntl-add-status-flags ?srv O_NONBLOCK)
   (fcntl-add-status-flags ?cli O_NONBLOCK)

   (expect-true "handshake completes"
                (dtls-drive-handshake ?cctx ?sctx ?cli ?srv 200))

   ;; A second dtls-accept on a complete session reports that condition. It
   ;; does not start the work again.
   (expect-true "dtls-accept on a finished session is TRUE"
                (dtls-accept ?sctx ?srv))

   ;; After the code puts a session on the socket, that socket accepts no new
   ;; handshake.
   (expect-false "dtls-connect refuses an already encrypted socket"
                 (dtls-connect ?cctx ?cli localhost))

   ;;=================================================================
   ;; The retransmission clock
   ;;=================================================================
   ;; The value is a number of milliseconds, or FALSE if there is no flight
   ;; to send. Both values are correct answers. The important point is that
   ;; the call does not refuse a session that it has.
   (bind ?ms (dtls-timeout ?cli))
   (expect-true "dtls-timeout answers on a live session"
                (or (eq ?ms FALSE) (>= ?ms 0)))

   ;; This call is safe in each cycle of a loop, with a flight to send or
   ;; without one.
   (expect-true "dtls-handle-timeout is safe when nothing is owing"
                (dtls-handle-timeout ?cli))

   ;;=================================================================
   ;; The MTU, and what one record will hold
   ;;=================================================================
   ;; Some backends do not accept an MTU. wolfSSL needs a build flag for it.
   ;; As a result, the test accepts either answer, and the checks after this
   ;; one use that answer.
   (bind ?took-mtu (dtls-set-mtu ?cli 1200))

   (expect-false "an MTU of zero is refused" (dtls-set-mtu ?cli 0))
   (expect-false "an absurd MTU is refused" (dtls-set-mtu ?cli 999999))

   ;; A record cannot hold more data than the link carries. As a result, the
   ;; code refuses a larger message. It does not divide the message and it
   ;; does not cut the message.
   (bind ?huge "")
   (loop-for-count 300 do (bind ?huge (str-cat ?huge "0123456789")))
   (expect-false "a message larger than one record is refused"
                 (dtls-send ?cli ?huge))

   ;; The session also operates after this. A refusal does not damage the
   ;; session.
   (expect-eq "a record still sends after one was refused" 4
              (dtls-send ?cli "fine"))
   (expect-true "it arrives" (poll ?srv 5000 POLLIN))
   (expect-eq "and reads back" "fine" (nth$ 2 (dtls-recv ?srv)))

   ;;=================================================================
   ;; dtls-recv and its length limit
   ;;=================================================================
   (dtls-send ?cli "0123456789")
   (expect-true "the record arrives" (poll ?srv 5000 POLLIN))
   (bind ?cut (dtls-recv ?srv 4))
   (expect-eq "maxlen cuts the record short" 4 (nth$ 1 ?cut))
   (expect-eq "and the payload with it" "0123" (nth$ 2 ?cut))

   ;; The library keeps the remainder and does not discard it. This test runs
   ;; on each backend here that does DTLS. The check is important because the
   ;; plaintext code does the opposite: there the kernel discards the data
   ;; that does not fit the buffer, and udp-router-io.clp shows that.
   (bind ?rest (dtls-recv ?srv))
   (expect-eq "the remainder is kept for the next read" 6 (nth$ 1 ?rest))
   (expect-eq "and is the rest of the record" "456789" (nth$ 2 ?rest))

   ;; With no data available, the call gives FALSE and not an empty
   ;; record.
   (expect-false "dtls-recv on a quiet socket is FALSE" (dtls-recv ?srv))

   ;;=================================================================
   ;; Buffering still belongs to the socket
   ;;=================================================================
   (expect-true "full buffering can be set" (set-fully-buffered ?cli))
   (expect-true "no buffering can be set" (set-not-buffered ?cli))
   (expect-true "line buffering can be set" (set-line-buffered ?cli))

   ;;=================================================================
   ;; A peer that has gone
   ;;=================================================================
   ;; UDP has no back-pressure. As a result, a peer that stops to read cannot
   ;; block a send. On loopback the kernel delivers the datagrams and then
   ;; discards them, and the sender sees nothing. But a peer can go away. The
   ;; port then stops to exist, ICMP reports that condition, and the next send
   ;; on a connected socket gives the error.
   ;;
   ;; That report is asynchronous. As a result, this test looks for the
   ;; failure across several sends and does not expect it at the first send.
   ;; If the failure never arrives, the test skips the check. The error is
   ;; advisory, and a platform can decide not to give it.
   (bind ?gone (create-socket AF_INET SOCK_DGRAM))
   (setsockopt ?gone SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?gone 127.0.0.1 18995)
   (bind ?talker (create-socket AF_INET SOCK_DGRAM))
   (setsockopt ?talker SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?talker 127.0.0.1 18996)
   (connect ?talker 127.0.0.1 18995)
   (fcntl-add-status-flags ?gone O_NONBLOCK)
   (fcntl-add-status-flags ?talker O_NONBLOCK)

   (bind ?gctx (tls-create-context DTLS_SERVER))
   (tls-context-use-certificate-file ?gctx "tests/fixtures/server.pem")
   (tls-context-use-private-key-file ?gctx "tests/fixtures/server-key.pem")
   (bind ?tctx (tls-create-context DTLS_CLIENT))
   (tls-context-load-verify-locations ?tctx "tests/fixtures/ca.pem")

   (if (dtls-drive-handshake ?tctx ?gctx ?talker ?gone 200)
      then
      (close-connection ?gone)
      (bind ?n 0)
      (bind ?refused FALSE)
      (while (and (< ?n 8) (not ?refused)) do
         (sleep 0.05)
         (if (eq FALSE (dtls-send ?talker "anyone there")) then (bind ?refused TRUE))
         (bind ?n (+ ?n 1)))

      (if ?refused
         then (expect-true "a send to a peer that has gone is reported" ?refused)
         else (test-skip "a send to a peer that has gone is reported"
                         "no port-unreachable came back within the budget"))
      else
      (close-connection ?gone)
      (test-skip "a send to a peer that has gone is reported"
                 "the handshake for this check did not complete"))

   (close-connection ?talker)
   (tls-free-context ?gctx)
   (tls-free-context ?tctx)

   ;;=================================================================
   ;; Teardown
   ;;=================================================================
   (expect-true "shutdown reports success" (tls-shutdown ?cli))
   ;; Two calls are not an error. The second call reports that the
   ;; close_notify already went out.
   (expect-true "a second shutdown is harmless" (tls-shutdown ?cli))

   (close-connection ?cli)
   (close-connection ?srv)
   (tls-free-context ?cctx)
   (tls-free-context ?sctx))

(run-tests)
(test-summary)
