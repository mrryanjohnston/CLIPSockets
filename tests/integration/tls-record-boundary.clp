;;; Where one TLS record ends and the next one starts.
;;;
;;; requires: tls
;;;
;;; tls-large-payloads.clp sends much more than one record and checks that the
;;; data at the other end is the data that went in. This file is about the
;;; limit between two records. A TLS record is the one place where the buffer
;;; of the reader is not the full picture.
;;;
;;; The library decrypts a record completely or not at all. As a result, bytes
;;; are in three locations and not in two: in the kernel, decrypted inside the
;;; TLS library, and in the read buffer above the library. poll(2) can see only
;;; the first location. A caller that polls the descriptor and finds no data
;;; can hold a complete record that it decrypted and did not read. A loop with
;;; that shape waits for data that it already has.
;;;
;;; Here (poll) answers for the library and also for the kernel, and
;;; (tls-pending) asks the same question directly. Those two statements are the
;;; purpose of this file, and one read of one line cannot check either of
;;; them.
;;;
;;; The code sets read timeouts before the reads that depend on those
;;; statements. If a backend holds bytes and reports nothing, the read stops
;;; and a check fails with a message about the problem. Without the timeout the
;;; read would wait until the runner stops the file.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(load* "tests/lib/tls.clp")
(test-suite "tls-record-boundary")
(test-plan 25)

(defglobal ?*port* = 19401)
(defglobal ?*ca*   = "tests/fixtures/ca.pem")
(defglobal ?*cert* = "tests/fixtures/server.pem")
(defglobal ?*key*  = "tests/fixtures/server-key.pem")

(deffunction payload (?doublings)
   (bind ?s "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/")
   (bind ?i 0)
   (while (< ?i ?doublings) do
      (bind ?s (str-cat ?s ?s))
      (bind ?i (+ ?i 1)))
   (return ?s))

