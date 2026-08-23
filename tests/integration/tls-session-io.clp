;;; What a TLS session does with the buffers, the pushback and the end of
;;; input.
;;;
;;; requires: tls
;;;
;;; tests/integration/tls-loopback.clp covers the start of a session. This test
;;; starts from a session that operates and checks the parts that a plaintext
;;; socket gets from stdio. Those parts are the three buffer modes, the
;;; one-character pushback behind unget-char, the buffered bytes that poll(2)
;;; cannot see, and the result of a read after the peer goes away.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(load* "tests/lib/tls.clp")
(test-suite "tls-session-io")
(test-plan 32)

(defglobal ?*port* = 18925)
(defglobal ?*ca*   = "tests/fixtures/ca.pem")
(defglobal ?*cert* = "tests/fixtures/server.pem")
(defglobal ?*key*  = "tests/fixtures/server-key.pem")

(deffunction run-tests ()
   (bind ?sctx (tls-create-context TLS_SERVER))
   (tls-context-use-certificate-file ?sctx ?*cert*)
   (tls-context-use-private-key-file ?sctx ?*key*)

   (bind ?cctx (tls-create-context TLS_CLIENT))
   (tls-context-load-verify-locations ?cctx ?*ca*)

   (bind ?pair (tcp-connected-pair ?*port*))
   (bind ?srv (nth$ 1 ?pair))
   (bind ?cli (nth$ 2 ?pair))
   (bind ?sfd (nth$ 3 ?pair))

   (fcntl-add-status-flags ?cli O_NONBLOCK)
   (fcntl-add-status-flags ?sfd O_NONBLOCK)
   (tls-connect ?cctx ?cli localhost)
   (tls-accept ?sctx ?sfd)
   (expect-true "session established" (tls-drive-handshake ?cli ?sfd 200))
   (fcntl-remove-status-flags ?cli O_NONBLOCK)
   (fcntl-remove-status-flags ?sfd O_NONBLOCK)

   (bind ?cname (get-socket-logical-name ?cli))
   (bind ?sname (get-socket-logical-name ?sfd))

   ;; A second call on a complete handshake does nothing. It is not an
   ;; error.
   (expect-true "handshake is idempotent once complete" (tls-handshake ?cli))

   ;;=================================================================
   ;; Fully buffered, which is the default
   ;;=================================================================
   (expect-eq "a fresh session holds nothing" 0 (get-retained-bytes ?cli))
   (expect-eq "and has no retained limit" 0 (get-retained-limit ?cli))

   (printout ?cname "part1")
   (expect-false "a full buffer holds its bytes back" (poll ?sfd 200 POLLIN))

   ;; The bytes are in the write buffer of the session, and get-retained-bytes
   ;; reports that buffer. A plaintext socket gives 0 here, because its bytes
   ;; are in the buffer of the C library and this function cannot see that
   ;; buffer. The two agree after the peer refuses data, and that condition is
   ;; the purpose of the count. Before that point they differ, and the README
   ;; says so.
   (expect-eq "which get-retained-bytes counts" 5 (get-retained-bytes ?cli))

   (printout ?cname " part2" crlf)
   (expect-true "flush releases them" (flush-connection ?cli))
   (expect-eq "leaving nothing retained" 0 (get-retained-bytes ?cli))
   (expect-true "server sees the flushed write" (poll ?sfd 5000 POLLIN))
   (expect-eq "both writes arrive as one line" "part1 part2" (readline ?sname))

   ;;=================================================================
   ;; Line buffered: a newline is enough, no flush needed
   ;;=================================================================
   (expect-true "line buffering can be set" (set-line-buffered ?cli))
   (printout ?cname "a line" crlf)
   (expect-true "the newline sent it" (poll ?sfd 5000 POLLIN))
   (expect-eq "line arrives without an explicit flush" "a line" (readline ?sname))

   ;;=================================================================
   ;; Not buffered: every write goes out, newline or not
   ;;=================================================================
   (expect-true "buffering can be turned off" (set-not-buffered ?cli))
   (printout ?cname "xy")
   (expect-true "an unterminated write still went" (poll ?sfd 5000 POLLIN))
   (expect-eq "first character" 120 (get-char ?sname))
   (expect-eq "second character" 121 (get-char ?sname))

   (expect-true "full buffering can be restored" (set-fully-buffered ?cli))

   ;;=================================================================
   ;; Pushback, and bytes poll(2) cannot see
   ;;=================================================================
   (printout ?sname "hello" crlf)
   (flush-connection ?sfd)
   (expect-true "client sees the server's line" (poll ?cli 5000 POLLIN))

   ;; One read decrypts the full record. As a result, the remainder of the
   ;; line is now in this process, and the descriptor knows nothing about
   ;; it.
   (expect-eq "one character off the front" 104 (get-char ?cname))
   (expect-gte "the rest of the line is buffered here" 1 (tls-pending ?cli))
   (expect-true "poll reports the buffered bytes without waiting"
                (poll ?cli 0 POLLIN))
   ;; This is the same question in a different form. With no flags, poll
   ;; watches for each condition, and the buffered input must still count.
   (expect-true "poll with no flags named sees them too" (poll ?cli 0))

   (expect-eq "the character goes back" 104 (unget-char ?cname 104))
   (expect-eq "and the whole line reads normally" "hello" (readline ?cname))

   ;;=================================================================
   ;; empty-connection
   ;;=================================================================
   ;; This function reads until there is no more data. As a result, it needs
   ;; a socket that reports that condition and does not wait. On a blocking
   ;; socket it would stay in the last read. The plaintext code does the
   ;; same.
   (printout ?sname "discard me" crlf)
   (flush-connection ?sfd)
   (poll ?cli 5000 POLLIN)
   (fcntl-add-status-flags ?cli O_NONBLOCK)
   (expect-true "connection empties" (empty-connection ?cli))
   (expect-eq "nothing is left buffered" 0 (tls-pending ?cli))
   (fcntl-remove-status-flags ?cli O_NONBLOCK)

   ;;=================================================================
   ;; The peer goes away
   ;;=================================================================
   (expect-true "server closes its end" (close-connection ?sfd))
   (expect-eq "client reads end of input" EOF (readline ?cname))

   ;;=================================================================
   ;; Teardown
   ;;=================================================================
   (expect-true "client shuts down" (tls-shutdown ?cli))
   (expect-true "shutting down twice is a no-op" (tls-shutdown ?cli))
   (expect-true "client closes" (close-connection ?cli))
   (close-connection ?srv)
   (tls-free-context ?cctx)
   (tls-free-context ?sctx))

(run-tests)
(test-summary)
