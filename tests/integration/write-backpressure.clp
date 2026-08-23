;;; A socket that accepts no more data.
;;;
;;; Each other write in this suite is small, and the kernel takes all of it.
;;; large-payloads.clp moves much more data than a buffer holds, but it empties
;;; each part before it sends the next one. As a result, it never fills a
;;; buffer. This file is about the condition that the other tests prevent: a
;;; writer with no more space.
;;;
;;; The send buffer of a socket has a limit. A write past that limit makes the
;;; kernel take the data that fits and refuse the remainder. On a blocking
;;; socket the kernel waits. On a non-blocking socket it gives EAGAIN in the
;;; middle of the write. The write is short: some of the data went out and some
;;; did not. This file is about what happens to the data that did not go out.
;;;
;;; The socket must be non-blocking, or this test cannot run. The two ends are
;;; in this one process. A blocking writer that fills the buffer waits for a
;;; reader in the same thread, and the test never returns. This is not a limit
;;; of the test. It is why a server that writes replies of a size that it does
;;; not control needs O_NONBLOCK, and why this code needs a test.
;;;
;;; The rules below are not new here. src/socktls.c already applies them to
;;; encrypted sockets, in FlushOutput. On a short write that function moves the
;;; remainder to the start of its buffer, sets errno to EAGAIN and gives false.
;;; Its comment says that the code keeps the data and does not discard it, so
;;; that a caller that flushes again after the socket is ready sends the same
;;; record and not a shorter one. A caller must not have to write different
;;; code for an encrypted socket. As a result, the plaintext code must do the
;;; same three things:
;;;
;;;   it discards no accepted data  the bytes that printout took are still
;;;                                 available to send when the socket is ready
;;;   it can report the failure     flush-connection gives FALSE while the
;;;                                 data is not out, and errno gives EAGAIN
;;;   it recovers completely        after the peer reads, a later flush
;;;                                 succeeds and each byte arrives in sequence
;;;
;;; printout can report nothing itself. The write callback of a CLIPS router
;;; gives void, and the TLS callback does the same. flush-connection is the
;;; only location for the answer, and this test asks that function.
;;;
;;; Most of the work below uses AF_UNIX and not TCP. This is about the quantity
;;; of data that fills a buffer. The kernel tunes a loopback TCP connection,
;;; and it takes some megabytes before it refuses data. A unix socket refuses
;;; after approximately two hundred kilobytes. The code under test is the write
;;; callback of the router, and that code is the same in each domain. As a
;;; result, the domain sets only the duration of the test, and ten times less
;;; data is ten times less wait. The last section uses TCP, because almost each
;;; caller is in that domain.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(test-suite "write-backpressure")
(test-plan 42)

(defglobal ?*path-fill*  = "/tmp/clipsockets-test-backpressure.sock")
(defglobal ?*path-line*  = "/tmp/clipsockets-test-backpressure-line.sock")
(defglobal ?*path-flood* = "/tmp/clipsockets-test-backpressure-flood.sock")
(defglobal ?*path-limit* = "/tmp/clipsockets-test-backpressure-limit.sock")
(defglobal ?*path-big*   = "/tmp/clipsockets-test-backpressure-big.sock")
(defglobal ?*port-tcp*   = 19411)

;;; The number of chunks that the test writes before it expects a refusal. The
;;; kernel sets the size of a send buffer, and no code here sets it. As a
;;; result, this value is a limit that stops a loop with no end. It is not a
;;; target.
(defglobal ?*max-chunks* = 200)

;;; The number of read and flush cycles that one drain can take. Each cycle
;;; moves each available byte. As a result, a small number is sufficient, and
;;; this limit is of use only when the code stops to make progress.
(defglobal ?*max-rounds* = 200)

;;; The limit that the section below gives to set-retained-limit. It is smaller
;;; than one chunk, and that is on purpose. As a result, the limit sets the
;;; maximum size of the queue, and the size of one write does not.
(defglobal ?*limit* = 16384)

;;; The number of chunks that the two sections send after a refusal. A unix
;;; socket refuses after approximately four chunks, and this value is well past
;;; that point. It is not larger, because with no limit the code keeps each of
;;; these bytes and then reads them back one character at a time.
(defglobal ?*flood-chunks* = 12)

;;=====================================================================
;; Scaffolding
;;=====================================================================

