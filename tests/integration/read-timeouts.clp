;;; What a read does when the receive timeout ends.
;;;
;;; The other files cover (set-timeout) only as a set function. A value goes
;;; in, the same value comes back, and no code waits. This file is about the
;;; behaviour that the setting gives, and a caller depends on that behaviour.
;;;
;;; A read that timed out and a peer that closed give the same return value.
;;; Both give the symbol EOF, and neither gives a string. As a result, the
;;; value from (readline) cannot tell the two conditions apart. The difference
;;; is in errno. A timeout leaves EAGAIN, and a correct close does not change
;;; errno.
;;;
;;; This is why the test checks the close condition first, before any timeout
;;; in this file. A call that succeeds does not clear errno. That rule comes
;;; from C and not from this library. As a result, after a timeout leaves
;;; EAGAIN, a later close still reads EAGAIN and looks like a timeout. In the
;;; opposite sequence the two sections would pass and show nothing.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(test-suite "read-timeouts")
(test-plan 48)

(defglobal ?*port-close* = 19301)
(defglobal ?*port-time*  = 19302)
(defglobal ?*port-char*  = 19303)
(defglobal ?*port-long*  = 19304)

;;=====================================================================
;; A correct close, with a clean errno.
;;
;; This section must run before any timeout in this file. See the header.
;;=====================================================================
(deffunction run-close-tests ()
   (bind ?p (tcp-connected-pair ?*port-close*))
   (bind ?srv (nth$ 1 ?p))
   (bind ?cli (nth$ 2 ?p))
   (bind ?acc (nth$ 3 ?p))
   (bind ?aname (get-socket-logical-name ?acc))

   (expect-eq "errno is clean before anything has failed" 0 (errno))

   ;; The peer goes away and sends nothing.
   (close-connection ?cli)

   (bind ?got (readline ?aname))
   (expect-eq "a closed peer reads as EOF" EOF ?got)
   (expect-true "and EOF is a symbol, not the string \"EOF\"" (symbolp ?got))
   (expect-false "which is not a string" (stringp ?got))

   ;; This is the purpose of the sequence. Nothing failed, and errno keeps
   ;; its value.
   (expect-eq "an orderly close leaves errno alone" 0 (errno))

   (close-connection ?acc)
   (close-connection ?srv))

;;=====================================================================
;; A peer that stays silent.
;;=====================================================================
(deffunction run-timeout-tests ()
   (bind ?p (tcp-connected-pair ?*port-time*))
   (bind ?srv (nth$ 1 ?p))
   (bind ?cli (nth$ 2 ?p))
   (bind ?acc (nth$ 3 ?p))
   (bind ?aname (get-socket-logical-name ?acc))

   (expect-eq "a new socket has no timeout" 0 (get-timeout ?acc))

   (expect-true "a timeout can be set" (set-timeout ?acc 300000))
   (expect-eq "and reads back" 300000 (get-timeout ?acc))

   ;; The client is connected and sends nothing. As a result, only the clock
   ;; can end this read.
   (bind ?t0 (time))
   (bind ?got (readline ?aname))
   (bind ?elapsed (- (time) ?t0))

   (expect-eq "a timed-out read reads as EOF" EOF ?got)
   (expect-errno "a timed-out read leaves EAGAIN" EAGAIN)

   ;; The test checks a minimum and a maximum. The minimum is below the 300ms
   ;; of the request and not equal to it. The two (time) calls cover more than
   ;; the read, and a clock with a step of 10ms can report a value that is a
   ;; little below. The maximum separates a read that waited from a read that
   ;; stopped. The time limit of the suite for each file would find a read
   ;; that stopped, but it would report a timeout and not a failure of this
   ;; check.
   (expect-gte "it waited for about the time it was given" 0.2 ?elapsed)
   (expect-lte "and returned rather than hanging" 5.0 ?elapsed)

   ;; A timeout does not damage the socket. The same socket reads correctly
   ;; after the peer sends data.
   (printout (get-socket-logical-name ?cli) "after the timeout" crlf)
   (flush-connection ?cli)
   (expect-eq "the socket still works afterwards"
              "after the timeout" (readline ?aname))

   ;; The socket also times out again. As a result, one use does not remove
   ;; the setting.
   (bind ?again (readline ?aname))
   (expect-eq "it times out a second time" EOF ?again)
   (expect-errno "leaving EAGAIN again" EAGAIN)

   ;; A clear call puts the socket back to no timeout.
   (expect-true "the timeout can be cleared" (set-timeout ?acc 0))
   (expect-eq "and reads back as none" 0 (get-timeout ?acc))

   (close-connection ?cli)
   (close-connection ?acc)
   (close-connection ?srv))

;;=====================================================================
;; The timeout belongs to the socket and not to (readline).
;;=====================================================================
(deffunction run-get-char-tests ()
   (bind ?p (tcp-connected-pair ?*port-char*))
   (bind ?srv (nth$ 1 ?p))
   (bind ?cli (nth$ 2 ?p))
   (bind ?acc (nth$ 3 ?p))
   (bind ?aname (get-socket-logical-name ?acc))

   (set-timeout ?acc 200000)

   ;; The peer sends one character, and the test reads two. The first read
   ;; comes from the buffer. The second read goes to the socket and waits.
   (printout (get-socket-logical-name ?cli) "x")
   (flush-connection ?cli)

   (bind ?first (get-char ?aname))
   (expect-eq "the character that was sent arrives" 120 ?first)

   (bind ?t0 (time))
   (bind ?second (get-char ?aname))
   (bind ?elapsed (- (time) ?t0))

   (expect-eq "get-char times out as -1" -1 ?second)
   (expect-errno "and leaves EAGAIN too" EAGAIN)
   (expect-gte "having waited" 0.1 ?elapsed)
   (expect-lte "and not hung" 5.0 ?elapsed)

   (close-connection ?cli)
   (close-connection ?acc)
   (close-connection ?srv))

