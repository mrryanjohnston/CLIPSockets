;;; An encrypted socket that accepts no more data.
;;;
;;; requires: tls
;;;
;;; write-backpressure.clp asks this question of a plaintext socket and gives
;;; the necessary answer. Its header says why the two must agree: a caller must
;;; not have to write different code for an encrypted socket. As a result, the
;;; rules here are the rules of that file, asked again after a handshake:
;;;
;;;   it discards no accepted data  the bytes that printout took are still
;;;                                 available to send when the socket is ready
;;;   it can report the failure     flush-connection gives FALSE while the
;;;                                 data is not out
;;;   a limit bounds the memory     with set-retained-limit set, the data that
;;;                                 the code keeps stays inside the limit and
;;;                                 the code discards the data above it
;;;   it recovers completely        after the peer reads, a later flush
;;;                                 succeeds and each byte arrives in sequence
;;;
;;; None of this is a new rule. The README already says of flush-connection
;;; that "the library keeps the data for a write of any size ... it discards no
;;; data that it accepted". It then says that "if the socket has a TLS session
;;; ... the rules in this section are also correct for that socket".
;;;
;;; The two ends are in this one process, and the writer must be non-blocking.
;;; A blocking writer that fills the buffer waits for a reader in the same
;;; thread, and the test never returns. The plaintext file gives the same
;;; cause, and it is also why a real server that writes replies of a size that
;;; it does not control needs O_NONBLOCK.
;;;
;;; The test makes the send and receive buffers small with setsockopt. It does
;;; not fill large buffers. The kernel tunes a loopback TCP connection, and that
;;; connection takes some megabytes before it refuses data. The code would keep
;;; each of those bytes, and the interpreter would then read them back one
;;; character at a time. This test is about what happens after a refusal. It is
;;; not about how much data the kernel needs before it refuses.
;;;
;;; This file does not cover datagram sessions, and that is on purpose. A DTLS
;;; record goes out complete or not at all, and the code cannot send one half
;;; of a record. As a result, a datagram session holds exactly one record. To
;;; keep more data would mean a queue of complete records. That is a different
;;; design, and set-retained-limit does not describe it. dtls-send reports its
;;; own refusal instead.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(load* "tests/lib/tls.clp")
(test-suite "tls-write-backpressure")
(test-plan 22)

(defglobal ?*port* = 19451)

;;; This size is small enough that a few chunks fill the connection. It is also
;;; above the minimum that the kernel applies. Linux multiplies the requested
;;; size by two and refuses a size below approximately two kilobytes. As a
;;; result, this value is a request and not a setting.
(defglobal ?*sockbuf* = 2048)

;;; This value is much more than one 4k write buffer. As a result, the test can
;;; tell a run that keeps only the buffer from a run that keeps each byte that
;;; the socket must still send.
(defglobal ?*flood-chunks* = 24)

;;; The limit that the test gives to set-retained-limit. It is above the 4k
;;; buffer, and code that only stops at the buffer cannot pass this test.
;;; It is also well below the quantity that the test writes, and the limit
;;; sets the maximum size of the queue.
(defglobal ?*limit* = 16384)

(defglobal ?*max-rounds* = 400)

;;=====================================================================
;; Scaffolding
;;=====================================================================

;;; One line of a known length. The code makes it by multiplication and not by
;;; addition, for the cause that large-payloads.clp gives: str-cat copies the
;;; string, and to add in a loop costs the square of the length.
(deffunction chunk ()
   (bind ?s "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde")
   (loop-for-count 6 do (bind ?s (str-cat ?s ?s)))
   (return ?s))