;;; A chunk that is large enough that the stdio buffer cannot hold one
;;; printout. Such a chunk must reach the socket in more than one write. The
;;; code makes it by multiplication and not by addition, for the cause that
;;; large-payloads.clp gives: str-cat copies the string, and to add in a loop
;;; costs the square of the length.
(deffunction chunk ()
   (bind ?s "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde")
   (loop-for-count 10 do (bind ?s (str-cat ?s ?s)))
   (return ?s))

;;; A connected AF_UNIX pair with two non-blocking ends. Gives the listen
;;; socket, the write end, the read end, and the two logical names.
(deffunction unix-pair (?path)
   (remove ?path)

   (bind ?srv (create-socket AF_UNIX SOCK_STREAM))
   (bind-socket ?srv ?path)
   (listen ?srv)

   (bind ?cli (create-socket AF_UNIX SOCK_STREAM))
   (bind ?cname (connect ?cli ?path))
   (bind ?acc (accept ?srv))
   (bind ?aname (get-socket-logical-name ?acc))

   (fcntl-add-status-flags ?cli O_NONBLOCK)
   (fcntl-add-status-flags ?acc O_NONBLOCK)

   (return (create$ ?srv ?cli ?acc ?cname ?aname)))

;;; Writes chunks to a socket that no code reads, until one chunk cannot go
;;; out. Gives the number of bytes that the code wrote, the number of lines,
;;; and a flag that is true if the socket refused a write.
;;;
;;; printout accepted each byte in this count. As a result, the socket must
;;; send each of these bytes to the peer, and this includes the bytes in the
;;; write that got the refusal. The test compares the drain below with this
;;; total.
(deffunction fill-until-refused (?wfd ?wname ?chunk)
   (bind ?written 0)
   (bind ?lines 0)
   (bind ?refused FALSE)
   (bind ?i 0)
   (while (< ?i ?*max-chunks*) do
      (printout ?wname ?chunk crlf)
      (bind ?written (+ ?written (str-length ?chunk) 1))
      (bind ?lines (+ ?lines 1))
      (if (not (flush-connection ?wfd)) then
         (bind ?refused TRUE)
         (break))
      (bind ?i (+ ?i 1)))
   (return (create$ ?written ?lines ?refused)))

;;; Reads each byte that the peer gives and at the same time sends the data
;;; that the writer holds. The loop ends when the writer has no data left and
;;; the reader has no data left. The function gives the byte count, the line
;;; count, the number of lines with an incorrect length, and a flag that is
;;; true if the writer completed.
;;;
;;; A real relay has this shape, and it is the only shape that operates with
;;; the two ends in one process. A read makes space, the space lets the flush
;;; make progress, and the poll prevents a cycle with no wait.
;;;
;;; The loop reads one character at a time and not one line at a time, for two
;;; causes. First, an exact byte total is the only method to see a short
;;; delivery. readline stops at a newline and reports nothing about a line that
;;; arrived incomplete. Second, the distance between newlines shows that the
;;; bytes arrived in the sequence of the writes. Each line here has the same
;;; length. As a result, a newline at a different position shows that some data
;;; came in the incorrect sequence. That is the failure to watch for, because a
;;; write can go out through two paths: through stdio, or directly to the
;;; descriptor from the retained data.
(deffunction drain-both (?wfd ?rname ?linelen)
   (bind ?read 0)
   (bind ?lines 0)
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
            (bind ?lines (+ ?lines 1))
            (if (neq ?since ?linelen) then (bind ?ragged (+ ?ragged 1)))
            (bind ?since 0)
            else
            (bind ?since (+ ?since 1)))
         (bind ?c (get-char ?rname)))
      (bind ?read (+ ?read ?n))

      (bind ?done (flush-connection ?wfd))
      (if (and ?done (= ?n 0)) then (break))

      (poll ?wfd 20 POLLOUT)
      (bind ?r (+ ?r 1)))

   ;; A last part with no newline after it is a line that did not arrive
   ;; completely. The count of incorrect lines includes it, and no one can
   ;; miss it.
   (if (> ?since 0) then (bind ?ragged (+ ?ragged 1)))

   (return (create$ ?read ?lines ?ragged ?done)))

