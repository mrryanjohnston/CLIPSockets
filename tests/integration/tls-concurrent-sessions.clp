;;; Several TLS sessions that are open at the same time in one process.
;;;
;;; requires: tls
;;;
;;; concurrent-connections.clp makes this test for plain sockets. There, only
;;; the descriptor of a name keeps two connections apart. A TLS session has
;;; much more behind its name: its own keys, its own sequence numbers, and its
;;; own record buffer with bytes that the library decrypted and no code read.
;;; If two sessions share one of these items, a peer gets the plaintext of a
;;; different peer. A test of one session at a time cannot see that error.
;;;
;;; The six sockets do their handshakes at the same time and not one pair at a
;;; time. A real server has that condition, and it is the condition where a
;;; shared buffer is most likely to be in use.
;;;
;;; One client context serves the three clients and one server context serves
;;; the three servers, and that is on purpose. A program sets a context one
;;; time and uses it for each connection, and a real server does the same. As a
;;; result, a field that belongs to the context but must belong to the session
;;; fails this test.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(load* "tests/lib/tls.clp")
(test-suite "tls-concurrent-sessions")
(test-plan 32)

(defglobal ?*port* = 19331)
(defglobal ?*ca*   = "tests/fixtures/ca.pem")
(defglobal ?*cert* = "tests/fixtures/server.pem")
(defglobal ?*key*  = "tests/fixtures/server-key.pem")

;;; Runs any number of incomplete handshakes at the same time.
;;;
;;; The function runs each given socket in each cycle until that socket reports
;;; that it is complete. It does not call a socket that is complete. A backend
;;; can read a second handshake call on a session as a request to negotiate
;;; again, and this file does not test that behaviour.
(deffunction drive-many (?limit $?fds)
   (bind ?n (length$ ?fds))
   (bind ?done (create$))
   (bind ?k 1)
   (while (<= ?k ?n) do
      (bind ?done (create$ ?done FALSE))
      (bind ?k (+ ?k 1)))

   (bind ?i 0)
   (while (< ?i ?limit) do
      (bind ?left 0)
      (bind ?k 1)
      (while (<= ?k ?n) do
         (if (eq (nth$ ?k ?done) FALSE) then
            (if (tls-handshake (nth$ ?k ?fds))
               then (bind ?done (replace$ ?done ?k ?k TRUE))
               else (bind ?left (+ ?left 1))))
         (bind ?k (+ ?k 1)))
      (if (= ?left 0) then (return TRUE))

      (bind ?k 1)
      (while (<= ?k ?n) do
         (if (eq (nth$ ?k ?done) FALSE) then (poll (nth$ ?k ?fds) 20 POLLIN))
         (bind ?k (+ ?k 1)))
      (bind ?i (+ ?i 1)))
   (return FALSE))