;;; A TLS pair after the handshake, with kernel buffers that are small enough
;;; to fill.
;;;
;;; The code sets the buffers before connect. TCP selects the window that it
;;; announces at that time. A call after connect succeeds but comes too late.
;;; As a result, this function cannot use tcp-connected-pair, which connects
;;; for its caller. This is the one place in the suite that needs code between
;;; the creation of the sockets and the connect call.
;;;
;;; Gives the client, the accepted end and the listen socket. It gives FALSE if
;;; the handshake did not complete.
(deffunction small-tls-pair (?cctx ?sctx ?port)
   (bind ?srv (create-socket AF_INET SOCK_STREAM))
   (setsockopt ?srv SOL_SOCKET SO_REUSEADDR 1)
   (setsockopt ?srv SOL_SOCKET SO_RCVBUF ?*sockbuf*)
   (bind-socket ?srv 127.0.0.1 ?port)
   (listen ?srv)

   (bind ?cli (create-socket AF_INET SOCK_STREAM))
   (setsockopt ?cli SOL_SOCKET SO_SNDBUF ?*sockbuf*)
   (connect ?cli 127.0.0.1 ?port)
   (bind ?acc (accept ?srv))

   (fcntl-add-status-flags ?cli O_NONBLOCK)
   (fcntl-add-status-flags ?acc O_NONBLOCK)

   (tls-connect ?cctx ?cli localhost)
   (tls-accept ?sctx ?acc)

   (if (not (tls-drive-handshake ?cli ?acc 80)) then (return FALSE))

   (return (create$ ?cli ?acc ?srv)))

;;; Writes lines to a session that no code reads. Gives the number of bytes
;;; that the code wrote, and a flag that is true if a flush reported a failure.
;;;
;;; printout accepted each byte in this count. As a result, the session must
;;; send each of these bytes to the peer, and this includes the bytes in the
;;; write that got the refusal. The test compares the drain with this total.
(deffunction flood (?wfd ?wname ?chunk ?n)
   (bind ?written 0)
   (bind ?refused FALSE)
   (loop-for-count ?n do
      (printout ?wname ?chunk crlf)
      (bind ?written (+ ?written (str-length ?chunk) 1))
      (if (not (flush-connection ?wfd)) then (bind ?refused TRUE)))
   (return (create$ ?written ?refused)))

;;; Reads each byte that the peer gives and at the same time sends the data
;;; that the writer holds. The loop ends when the writer has no data left and
;;; the reader has no data left. The function gives the byte count, the number
;;; of lines with an incorrect length, and a flag that is true if the writer
;;; completed.
;;;
;;; The loop reads one character at a time and not one line at a time, for the
;;; cause that the plaintext file gives. An exact byte total is the only method
;;; to see a short delivery, and the distance between newlines shows that the
;;; bytes arrived in the sequence of the writes.
(deffunction drain-both (?wfd ?rfd ?rname ?linelen)
   (bind ?read 0)
   (bind ?ragged 0)
   (bind ?since 0)
   (bind ?done FALSE)
   (bind ?r 0)
   (while (< ?r ?*max-rounds*) do
      (bind ?n 0)
      (bind ?c (get-char ?rname))
      (while (neq ?c -1) do
         (bind ?n (+ ?n 1))
         (if (eq ?c 10)
            then
            (if (neq ?since ?linelen) then (bind ?ragged (+ ?ragged 1)))
            (bind ?since 0)
            else
            (bind ?since (+ ?since 1)))
         (bind ?c (get-char ?rname)))
      (bind ?read (+ ?read ?n))

      (bind ?done (flush-connection ?wfd))

      ;; A writer with no data left is not the end. The data that it gave to
      ;; the kernel in this cycle is still on the network, and the read above
      ;; came before that flush. To stop here would leave the last data
      ;; unread and count it as lost. As a result, the loop asks the reader
      ;; directly, and it ends only when the reader also has no data.
      (if (and ?done (= ?n 0)) then
         (if (not (poll ?rfd 100 POLLIN)) then (break)))

      (poll ?wfd 20 POLLOUT)
      (bind ?r (+ ?r 1)))

   ;; A last part with no newline after it is a line that did not arrive
   ;; completely. The count of incorrect lines includes it, and no one can
   ;; miss it.
   (if (> ?since 0) then (bind ?ragged (+ ?ragged 1)))

   (return (create$ ?read ?ragged ?done)))

(deffunction shut (?pair ?cctx ?sctx)
   (close-connection (nth$ 1 ?pair))
   (close-connection (nth$ 2 ?pair))
   (close-connection (nth$ 3 ?pair))
   (tls-free-context ?cctx)
   (tls-free-context ?sctx))