;;; The same loop without the counts, for a section that only needs an empty
;;; socket. empty-connection discards the data in one call in C. That call is
;;; of value here, because the interpreter would otherwise read megabytes one
;;; character at a time.
;;;
;;; A flush that gives TRUE is not the end. It says that the writer has no data
;;; of its own. It says nothing about the megabytes that the kernel still holds
;;; for the reader. As a result, the loop asks the reader directly, and it ends
;;; only when the two sides are empty. To stop at the flush leaves data in the
;;; queue in front of the next write.
(deffunction drain-quickly (?wfd ?rfd ?rname)
   (bind ?done FALSE)
   (bind ?r 0)
   (while (< ?r ?*max-rounds*) do
      (empty-connection ?rname)
      (bind ?done (flush-connection ?wfd))
      (if (and ?done (not (poll ?rfd 0 POLLIN))) then (break))
      (poll ?wfd 20 POLLOUT)
      (bind ?r (+ ?r 1)))
   (return ?done))

;;=====================================================================
;; A full socket reports that condition, and it sends each byte that it
;; accepted.
;;
;; This section uses the default buffer mode. As a result, the refusal comes
;; from the flush-connection call after the printout.
;;=====================================================================
(deffunction run-buffered-tests ()
   ;; The test asks this before it makes the sockets, and not after. A call
   ;; that succeeds does not clear errno. That rule comes from C and not from
   ;; this library. unix-pair removes a socket file that is usually not
   ;; present, and that leaves ENOENT in errno. ENOENT is a real value from a
   ;; real call that failed. As a result, this position is the only one where
   ;; the question has a clean answer.
   (expect-eq "errno is clean before anything has failed" 0 (errno))

   (bind ?p (unix-pair ?*path-fill*))
   (bind ?srv   (nth$ 1 ?p))
   (bind ?wfd   (nth$ 2 ?p))
   (bind ?rfd   (nth$ 3 ?p))
   (bind ?wname (nth$ 4 ?p))
   (bind ?rname (nth$ 5 ?p))

   (expect-true "and an empty socket flushes cleanly" (flush-connection ?wfd))

   (bind ?chunk (chunk))
   (bind ?f (fill-until-refused ?wfd ?wname ?chunk))
   (bind ?written (nth$ 1 ?f))
   (bind ?lines   (nth$ 2 ?f))
   (bind ?refused (nth$ 3 ?f))

   (expect-true "a socket with a full send buffer refuses the write" ?refused)
   (expect-errno "and leaves EAGAIN behind" EAGAIN)

   ;; The peer starts to read, and the writer can then make progress.
   (bind ?d (drain-both ?wfd ?rname (str-length ?chunk)))
   (bind ?read   (nth$ 1 ?d))
   (bind ?got    (nth$ 2 ?d))
   (bind ?ragged (nth$ 3 ?d))
   (bind ?done   (nth$ 4 ?d))

   (expect-true "once the peer drains, the writer finishes" ?done)
   (expect-eq "and every byte written arrives" ?written ?read)
   (expect-eq "as the same number of lines" ?lines ?got)
   (expect-eq "each of them whole and in order" 0 ?ragged)

   ;; A socket that reported a failure and then recovered is a socket that
   ;; operates. The error flag of stdio stays set. As a result, this check
   ;; shows that the recovery was real and not only silent.
   (printout ?wname "still here" crlf)
   (expect-true "a later write flushes cleanly" (flush-connection ?wfd))
   (poll ?rfd 200 POLLIN)
   (expect-eq "and reaches the peer intact" "still here" (readline ?rname))

   (close-connection ?wfd)
   (close-connection ?rfd)
   (close-connection ?srv)
   (remove ?*path-fill*))

;;=====================================================================
;; The same checks, but the failure is inside printout.
;;
;; A line-buffered socket flushes at each newline. As a result, the write that
;; cannot go out fails in the router callback and not in a flush-connection
;; call from the test. The callback gives void and has no location to report
;; to. In this condition a caller most needs the next flush-connection to still
;; know about the failure.
;;=====================================================================
(deffunction run-line-buffered-tests ()
   (bind ?p (unix-pair ?*path-line*))
   (bind ?srv   (nth$ 1 ?p))
   (bind ?wfd   (nth$ 2 ?p))
   (bind ?rfd   (nth$ 3 ?p))
   (bind ?wname (nth$ 4 ?p))
   (bind ?rname (nth$ 5 ?p))

   (set-line-buffered ?wfd)

   (bind ?chunk (chunk))
   (bind ?f (fill-until-refused ?wfd ?wname ?chunk))
   (bind ?written (nth$ 1 ?f))
   (bind ?refused (nth$ 3 ?f))

   (expect-true "a line-buffered socket refuses the write too" ?refused)
   (expect-errno "and leaves EAGAIN behind" EAGAIN)

   (bind ?d (drain-both ?wfd ?rname (str-length ?chunk)))
   (expect-eq "with every byte still accounted for" ?written (nth$ 1 ?d))
   (expect-eq "and every line whole and in order" 0 (nth$ 3 ?d))

   (close-connection ?wfd)
   (close-connection ?rfd)
   (close-connection ?srv)
   (remove ?*path-line*))

