;;; A session whose peer stopped to read, or stopped to exist.
;;;
;;; requires: tls
;;;
;;; Each other test in the suite reads and writes to a peer that is present and
;;; that keeps up. As a result, no test reaches the code that a backend runs
;;; when the peer is absent. That code is the part of TLSBackendRead and
;;; TLSBackendWrite that reports a closed connection, a would-block condition,
;;; or a failure.
;;;
;;; There are two conditions, and they are not the same:
;;;
;;;   The peer is gone.       A read finds the end of input. A write fails,
;;;                           but not always at the first attempt, because TCP
;;;                           accepts bytes that it did not deliver.
;;;
;;;   The peer is silent.     Nothing failed and the code can send nothing,
;;;                           because the far end stopped to read and each
;;;                           buffer between the two ends is full. The write
;;;                           must report that condition and must keep the
;;;                           data. That is the last check here and the most
;;;                           important one.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(load* "tests/lib/tls.clp")
(test-suite "tls-peer-gone")
(test-plan 9)

(defglobal ?*port* = 19211)

;;; The quantity that the test writes before it stops. Each cycle is two
;;; thousand bytes, and the total is some megabytes. That is much more than the
;;; loopback buffers hold. The limit also makes the test end on a platform with
;;; larger buffers, and the test does not run to its time limit.
(defglobal ?*rounds* = 4000)

;;; A connected TLS pair after the handshake, with two non-blocking ends.
;;; Gives the client, the accepted socket and the listen socket.
;;;
;;; The code frees the contexts as soon as the sessions exist. That is not
;;; cleanup. It is part of this test: a session must keep its own context in
;;; memory. Each backend except OpenSSL needs the reference count in socktls.c
;;; for that. See tests/error/tls-context-lifetime.clp.
(deffunction tls-pair (?port)
   (bind ?ctx (tls-fixture-contexts))
   (bind ?cctx (nth$ 1 ?ctx))
   (bind ?sctx (nth$ 2 ?ctx))

   (bind ?pair (tls-connected-pair ?cctx ?sctx ?port))

   (tls-free-context ?sctx)
   (tls-free-context ?cctx)

   (return ?pair))

(deffunction run-tests ()
   ;; A write to a peer that went away raises SIGPIPE. That signal would end
   ;; the process and give no error.
   (signal SIGPIPE SIG_IGN)

   ;;=================================================================
   ;; The peer is gone
   ;;=================================================================
   (bind ?p (tls-pair ?*port*))
   (bind ?cli (nth$ 1 ?p))
   (bind ?acc (nth$ 2 ?p))
   (bind ?name (get-socket-logical-name ?cli))
   (expect-true "the pair is handshaked" (tls-version ?cli))

   ;; The code closes without tls-shutdown, and it sends no close_notify. The
   ;; far end sees a connection that ends in the middle of the data. The
   ;; backends must read that condition as the end of input and not as a
   ;; failure. It is usual on a real network, and a failure here would make
   ;; usual traffic look like an error.
   (close-connection ?acc)
   (sleep 0.1)

   (expect-eq "reading from a peer that vanished gives end of input" -1
              (get-char ?name))

   ;; The first write usually succeeds. TCP takes the bytes before it knows
   ;; that the far end is gone. The failure comes at a later write. As a
   ;; result, this test writes two times and checks the second write.
   (printout ?name "first" crlf)
   (flush-connection ?cli)
   (sleep 0.1)
   (printout ?name "second" crlf)
   (expect-false "writing to a peer that vanished eventually fails"
                 (flush-connection ?cli))

   (close-connection ?cli)
   (close-connection (nth$ 3 ?p))

   ;;=================================================================
   ;; The peer is silent
   ;;=================================================================
   (bind ?q (tls-pair (+ ?*port* 1)))
   (bind ?c2 (nth$ 1 ?q))
   (bind ?a2 (nth$ 2 ?q))
   (bind ?n2 (get-socket-logical-name ?c2))
   (expect-true "the second pair is handshaked" (tls-version ?c2))

   ;; The mode is full buffering. As a result, the data collects and goes out
   ;; in complete buffers and not one line at a time.
   (expect-true "full buffering is set" (set-fully-buffered ?c2))

   (bind ?blob "")
   (loop-for-count 200 do (bind ?blob (str-cat ?blob "0123456789")))

   ;; The far end never reads, and at some point there is no space for the
   ;; data. The loop has a limit, because a platform with large buffers can
   ;; always have space.
   (bind ?k 0)
   (bind ?blocked FALSE)
   (while (and (< ?k ?*rounds*) (not ?blocked)) do
      (printout ?n2 ?blob)
      (if (not (flush-connection ?c2)) then (bind ?blocked TRUE))
      (bind ?k (+ ?k 1)))

   (if (not ?blocked)
      then
      (test-skip "a full connection reports EAGAIN rather than failing"
                 "the far end absorbed everything this test is willing to write")
      (test-skip "nothing is lost when a write cannot complete"
                 "the far end absorbed everything this test is willing to write")
      (test-skip "a buffering change fails while the buffer cannot go out"
                 "the far end absorbed everything this test is willing to write")
      (test-skip "the same change succeeds once the buffer is empty"
                 "the far end absorbed everything this test is willing to write")
      else
      ;; This is the same result as a non-blocking write through stdio. Code
      ;; that already controls that condition also controls this one.
      (expect-errno "a full connection reports EAGAIN rather than failing" EAGAIN)

      ;; A change of buffering mode has to send what the old mode collected.
      ;; The data went into the buffer under a rule about when it leaves, and
      ;; a new rule cannot apply to bytes that the old one already holds.
      ;; Here that send cannot finish, because the far end still reads
      ;; nothing. The change has to fail and say so.
      ;;
      ;; The alternative is worse than an error. A mode that changes anyway
      ;; would leave the collected bytes under a rule that no longer sends
      ;; them, and the caller would have no way to learn that.
      (expect-false "a buffering change fails while the buffer cannot go out"
                    (set-not-buffered ?c2))

      ;; The bytes that could not go out are still available. This is the
      ;; important part of the test. A write that stops must keep its
      ;; remainder and continue from that point. It must not send the data
      ;; that already went, and it must not discard the data that did not go.
      ;; A read at the far end makes space, and the same buffer then
      ;; completes.
      (empty-connection ?a2)
      (bind ?j 0)
      (bind ?drained FALSE)
      (while (and (< ?j 200) (not ?drained)) do
         (if (flush-connection ?c2) then (bind ?drained TRUE))
         (empty-connection ?a2)
         (bind ?j (+ ?j 1)))
      (expect-true "nothing is lost when a write cannot complete" ?drained)

      ;; The refusal above was about the state of the buffer and not about the
      ;; session. An empty buffer has nothing to send under the old rule, so
      ;; the same call now has nothing to stop it.
      (expect-true "the same change succeeds once the buffer is empty"
                   (set-not-buffered ?c2)))

   (close-connection ?c2)
   (close-connection ?a2)
   (close-connection (nth$ 3 ?q)))

(run-tests)
(test-summary)
