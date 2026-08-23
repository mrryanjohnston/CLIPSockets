;;; printout, readline and get-char on a DTLS session.
;;;
;;; requires: tls
;;; requires: dtls
;;;
;;; A plaintext datagram socket already replies to printout and readline
;;; through its logical name. examples/server-udp.bat needs that, and
;;; tests/integration/udp-router-io.clp gives its rules. As a result, a
;;; handshake must not remove the capability, and this file checks that.
;;;
;;; A DTLS session also gets each problem of a character stream on datagrams.
;;; The checks below are the same checks that udp-router-io.clp makes of the
;;; plaintext path, and that is on purpose. A record has no end mark, and
;;; readline joins records and does not show the join. DTLS changes neither of
;;; these.
;;;
;;; There is one difference. A record limit is real and available here.
;;; dtls-send and dtls-recv each move one record. As a result, a program that
;;; needs the limits has a function for them. The two interfaces share one
;;; session, and the last check here is the rule that stops them from losing
;;; bytes to each other.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(load* "tests/lib/tls.clp")
(test-suite "dtls-router-io")
(test-plan 16)

(defglobal ?*sport* = 18975)
(defglobal ?*cport* = 18976)

(deffunction run-tests ()
   (bind ?sctx (tls-create-context DTLS_SERVER))
   (tls-context-use-certificate-file ?sctx "tests/fixtures/server.pem")
   (tls-context-use-private-key-file ?sctx "tests/fixtures/server-key.pem")
   (bind ?cctx (tls-create-context DTLS_CLIENT))
   (tls-context-load-verify-locations ?cctx "tests/fixtures/ca.pem")

   (bind ?srv (create-socket AF_INET SOCK_DGRAM))
   (setsockopt ?srv SOL_SOCKET SO_REUSEADDR 1)
   (bind ?sname (bind-socket ?srv 127.0.0.1 ?*sport*))
   (bind ?cli (create-socket AF_INET SOCK_DGRAM))
   (setsockopt ?cli SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?cli 127.0.0.1 ?*cport*)
   (bind ?cname (connect ?cli 127.0.0.1 ?*sport*))

   (fcntl-add-status-flags ?srv O_NONBLOCK)
   (fcntl-add-status-flags ?cli O_NONBLOCK)

   (expect-true "handshake completes"
                (dtls-drive-handshake ?cctx ?sctx ?cli ?srv 200))

   ;;=================================================================
   ;; The logical name still works
   ;;=================================================================
   ;; The name did not change during the handshake. It is the name that
   ;; bind-socket and connect gave. It now reaches an encrypted session and
   ;; not a plain socket, and a program does not need to know that.
   (expect-true "buffering can still be set on an encrypted datagram socket"
                (set-line-buffered ?cli))

   (printout ?cname "hello dtls" crlf)
   (flush-connection ?cli)

   (expect-true "the line arrives" (poll ?srv 5000 POLLIN))
   (expect-eq "readline returns what printout wrote" "hello dtls"
              (readline ?sname))

   ;;=================================================================
   ;; One line, one record
   ;;=================================================================
   ;; Line buffering is the default for a datagram session, and this check
   ;; shows the cause. A line through the router goes out as exactly one
   ;; record. As a result, the far end can use the record limits.
   (expect-false "nothing follows the line" (poll ?srv 200 POLLIN))

   ;;=================================================================
   ;; A record has no end, exactly as in the plaintext case
   ;;=================================================================
   (expect-eq "a record is sent" 2 (dtls-send ?cli "AB"))
   (expect-true "it arrives" (poll ?srv 5000 POLLIN))
   (expect-eq "first character" 65 (get-char ?sname))
   (expect-eq "second character" 66 (get-char ?sname))
   (expect-eq "reading past the record gives no character" -1 (get-char ?sname))

   ;;=================================================================
   ;; readline joins records
   ;;=================================================================
   (dtls-send ?cli "part1")
   (dtls-send ?cli "part2
")
   (expect-true "both records arrive" (poll ?srv 5000 POLLIN))
   (expect-eq "a record without a newline is continued by the next one"
              "part1part2" (readline ?sname))

   ;;=================================================================
   ;; The two interfaces do not step on each other
   ;;=================================================================
   ;; The code takes one character of this record through the router. The
   ;; remainder then belongs to the code that reads the stream. dtls-recv
   ;; would have to pass those bytes to give the next record, and it refuses
   ;; instead. This check prevents a silent loss of those bytes.
   (dtls-send ?cli "abc")
   (expect-true "the record arrives" (poll ?srv 5000 POLLIN))
   (expect-eq "one character is read through the router" 97 (get-char ?sname))
   (expect-false "dtls-recv refuses while characters are unread"
                 (dtls-recv ?srv))

   ;; A read of the data that the router holds gives the session back to
   ;; dtls-recv.
   (empty-connection ?srv)
   (dtls-send ?cli "whole")
   (poll ?srv 5000 POLLIN)
   (bind ?mf (dtls-recv ?srv))
   (expect-eq "dtls-recv works once the stream side is drained" "whole"
              (nth$ 2 ?mf))

   (tls-shutdown ?cli)
   (close-connection ?cli)
   (close-connection ?srv)
   (tls-free-context ?cctx)
   (tls-free-context ?sctx))

(run-tests)
(test-summary)