;;=====================================================================
;; A caller that ignores the refusal.
;;
;; The code discards no accepted data, and this is true however long a program
;; continues to write after a refusal. The bytes wait in memory instead. This
;; is a cost and not a failure. The caller already had these bytes in a CLIPS
;; string. As a result, to keep them makes a maximum of two copies of data that
;; was already in memory. The other choice is to discard the data at some fixed
;; size, and that is the silent loss that this file is about.
;;
;; A program that wants a limit on the memory sets one, and the next section
;; shows that.
;;=====================================================================
(deffunction run-ignored-refusal-tests ()
   (bind ?p (unix-pair ?*path-flood*))
   (bind ?srv   (nth$ 1 ?p))
   (bind ?wfd   (nth$ 2 ?p))
   (bind ?rfd   (nth$ 3 ?p))
   (bind ?wname (nth$ 4 ?p))
   (bind ?rname (nth$ 5 ?p))

   (expect-eq "a socket starts with no retained limit" 0 (get-retained-limit ?wfd))
   (expect-eq "and nothing retained" 0 (get-retained-bytes ?wfd))

   (bind ?chunk (chunk))
   (bind ?written 0)
   (bind ?refusals 0)

   (loop-for-count ?*flood-chunks* do
      (printout ?wname ?chunk crlf)
      (bind ?written (+ ?written (str-length ?chunk) 1))
      (if (not (flush-connection ?wfd)) then
         (bind ?refusals (+ ?refusals 1))))

   (expect-gte "a socket that cannot send keeps saying so" 1 ?refusals)
   (expect-gte "and holds what it could not send" 1 (get-retained-bytes ?wfd))

   (bind ?d (drain-both ?wfd ?rname (str-length ?chunk)))

   (expect-true "the writer finishes once the peer drains" (nth$ 4 ?d))
   (expect-eq "with no limit set, nothing is dropped" ?written (nth$ 1 ?d))
   (expect-eq "every line whole and in order" 0 (nth$ 3 ?d))
   (expect-eq "and nothing is left retained" 0 (get-retained-bytes ?wfd))

   (printout ?wname "still here" crlf)
   (expect-true "the socket takes new writes afterwards" (flush-connection ?wfd))
   (poll ?rfd 200 POLLIN)
   (expect-eq "which arrive intact" "still here" (readline ?rname))

   (close-connection ?wfd)
   (close-connection ?rfd)
   (close-connection ?srv)
   (remove ?*path-flood*))

;;=====================================================================
;; A socket with a limit on the data that it keeps.
;;
;; This is the choice that a server makes when it cannot trust its peers to
;; continue to read. With a limit, the memory has a maximum and the code
;; discards the data above that maximum. To discard the data of one client is a
;; better failure than a process with no memory left. That is why this library
;; offers the limit. A program must ask for the limit, because a program that
;; cannot detect the loss must not get the loss by default.
;;=====================================================================
(deffunction run-limited-tests ()
   (bind ?p (unix-pair ?*path-limit*))
   (bind ?srv   (nth$ 1 ?p))
   (bind ?wfd   (nth$ 2 ?p))
   (bind ?rfd   (nth$ 3 ?p))
   (bind ?wname (nth$ 4 ?p))
   (bind ?rname (nth$ 5 ?p))

   (expect-false "a limit cannot be negative" (set-retained-limit ?wfd -1))
   (expect-true "a limit can be set" (set-retained-limit ?wfd ?*limit*))
   (expect-eq "and reads back" ?*limit* (get-retained-limit ?wfd))

   (bind ?chunk (chunk))
   (bind ?written 0)

   (loop-for-count ?*flood-chunks* do
      (printout ?wname ?chunk crlf)
      (bind ?written (+ ?written (str-length ?chunk) 1))
      (flush-connection ?wfd))

   ;; This is the purpose of the limit. The test asks while the socket is
   ;; still full, because this is a statement about the maximum and not about
   ;; the final condition.
   (expect-lte "what is held stays inside the limit" ?*limit*
               (get-retained-bytes ?wfd))

   (bind ?d (drain-both ?wfd ?rname (str-length ?chunk)))

   (expect-true "past the limit the excess is dropped rather than held"
                (< (nth$ 1 ?d) ?written))
   (expect-true "and the socket still finishes what it kept" (nth$ 4 ?d))

   ;; A limit on the memory does not stop the socket.
   (printout ?wname "still here" crlf)
   (expect-true "it takes new writes afterwards" (flush-connection ?wfd))
   (poll ?rfd 200 POLLIN)
   (expect-eq "which arrive intact" "still here" (readline ?rname))

   ;; Back to no limit. The socket accepts that change in the same manner.
   (expect-true "a limit can be lifted" (set-retained-limit ?wfd 0))
   (expect-eq "and reads back as none" 0 (get-retained-limit ?wfd))

   (close-connection ?wfd)
   (close-connection ?rfd)
   (close-connection ?srv)
   (remove ?*path-limit*))