(deffunction run-tests ()
   ;;=================================================================
   ;; A verified session on loopback
   ;;=================================================================
   (bind ?sctx (tls-create-context TLS_SERVER))
   (tls-context-use-certificate-file ?sctx ?*cert*)
   (tls-context-use-private-key-file ?sctx ?*key*)
   (bind ?cctx (tls-create-context TLS_CLIENT))
   (tls-context-load-verify-locations ?cctx ?*ca*)
   (tls-context-set-verify ?cctx SSL_VERIFY_PEER)

   (bind ?pair (tcp-connected-pair ?*port*))
   (bind ?srv (nth$ 1 ?pair))
   (bind ?cli (nth$ 2 ?pair))
   (bind ?sfd (nth$ 3 ?pair))

   (fcntl-add-status-flags ?cli O_NONBLOCK)
   (fcntl-add-status-flags ?sfd O_NONBLOCK)
   (tls-connect ?cctx ?cli localhost)
   (tls-accept ?sctx ?sfd)
   (expect-true "handshake completes" (tls-drive-handshake ?cli ?sfd 200))
   (fcntl-remove-status-flags ?cli O_NONBLOCK)
   (fcntl-remove-status-flags ?sfd O_NONBLOCK)

   (bind ?cname (get-socket-logical-name ?cli))
   (bind ?sname (get-socket-logical-name ?sfd))

   ;; Each read below has a time limit, for the cause in the header.
   (set-timeout ?cli 2 0)

   ;;=================================================================
   ;; Nothing is decrypted until it is asked for
   ;;=================================================================
   ;; Two lines in one write. As a result, they go out in one record, and the
   ;; limit is after the second line and not between them.
   (printout ?sname "first line" crlf "second line" crlf)
   (flush-connection ?sfd)

   (expect-true "the arriving record makes the socket readable"
                (poll ?cli 2000 POLLIN))
   ;; The record is in the kernel, and no code used it. A library that
   ;; decrypted the record on arrival would report bytes here, and no code
   ;; asked it for data.
   (expect-eq "and nothing is decrypted yet" 0 (tls-pending ?cli))

   ;;=================================================================
   ;; Reading part of a record leaves the rest inside the library
   ;;=================================================================
   (expect-eq "the first line reads back" "first line" (readline ?cname))

   ;; The library decrypted the full record for that read. As a result, the
   ;; remainder is now in this process. poll(2) cannot see this condition.
   (expect-gte "the rest of the record is now held here"
               1 (tls-pending ?cli))

   ;; The full design depends on this statement. The kernel has no data left.
   ;; If poll answered only for the descriptor, it would report an idle
   ;; session while a complete decrypted line waits one call away.
   (expect-true "and poll reports it even though the kernel is empty"
                (poll ?cli 0 POLLIN))

   (expect-eq "the second line reads back from the buffer"
              "second line" (readline ?cname))
   (expect-eq "after which nothing is held" 0 (tls-pending ?cli))
   (expect-false "and the session is genuinely idle" (poll ?cli 0 POLLIN))

   ;;=================================================================
   ;; A record read one byte at a time
   ;;=================================================================
   ;; The count must go down by exactly one for each character. A count of the
   ;; full record, or a count from the socket, would stay the same or would
   ;; change by a large step.
   (printout ?sname "abcdefghij" crlf)
   (flush-connection ?sfd)

   (expect-eq "the first character arrives" 97 (get-char ?cname))
   (bind ?held (tls-pending ?cli))
   (expect-gte "with the rest of the record held" 1 ?held)

   (bind ?wrong 0)
   (bind ?i 0)
   (while (< ?i 9) do
      (get-char ?cname)
      (bind ?now (tls-pending ?cli))
      (if (neq ?now (- ?held 1)) then (bind ?wrong (+ ?wrong 1)))
      (bind ?held ?now)
      (bind ?i (+ ?i 1)))
   (expect-eq "every character takes exactly one byte off the count"
              0 ?wrong)

   (expect-eq "the newline is the last of the record" 10 (get-char ?cname))
   (expect-eq "and the record is spent" 0 (tls-pending ?cli))

   ;;=================================================================
   ;; Reading by character across a boundary
   ;;=================================================================
   ;; This is 32 KiB with one newline at the end. As a result, no read can
   ;; stop at a line, and the reader passes one record limit or more in the
   ;; middle of the data.
   ;;
   ;; The test counts each limit and does not use a fixed number. The count
   ;; goes to zero when the held bytes end, and it becomes positive again when
   ;; the library decrypts the next record. That change is the limit. The
   ;; count makes this a test of the limit and not a test of 32 KiB of data.
   ;; One record would give a count of zero and would still deliver each byte.
   (bind ?big (payload 9))
   (expect-eq "the payload is 32 KiB" 32768 (str-length ?big))

   (printout ?sname ?big crlf)
   (flush-connection ?sfd)

   (bind ?n 0)
   (bind ?crossings 0)
   (bind ?was 0)
   (bind ?c (get-char ?cname))
   (while (and (neq ?c -1) (neq ?c 10)) do
      (bind ?n (+ ?n 1))
      (bind ?now (tls-pending ?cli))
      ;; The count was zero before and is positive now. The library took
      ;; another record to give this character.
      (if (and (= ?was 0) (> ?now 0)) then
         (bind ?crossings (+ ?crossings 1)))
      (bind ?was ?now)
      (bind ?c (get-char ?cname)))

   (expect-eq "every byte of the payload is readable by character"
              32768 ?n)
   (expect-eq "and the line ends where it was ended" 10 ?c)
   (expect-gte "more than one record was crossed on the way" 1 ?crossings)
   (expect-eq "with nothing left over" 0 (tls-pending ?cli))
   (expect-false "and nothing left on the socket" (poll ?cli 0 POLLIN))

   ;;=================================================================
   ;; A boundary inside a line
   ;;=================================================================
   ;; The same data, read as a line and not one character at a time. readline
   ;; must join the records itself, because the newline is several records
   ;; after the first record that the library decrypted.
   (printout ?sname ?big crlf)
   (flush-connection ?sfd)
   (bind ?joined (readline ?cname))
   (expect-eq "a line spanning several records comes back whole"
              32768 (str-length ?joined))
   (expect-eq "and byte for byte" ?big ?joined)
   (expect-eq "leaving nothing held" 0 (tls-pending ?cli))

   ;; The session is still a usual session after this.
   (printout ?cname "done" crlf)
   (flush-connection ?cli)
   (expect-eq "and a short line still crosses" "done" (readline ?sname))

   ;;=================================================================
   ;; What pending means on a socket without a session
   ;;=================================================================
   ;; The answer is FALSE and not 0. A plaintext socket is not a session with
   ;; no data. A caller that treated the two the same would poll a plaintext
   ;; socket as if the library could hold its data.
   (bind ?plain (create-socket AF_INET SOCK_STREAM))
   (expect-false "a socket with no session has no pending count"
                 (tls-pending ?plain))
   (close-connection ?plain)

   (tls-shutdown ?cli)
   (close-connection ?cli)
   (close-connection ?sfd)
   (close-connection ?srv)
   (tls-free-context ?cctx)
   (tls-free-context ?sctx))

(run-tests)
(test-summary)
