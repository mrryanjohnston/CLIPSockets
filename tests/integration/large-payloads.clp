;;; Messages that are much larger than one buffer.
;;;
;;; Each other test in this suite moves one short line at a time. One write
;;; puts that line on the network and one read takes it off. Such a test never
;;; reaches the loop below stdio. stdio gives a long write to the kernel in the
;;; sizes that the kernel accepts, and a short read cannot make it do that more
;;; than one time.
;;;
;;; There are two shapes here, because they fail differently. One long write
;;; checks that the code completes one call and does not stop at the first
;;; partial write. Many writes in sequence check that no state stays between
;;; the calls. Examples of such state are an offset that the code does not
;;; reset and a buffer that the code uses again without a clear.
;;;
;;; The two ends are in this one process. As a result, a message that is larger
;;; than the socket buffer would give a deadlock. The writer would block with a
;;; full buffer, and the reader is in the same thread. The sizes here are below
;;; that limit. The second section moves much more data than any buffer holds.
;;; It stays safe because it reads each part before it sends the next one.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(test-suite "large-payloads")
(test-plan 18)

(defglobal ?*port-one*   = 19311)
(defglobal ?*port-many*  = 19312)

;;; A string that the code makes from a seed of 64 characters. It multiplies
;;; the length by two, ?doublings times. As a result, the length is exact and
;;; the content changes inside each line. The code multiplies and does not add
;;; in a loop. To add costs the square of the length, because str-cat copies
;;; the string. The counts here are large enough that the difference is a test
;;; or a timeout.
(deffunction payload (?doublings)
   (bind ?s "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/")
   (bind ?i 0)
   (while (< ?i ?doublings) do
      (bind ?s (str-cat ?s ?s))
      (bind ?i (+ ?i 1)))
   (return ?s))

;;=====================================================================
;; One write that is much larger than a buffer.
;;=====================================================================
(deffunction run-single-write-tests ()
   (bind ?big (payload 10))
   (expect-eq "the payload is 64 KiB" 65536 (str-length ?big))

   (bind ?p (tcp-connected-pair ?*port-one*))
   (bind ?srv (nth$ 1 ?p))
   (bind ?cli (nth$ 2 ?p))
   (bind ?acc (nth$ 3 ?p))
   (bind ?cname (get-socket-logical-name ?cli))
   (bind ?aname (get-socket-logical-name ?acc))

   (printout ?cname ?big crlf)
   (expect-true "a 64 KiB write flushes" (flush-connection ?cli))

   (bind ?got (readline ?aname))
   (expect-eq "the whole payload arrives as one line" 65536 (str-length ?got))
   ;; A check of the length alone would pass for data that arrived in the
   ;; incorrect sequence, or with a part two times. A partial write with
   ;; incorrect code gives those two results.
   (expect-eq "and is byte for byte what was sent" ?big ?got)

   ;; The same test in the other direction. The two ends are different
   ;; routers on different descriptors, and only one of them carried a long
   ;; write until now.
   (printout ?aname ?big crlf)
   (expect-true "the accepted end flushes a 64 KiB write" (flush-connection ?acc))
   (bind ?back (readline ?cname))
   (expect-eq "which arrives whole" 65536 (str-length ?back))
   (expect-eq "and unaltered" ?big ?back)

   ;; A short line immediately after a long one. If the long write left an
   ;; offset, this check shows it.
   (printout ?cname "short line after a long one" crlf)
   (flush-connection ?cli)
   (expect-eq "a short line after a long one is intact"
              "short line after a long one" (readline ?aname))

   (close-connection ?cli)
   (close-connection ?acc)
   (close-connection ?srv))