;;=====================================================================
;; One write that is larger than the send buffer.
;;
;; A fixed limit cannot serve this condition, and that is why there is no limit
;; by default. Each section above writes chunks and checks the result between
;; them. As a result, a refusal always comes between two writes, where the
;; caller can see it. Here the full body goes out in one printout, in the same
;; manner as examples/server-http-file.clp sends a file. The refusal comes in
;; the middle of that one call, before the call gives a result and with no
;; action available to the caller. To cut the data here would be silent, and
;; the caller could do nothing about it.
;;=====================================================================
(deffunction run-single-large-write-tests ()
   (bind ?p (unix-pair ?*path-big*))
   (bind ?srv   (nth$ 1 ?p))
   (bind ?wfd   (nth$ 2 ?p))
   (bind ?rfd   (nth$ 3 ?p))
   (bind ?wname (nth$ 4 ?p))
   (bind ?rname (nth$ 5 ?p))

   ;; The code multiplies the chunk of the sections above by two, four times.
   ;; The result is much larger than a unix socket holds.
   (bind ?body (chunk))
   (loop-for-count 4 do (bind ?body (str-cat ?body ?body)))
   (bind ?want (+ (str-length ?body) 1))

   (printout ?wname ?body crlf)

   (bind ?d (drain-both ?wfd ?rname (str-length ?body)))

   (expect-true "one oversized write is sent in full" (nth$ 4 ?d))
   (expect-eq "with every byte of it delivered" ?want (nth$ 1 ?d))
   (expect-eq "as one whole line" 0 (nth$ 3 ?d))

   (close-connection ?wfd)
   (close-connection ?rfd)
   (close-connection ?srv)
   (remove ?*path-big*))

;;=====================================================================
;; The same statements on TCP.
;;
;; No code in the write path tests the address family. As a result, this
;; section is here for the domain and not for the code. Callers use TCP, and a
;; rule that a test checks only on unix sockets is a rule that no one checked
;; where programs use it. Loopback refuses nothing until megabytes move. As a
;; result, this section empties the socket without a count. It checks only what
;; the sections above already checked byte by byte.
;;=====================================================================
(deffunction run-tcp-tests ()
   (bind ?p (tcp-connected-pair ?*port-tcp*))
   (bind ?srv   (nth$ 1 ?p))
   (bind ?wfd   (nth$ 2 ?p))
   (bind ?rfd   (nth$ 3 ?p))
   (bind ?wname (nth$ 4 ?p))
   (bind ?rname (nth$ 5 ?p))

   ;; The two ends are non-blocking, and that is necessary here. A blocking
   ;; write to a full socket waits for the reader and does not refuse the
   ;; data, and there is no reader outside this thread to wait for.
   (fcntl-add-status-flags ?wfd O_NONBLOCK)
   (fcntl-add-status-flags ?rfd O_NONBLOCK)

   (bind ?f (fill-until-refused ?wfd ?wname (chunk)))

   (expect-true "a full TCP socket refuses the write" (nth$ 3 ?f))
   (expect-errno "and leaves EAGAIN behind" EAGAIN)
   (expect-true "and finishes once the peer drains"
                (drain-quickly ?wfd ?rfd ?rname))

   (printout ?wname "still here" crlf)
   (expect-true "with the socket still usable" (flush-connection ?wfd))
   (poll ?rfd 200 POLLIN)
   (expect-eq "and delivering intact" "still here" (readline ?rname))

   (close-connection ?wfd)
   (close-connection ?rfd)
   (close-connection ?srv))

(run-buffered-tests)
(run-line-buffered-tests)
(run-ignored-refusal-tests)
(run-limited-tests)
(run-single-large-write-tests)
(run-tcp-tests)
(test-summary)