;;=====================================================================
;; With no limit, the code discards no accepted data.
;;
;; This is the default, and a caller gets it without a request. Each byte that
;; printout took must reach the peer.
;;=====================================================================
(deffunction run-unlimited-tests ()
   (bind ?ctx (tls-fixture-contexts))
   (bind ?cctx (nth$ 1 ?ctx))
   (bind ?sctx (nth$ 2 ?ctx))

   (bind ?pair (small-tls-pair ?cctx ?sctx ?*port*))
   (expect-neq "a small-buffered TLS pair handshakes" FALSE ?pair)

   (bind ?cli   (nth$ 1 ?pair))
   (bind ?acc   (nth$ 2 ?pair))
   (bind ?cname (get-socket-logical-name ?cli))
   (bind ?aname (get-socket-logical-name ?acc))

   (expect-eq "an encrypted socket starts with no retained limit"
              0 (get-retained-limit ?cli))
   (expect-eq "and nothing retained" 0 (get-retained-bytes ?cli))

   (bind ?chunk (chunk))
   (bind ?f (flood ?cli ?cname ?chunk ?*flood-chunks*))
   (bind ?written (nth$ 1 ?f))

   (expect-true "a session that cannot send says so" (nth$ 2 ?f))

   ;; The write buffer grows past its first size, and the session discards no
   ;; data that it accepted.
   (expect-gte "and holds more than one buffer's worth of what it owes"
               4097 (get-retained-bytes ?cli))

   (bind ?d (drain-both ?cli ?acc ?aname (str-length ?chunk)))

   (expect-true "the writer finishes once the peer drains" (nth$ 3 ?d))
   (expect-eq "with no limit set, nothing is dropped" ?written (nth$ 1 ?d))
   (expect-eq "every line whole and in order" 0 (nth$ 2 ?d))
   (expect-eq "and nothing is left retained" 0 (get-retained-bytes ?cli))

   (printout ?cname "still here" crlf)
   (expect-true "the session takes new writes afterwards" (flush-connection ?cli))
   (poll ?acc 200 POLLIN)
   (expect-eq "which arrive intact" "still here" (readline ?aname))

   (shut ?pair ?cctx ?sctx))

;;=====================================================================
;; A session with a limit on the data that it keeps.
;;
;; This is the same choice that the plaintext code offers. The memory has a
;; maximum, and the code discards the data above it. A program must ask for the
;; limit, because a program that cannot detect the loss must not get the loss
;; by default.
;;=====================================================================
(deffunction run-limited-tests ()
   (bind ?ctx (tls-fixture-contexts))
   (bind ?cctx (nth$ 1 ?ctx))
   (bind ?sctx (nth$ 2 ?ctx))

   (bind ?pair (small-tls-pair ?cctx ?sctx (+ ?*port* 1)))
   (expect-neq "a second small-buffered TLS pair handshakes" FALSE ?pair)

   (bind ?cli   (nth$ 1 ?pair))
   (bind ?acc   (nth$ 2 ?pair))
   (bind ?cname (get-socket-logical-name ?cli))
   (bind ?aname (get-socket-logical-name ?acc))

   (expect-false "a limit cannot be negative" (set-retained-limit ?cli -1))
   (expect-true "a limit can be set on an encrypted socket"
                (set-retained-limit ?cli ?*limit*))
   (expect-eq "and reads back" ?*limit* (get-retained-limit ?cli))

   (bind ?chunk (chunk))
   (bind ?f (flood ?cli ?cname ?chunk ?*flood-chunks*))
   (bind ?written (nth$ 1 ?f))

   ;; This is the purpose of the limit. The test asks while the session is
   ;; still full, because this is a statement about the maximum and not about
   ;; the final condition.
   (expect-lte "what is held stays inside the limit"
               ?*limit* (get-retained-bytes ?cli))

   (bind ?d (drain-both ?cli ?acc ?aname (str-length ?chunk)))

   (expect-true "past the limit the excess is dropped rather than held"
                (< (nth$ 1 ?d) ?written))
   (expect-true "and the session still finishes what it kept" (nth$ 3 ?d))

   ;; A limit on the memory does not stop the session.
   (printout ?cname "still here" crlf)
   (expect-true "it takes new writes afterwards" (flush-connection ?cli))
   (poll ?acc 200 POLLIN)
   (expect-eq "which arrive intact" "still here" (readline ?aname))

   (expect-true "a limit can be lifted" (set-retained-limit ?cli 0))
   (expect-eq "and reads back as none" 0 (get-retained-limit ?cli))

   (shut ?pair ?cctx ?sctx))

(deffunction run-tests ()
   (run-unlimited-tests)
   (run-limited-tests))

(run-tests)
(test-summary)