;;=====================================================================
;; Many writes, with a total that is larger than any buffer.
;;=====================================================================
(deffunction run-repeated-write-tests ()
   (bind ?chunk (payload 8))
   (expect-eq "each chunk is 16 KiB" 16384 (str-length ?chunk))

   (bind ?p (tcp-connected-pair ?*port-many*))
   (bind ?srv (nth$ 1 ?p))
   (bind ?cli (nth$ 2 ?p))
   (bind ?acc (nth$ 3 ?p))
   (bind ?cname (get-socket-logical-name ?cli))
   (bind ?aname (get-socket-logical-name ?acc))

   (bind ?rounds 40)
   (bind ?moved 0)
   (bind ?bad 0)
   (bind ?i 0)
   ;; The code reads in each cycle. As a result, the buffer does not limit
   ;; the total quantity of data.
   (while (< ?i ?rounds) do
      (printout ?cname ?chunk crlf)
      (flush-connection ?cli)
      (bind ?got (readline ?aname))
      (if (neq ?got ?chunk) then (bind ?bad (+ ?bad 1)))
      (bind ?moved (+ ?moved (str-length ?got)))
      (bind ?i (+ ?i 1)))

   (expect-eq "every round arrives unaltered" 0 ?bad)
   (expect-eq "640 KiB moved in total" 655360 ?moved)

   ;; The directions change in each cycle, and the loop uses the two
   ;; routers.
   (bind ?bad2 0)
   (bind ?j 0)
   (while (< ?j 10) do
      (printout ?cname ?chunk crlf)
      (flush-connection ?cli)
      (if (neq (readline ?aname) ?chunk) then (bind ?bad2 (+ ?bad2 1)))
      (printout ?aname ?chunk crlf)
      (flush-connection ?acc)
      (if (neq (readline ?cname) ?chunk) then (bind ?bad2 (+ ?bad2 1)))
      (bind ?j (+ ?j 1)))
   (expect-eq "alternating directions stay separate" 0 ?bad2)

   ;; The socket still operates after all of that data.
   (printout ?cname "done" crlf)
   (flush-connection ?cli)
   (expect-eq "the socket still carries a short line" "done" (readline ?aname))

   (close-connection ?cli)
   (close-connection ?acc)
   (close-connection ?srv))

;;=====================================================================
;; A read of a long line, one character at a time.
;;
;; readline takes the full line in one call. As a result, it asks the read
;; callback for more data only one or two times. A read of one character at a
;; time passes each point where the code fills the buffer again.
;;=====================================================================
(deffunction run-char-read-tests ()
   (bind ?chunk (payload 6))
   (expect-eq "the payload is 4 KiB" 4096 (str-length ?chunk))

   (bind ?p (tcp-connected-pair 19313))
   (bind ?srv (nth$ 1 ?p))
   (bind ?cli (nth$ 2 ?p))
   (bind ?acc (nth$ 3 ?p))
   (bind ?cname (get-socket-logical-name ?cli))
   (bind ?aname (get-socket-logical-name ?acc))

   (printout ?cname ?chunk crlf)
   (flush-connection ?cli)

   ;; The code counts the characters and does not make the string again. To
   ;; make a string of 4096 characters with one str-cat for each character
   ;; costs the square of the length, and that would be most of the time of
   ;; this file. The test checks the first and the last character
   ;; directly.
   (bind ?first (get-char ?aname))
   (expect-eq "the first character is the first one sent" 48 ?first)

   (bind ?n 1)
   (bind ?c (get-char ?aname))
   (while (and (neq ?c -1) (neq ?c 10)) do
      (bind ?n (+ ?n 1))
      (bind ?c (get-char ?aname)))

   (expect-eq "reading by character finds every byte of the line" 4096 ?n)
   (expect-eq "and stops at the newline" 10 ?c)

   ;; The socket still operates after the code reads to the end of the
   ;; line.
   (printout ?cname "after the walk" crlf)
   (flush-connection ?cli)
   (expect-eq "a line still reads normally afterwards"
              "after the walk" (readline ?aname))

   (close-connection ?cli)
   (close-connection ?acc)
   (close-connection ?srv))

(run-single-write-tests)
(run-repeated-write-tests)
(run-char-read-tests)
(test-summary)