(deffunction run-tests ()
   ;;=================================================================
   ;; Two contexts, six sockets
   ;;=================================================================
   (bind ?sctx (tls-create-context TLS_SERVER))
   (expect-true "server certificate loads"
                (tls-context-use-certificate-file ?sctx ?*cert*))
   (expect-true "server key loads"
                (tls-context-use-private-key-file ?sctx ?*key*))

   (bind ?cctx (tls-create-context TLS_CLIENT))
   (expect-true "client trusts the fixture CA"
                (tls-context-load-verify-locations ?cctx ?*ca*))
   (expect-true "client requires a verified peer"
                (tls-context-set-verify ?cctx SSL_VERIFY_PEER))

   (bind ?srv (create-socket AF_INET SOCK_STREAM))
   (setsockopt ?srv SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?srv 127.0.0.1 ?*port*)
   (listen ?srv)

   ;; Each client connects before the code accepts any of them. As a result,
   ;; the three connections wait in the backlog together.
   (bind ?c1 (create-socket AF_INET SOCK_STREAM))
   (connect ?c1 127.0.0.1 ?*port*)
   (bind ?c2 (create-socket AF_INET SOCK_STREAM))
   (connect ?c2 127.0.0.1 ?*port*)
   (bind ?c3 (create-socket AF_INET SOCK_STREAM))
   (connect ?c3 127.0.0.1 ?*port*)

   (bind ?s1 (accept ?srv))
   (bind ?s2 (accept ?srv))
   (bind ?s3 (accept ?srv))

   (fcntl-add-status-flags ?c1 O_NONBLOCK)
   (fcntl-add-status-flags ?c2 O_NONBLOCK)
   (fcntl-add-status-flags ?c3 O_NONBLOCK)
   (fcntl-add-status-flags ?s1 O_NONBLOCK)
   (fcntl-add-status-flags ?s2 O_NONBLOCK)
   (fcntl-add-status-flags ?s3 O_NONBLOCK)

   ;;=================================================================
   ;; Three handshakes at once, off one context each way
   ;;=================================================================
   (tls-connect ?cctx ?c1 localhost)
   (tls-connect ?cctx ?c2 localhost)
   (tls-connect ?cctx ?c3 localhost)
   (tls-accept ?sctx ?s1)
   (tls-accept ?sctx ?s2)
   (tls-accept ?sctx ?s3)

   (expect-true "all three handshakes complete together"
                (drive-many 400 ?c1 ?s1 ?c2 ?s2 ?c3 ?s3))

   (fcntl-remove-status-flags ?c1 O_NONBLOCK)
   (fcntl-remove-status-flags ?c2 O_NONBLOCK)
   (fcntl-remove-status-flags ?c3 O_NONBLOCK)
   (fcntl-remove-status-flags ?s1 O_NONBLOCK)
   (fcntl-remove-status-flags ?s2 O_NONBLOCK)
   (fcntl-remove-status-flags ?s3 O_NONBLOCK)

   ;; One context verified three peers. It did not verify one peer three
   ;; times.
   (expect-true "session 1 verified its peer" (tls-verify-result ?c1))
   (expect-true "session 2 verified its peer" (tls-verify-result ?c2))
   (expect-true "session 3 verified its peer" (tls-verify-result ?c3))

   ;; Each session reports the parameters that it agreed on.
   (expect-true "session 1 negotiated a cipher" (tls-cipher ?c1))
   (expect-true "session 2 negotiated a cipher" (tls-cipher ?c2))
   (expect-true "session 3 negotiated a cipher" (tls-cipher ?c3))

   (bind ?n1 (get-socket-logical-name ?c1))
   (bind ?n2 (get-socket-logical-name ?c2))
   (bind ?n3 (get-socket-logical-name ?c3))
   (bind ?m1 (get-socket-logical-name ?s1))
   (bind ?m2 (get-socket-logical-name ?s2))
   (bind ?m3 (get-socket-logical-name ?s3))

   (expect-neq "sessions 1 and 2 have different names" ?n1 ?n2)
   (expect-neq "sessions 2 and 3 have different names" ?n2 ?n3)
   (expect-neq "sessions 1 and 3 have different names" ?n1 ?n3)

   ;;=================================================================
   ;; Traffic goes to the session it was addressed to
   ;;=================================================================
   ;; The code writes in a mixed sequence, and each message is different. As a
   ;; result, a session that gives the buffer of a different session gives an
   ;; incorrect answer, and the test can see that.
   (printout ?n2 "message for session two" crlf)
   (printout ?n3 "message for session three" crlf)
   (printout ?n1 "message for session one" crlf)
   (flush-connection ?c2)
   (flush-connection ?c3)
   (flush-connection ?c1)

   ;; The code reads them back in a third sequence.
   (expect-eq "session 3 reads only its own message"
              "message for session three" (readline ?m3))
   (expect-eq "session 1 reads only its own message"
              "message for session one" (readline ?m1))
   (expect-eq "session 2 reads only its own message"
              "message for session two" (readline ?m2))

   ;; The same test in the other direction.
   (printout ?m1 "reply to one" crlf)
   (printout ?m3 "reply to three" crlf)
   (printout ?m2 "reply to two" crlf)
   (flush-connection ?s1)
   (flush-connection ?s3)
   (flush-connection ?s2)

   (expect-eq "session 2 gets its own reply" "reply to two"   (readline ?n2))
   (expect-eq "session 1 gets its own reply" "reply to one"   (readline ?n1))
   (expect-eq "session 3 gets its own reply" "reply to three" (readline ?n3))

   ;;=================================================================
   ;; Buffered plaintext is per session
   ;;=================================================================
   ;; Two lines in one record leave the second line in the library after the
   ;; code reads the first line. Only that session can hold data. A record
   ;; buffer that two sessions share shows here as a count on a session that
   ;; got no data.
   (printout ?m1 "first line" crlf "second line" crlf)
   (flush-connection ?s1)

   (expect-eq "session 1 reads the first of two lines"
              "first line" (readline ?n1))
   (expect-gte "session 1 is holding the rest" 1 (tls-pending ?c1))
   (expect-eq "session 2 is holding nothing" 0 (tls-pending ?c2))
   (expect-eq "session 3 is holding nothing" 0 (tls-pending ?c3))
   (expect-eq "session 1 reads the second line"
              "second line" (readline ?n1))
   (expect-eq "and is then holding nothing" 0 (tls-pending ?c1))

   ;;=================================================================
   ;; Closing one session leaves the others alone
   ;;=================================================================
   (expect-true "session 2 shuts down" (tls-shutdown ?c2))
   (expect-true "session 2 closes" (close-connection ?c2))
   (close-connection ?s2)

   (printout ?n1 "still working" crlf)
   (flush-connection ?c1)
   (expect-eq "session 1 still carries traffic after session 2 closed"
              "still working" (readline ?m1))

   (printout ?n3 "also still working" crlf)
   (flush-connection ?c3)
   (expect-eq "session 3 still carries traffic after session 2 closed"
              "also still working" (readline ?m3))

   (expect-true "session 1 is still verified" (tls-verify-result ?c1))
   (expect-true "session 3 is still verified" (tls-verify-result ?c3))

   (tls-shutdown ?c1)
   (tls-shutdown ?c3)
   (close-connection ?c1)
   (close-connection ?c3)
   (close-connection ?s1)
   (close-connection ?s3)
   (close-connection ?srv)
   (tls-free-context ?cctx)
   (tls-free-context ?sctx))

(run-tests)
(test-summary)
