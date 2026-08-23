;;; Messages that are much larger than one TLS record.
;;;
;;; requires: tls
;;;
;;; large-payloads.clp has the plain-socket half of this test. A second file is
;;; necessary for more than one cause. The tls-* functions must be absent from
;;; a TLS=0 build, and TLS also does not carry a byte stream in the manner of a
;;; socket. TLS carries records. A record holds a maximum of 16 KiB, whatever
;;; the caller asks for. As a result, a longer write is several records. The
;;; library frames, encrypts and authenticates each record separately, and the
;;; reader must have a full record before it can give the first byte of it.
;;;
;;; As a result, the record limits fall at positions where a plain socket has
;;; none: in the middle of a line, and in the middle of the buffer that serves
;;; a read. A message of 64 KiB passes three limits or more.
;;;
;;; The two ends are in this process. As a result, the sizes stay below the
;;; socket buffer, for the cause in large-payloads.clp.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(load* "tests/lib/tls.clp")
(test-suite "tls-large-payloads")
(test-plan 22)

(defglobal ?*port* = 19321)
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
   (expect-true "server certificate loads"
                (tls-context-use-certificate-file ?sctx ?*cert*))
   (expect-true "server key loads"
                (tls-context-use-private-key-file ?sctx ?*key*))

   (bind ?cctx (tls-create-context TLS_CLIENT))
   (expect-true "client trusts the fixture CA"
                (tls-context-load-verify-locations ?cctx ?*ca*))
   (expect-true "client requires a verified peer"
                (tls-context-set-verify ?cctx SSL_VERIFY_PEER))

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
   (expect-true "peer certificate verified" (tls-verify-result ?cli))

   (bind ?cname (get-socket-logical-name ?cli))
   (bind ?sname (get-socket-logical-name ?sfd))

   ;;=================================================================
   ;; One write, several records
   ;;=================================================================
   (bind ?big (payload 10))
   (expect-eq "the payload is 64 KiB" 65536 (str-length ?big))

   (printout ?cname ?big crlf)
   (expect-true "a 64 KiB encrypted write flushes" (flush-connection ?cli))

   (bind ?got (readline ?sname))
   (expect-eq "the whole payload arrives as one line" 65536 (str-length ?got))
   ;; This is the important check. Incorrect code at a record limit on either
   ;; side gives the correct number of bytes much more often than it gives the
   ;; correct bytes. Examples of such code are a partial record that the code
   ;; reads as complete, a length from the incorrect field, and a buffer that
   ;; the code uses again before it reads the buffer.
   (expect-eq "and decrypts byte for byte" ?big ?got)

   ;; Server to client. This uses the write path and the record layer of the
   ;; other end.
   (printout ?sname ?big crlf)
   (expect-true "the server flushes 64 KiB" (flush-connection ?sfd))
   (bind ?back (readline ?cname))
   (expect-eq "which arrives whole" 65536 (str-length ?back))
   (expect-eq "and unaltered" ?big ?back)

   ;;=================================================================
   ;; Repeated writes over one session
   ;;=================================================================
   ;; Each of these writes is a new record on a session whose sequence
   ;; numbers already advanced a long way. A sequence number that stops to
   ;; increase, or a nonce that the code uses again, fails here and not
   ;; above.
   (bind ?chunk (payload 8))
   (expect-eq "each chunk is 16 KiB" 16384 (str-length ?chunk))

   (bind ?bad 0)
   (bind ?moved 0)
   (bind ?i 0)
   (while (< ?i 20) do
      (printout ?cname ?chunk crlf)
      (flush-connection ?cli)
      (bind ?r (readline ?sname))
      (if (neq ?r ?chunk) then (bind ?bad (+ ?bad 1)))
      (bind ?moved (+ ?moved (str-length ?r)))
      (bind ?i (+ ?i 1)))

   (expect-eq "every round decrypts unaltered" 0 ?bad)
   (expect-eq "320 KiB moved over one session" 327680 ?moved)

   ;;=================================================================
   ;; Reading across a record boundary one character at a time
   ;;=================================================================
   ;; tls-pending is the number of bytes that the record layer decrypted and
   ;; did not give out. A caller must read that count before it polls. The
   ;; socket can have no data to read while a full decrypted record is in the
   ;; library, and a loop that uses only poll stops and holds the answer.
   (printout ?sname ?chunk crlf)
   (flush-connection ?sfd)

   (bind ?first (get-char ?cname))
   (expect-eq "the first character arrives" 48 ?first)
   (expect-gte "and the rest of the record is pending in the library" 1
               (tls-pending ?cli))

   (bind ?n 1)
   (bind ?c (get-char ?cname))
   (while (and (neq ?c -1) (neq ?c 10)) do
      (bind ?n (+ ?n 1))
      (bind ?c (get-char ?cname)))
   (expect-eq "every byte of the record is readable by character" 16384 ?n)
   (expect-eq "and the line ends where it was ended" 10 ?c)
   (expect-eq "with nothing left pending" 0 (tls-pending ?cli))

   ;; The session still operates correctly after all of that data.
   (printout ?cname "done" crlf)
   (flush-connection ?cli)
   (expect-eq "a short line still crosses the session" "done" (readline ?sname))

   (tls-shutdown ?cli)
   (close-connection ?cli)
   (close-connection ?sfd)
   (close-connection ?srv)
   (tls-free-context ?cctx)
   (tls-free-context ?sctx))

(run-tests)
(test-summary)