;;=====================================================================
;; How a timeout is written down
;;=====================================================================
;;; A timeout goes to the kernel as a struct timeval. That structure holds the
;;; seconds and the microseconds in separate fields, and the kernel refuses a
;;; microseconds field of 1000000 or more. It gives EDOM.
;;;
;;; As a result, the two forms of this call must agree. One argument is
;;; microseconds, and the code moves the whole seconds into the seconds field
;;; before the call. Two arguments are the seconds and then the microseconds,
;;; in the sequence of the structure. A microseconds argument of one million or
;;; more moves in the same manner.
;;;
;;; A call that the kernel refuses changes nothing and leaves the socket with
;;; no limit, and nothing about the socket shows that. As a result, the checks
;;; below are about the return value and the value that reads back. The section
;;; after this one is about the wait.
(deffunction run-argument-tests ()
   (bind ?fd (create-socket AF_INET SOCK_STREAM))

   ;; One argument, below one second. The value goes to the microseconds
   ;; field as it is.
   (expect-true "microseconds alone are accepted" (set-timeout ?fd 250000))
   (expect-eq "and read back" 250000 (get-timeout ?fd))

   ;; One argument, one second or more. The whole seconds must move out of
   ;; the microseconds field.
   (expect-true "a whole second in microseconds is accepted"
                (set-timeout ?fd 1000000))
   (expect-eq "and reads back as a second" 1000000 (get-timeout ?fd))
   (expect-true "so is more than a second" (set-timeout ?fd 5500000))
   (expect-eq "and reads back unchanged" 5500000 (get-timeout ?fd))

   ;; Two arguments: the seconds and then the microseconds.
   (expect-true "seconds and microseconds are accepted"
                (set-timeout ?fd 2 500000))
   (expect-eq "and read back as one number of microseconds"
              2500000 (get-timeout ?fd))
   (expect-true "seconds alone" (set-timeout ?fd 3 0))
   (expect-eq "read back as seconds" 3000000 (get-timeout ?fd))

   ;; The two forms give the same timeout, and that property makes each of
   ;; them safe to use.
   (set-timeout ?fd 2 500000)
   (bind ?two-args (get-timeout ?fd))
   (set-timeout ?fd 2500000)
   (expect-eq "both forms give the same timeout" ?two-args (get-timeout ?fd))

   ;; A microseconds argument of one million or more moves into the seconds
   ;; field. The code does not send it to the kernel, which would refuse
   ;; it.
   (expect-true "microseconds over a million carry into seconds"
                (set-timeout ?fd 1 1500000))
   (expect-eq "and add up" 2500000 (get-timeout ?fd))

   ;; Zero is not a timeout of zero length. It means no timeout, and each of
   ;; the two forms must accept it.
   (expect-true "zero clears the timeout" (set-timeout ?fd 0))
   (expect-eq "and reads back as none" 0 (get-timeout ?fd))
   (set-timeout ?fd 1 0)
   (expect-true "zero in both fields clears it too" (set-timeout ?fd 0 0))
   (expect-eq "and reads back as none again" 0 (get-timeout ?fd))

   ;; Refusals. A negative timeout has no meaning. A FALSE result separates a
   ;; request that the code refused from a request that the code applied.
   (expect-false "negative microseconds are refused" (set-timeout ?fd -1))
   (expect-false "negative seconds are refused" (set-timeout ?fd -1 0))
   (expect-false "and negative microseconds beside seconds"
                 (set-timeout ?fd 1 -5))
   (expect-eq "a refused call changes nothing" 0 (get-timeout ?fd))

   (close-connection ?fd))

;;=====================================================================
;; A timeout of a second or more waits
;;=====================================================================
;;; The section above shows that the value reaches the kernel. This section
;;; shows that the value operates. A socket with a failed set-timeout reads in
;;; the same manner as a socket with no timeout. Only a read that returns shows
;;; the difference.
(deffunction run-long-timeout-tests ()
   (bind ?p (tcp-connected-pair ?*port-long*))
   (bind ?srv (nth$ 1 ?p))
   (bind ?cli (nth$ 2 ?p))
   (bind ?acc (nth$ 3 ?p))
   (bind ?aname (get-socket-logical-name ?acc))

   (expect-true "a timeout of over a second is set"
                (set-timeout ?acc 1 100000))

   ;; The peer is connected and sends nothing. As a result, the clock ends
   ;; this read, or nothing ends it. Before the correction, nothing ended
   ;; it.
   (bind ?t0 (time))
   (bind ?got (readline ?aname))
   (bind ?elapsed (- (time) ?t0))

   (expect-eq "the read gives up" EOF ?got)
   (expect-errno "leaving EAGAIN" EAGAIN)

   ;; The minimum is the purpose of this section. A set-timeout call that
   ;; failed leaves a socket with no timeout, and such a socket never returns
   ;; from the read. As a result, any return here is better than the defect.
   ;; The minimum shows that the socket used the one second and not a smaller
   ;; value from an earlier call.
   (expect-gte "after waiting more than a second" 1.0 ?elapsed)
   (expect-lte "and not much longer" 5.0 ?elapsed)

   (close-connection ?cli)
   (close-connection ?acc)
   (close-connection ?srv))

(run-close-tests)
(run-timeout-tests)
(run-get-char-tests)
(run-argument-tests)
(run-long-timeout-tests)
(test-summary)
