;;; A handshake that continues after a lost flight.
;;;
;;; requires: tls
;;; requires: dtls
;;;
;;; A DTLS implementation needs this test most, and it is the easiest test to
;;; omit. Each other DTLS test here runs on loopback, where no packet is lost,
;;; out of sequence or late. As a result, a handshake that never sends a flight
;;; again passes each of those tests. It then stops on the first real network
;;; that loses a packet.
;;;
;;; This test makes the loss and does not wait for one. The server reads the
;;; first flight of the client from its own socket with (rcvfrom) and discards
;;; it. At that time there is no session to see the flight. For the client,
;;; that datagram never arrived. Nothing then moves until the retransmission
;;; timer of the client ends and it sends the flight again. That is the path
;;; under test.
;;;
;;; This test does not use a relay, and that is on purpose. A relay is a third
;;; socket that forwards datagrams and discards some of them, and it cannot
;;; operate here. (rcvfrom) gives the data back as an H/L string, and such a
;;; string ends at the first NUL byte. Handshake records are binary. As a
;;; result, a relay would cut each flight that it carried. To discard a flight
;;; needs no exact copy.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(load* "tests/lib/tls.clp")
(test-suite "dtls-retransmit")
(test-plan 8)

(defglobal ?*sport* = 18985)
(defglobal ?*cport* = 18986)

;;; Runs the two ends and gives the number of times that the client sent a
;;; flight again. This function counts and the shared function does not,
;;; because the count is the purpose of the test. A count of zero would show
;;; that the code below lost no flight.
(deffunction drive-counting (?cctx ?sctx ?cfd ?sfd ?limit)
   (bind ?client-done FALSE)
   (bind ?server-done FALSE)
   (bind ?resends 0)
   (bind ?i 0)
   (while (and (< ?i ?limit)
               (or (not ?client-done) (not ?server-done))) do
      (if (not ?client-done) then
         ;; See the note in tests/lib/tls.clp. The library decides if it
         ;; must send a flight again, because (dtls-timeout) does not give
         ;; the same measurement on each backend.
         (if (dtls-handle-timeout ?cfd) then (bind ?resends (+ ?resends 1)))
         (bind ?client-done (tls-handshake ?cfd)))

      (if (not ?server-done) then
         (bind ?server-done (dtls-accept ?sctx ?sfd)))

      (poll ?sfd 20 POLLIN)
      (poll ?cfd 20 POLLIN)
      (bind ?i (+ ?i 1)))

   (return (create$ (and ?client-done ?server-done) ?resends)))

(deffunction run-tests ()
   (bind ?sctx (tls-create-context DTLS_SERVER))
   (expect-true "server certificate loads"
                (tls-context-use-certificate-file ?sctx "tests/fixtures/server.pem"))
   (expect-true "server key loads"
                (tls-context-use-private-key-file ?sctx "tests/fixtures/server-key.pem"))

   (bind ?cctx (tls-create-context DTLS_CLIENT))
   (expect-true "client trusts the fixture CA"
                (tls-context-load-verify-locations ?cctx "tests/fixtures/ca.pem"))

   (bind ?srv (create-socket AF_INET SOCK_DGRAM))
   (setsockopt ?srv SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?srv 127.0.0.1 ?*sport*)

   (bind ?cli (create-socket AF_INET SOCK_DGRAM))
   (setsockopt ?cli SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?cli 127.0.0.1 ?*cport*)
   (connect ?cli 127.0.0.1 ?*sport*)

   (fcntl-add-status-flags ?srv O_NONBLOCK)
   (fcntl-add-status-flags ?cli O_NONBLOCK)

   ;;=================================================================
   ;; The client speaks first, and the server eats it
   ;;=================================================================
   (dtls-connect ?cctx ?cli localhost)

   (expect-true "the first flight arrives at the server"
                (poll ?srv 5000 POLLIN))

   ;; The code reads from the plaintext socket, before a session exists. It
   ;; discards the bytes, and for the client this datagram was lost.
   (bind ?lost (rcvfrom ?srv))
   (expect-true "the first flight is taken off the socket and discarded"
                (neq ?lost FALSE))
   (expect-false "nothing of it is left for the handshake to find"
                 (poll ?srv 200 POLLIN))

   ;;=================================================================
   ;; Recovery
   ;;=================================================================
   ;; This budget is long enough for a retransmission timer to end. One
   ;; second is the usual first interval, and this budget is several times
   ;; that value.
   (bind ?outcome (drive-counting ?cctx ?sctx ?cli ?srv 600))

   (expect-true "the handshake completes despite the loss" (nth$ 1 ?outcome))
   (expect-true "the session is usable afterwards" (tls-version ?cli))

   (tls-shutdown ?cli)
   (close-connection ?cli)
   (close-connection ?srv)
   (tls-free-context ?cctx)
   (tls-free-context ?sctx))

(run-tests)
(test-summary)
