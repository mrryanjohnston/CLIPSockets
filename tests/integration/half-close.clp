;;; A shutdown of one direction of a connection, and the use of the other
;;; direction.
;;;
;;; A TCP connection has two independent streams. A close call closes both of
;;; them, and close-connection does that. A shutdown call closes one stream and
;;; leaves the other one in operation. The end that stops to send reports that
;;; condition. The peer then reads the end of input and does not wait for more
;;; data. The reply still comes back through the half that is open.
;;;
;;; This is how a client says "this is the full request" on a protocol with no
;;; length field. The peer cannot know that a request is complete until a
;;; framing rule says so or the stream ends. A shutdown of the write side ends
;;; the stream but keeps the answer. curl does this, and each server that reads
;;; to the end of input needs it.
;;;
;;; SHUT_WR, SHUT_RD and SHUT_RDWR are already in two files of this suite.
;;; socket-creation.clp checks that the code accepts each name, and
;;; descriptor-leaks.clp calls one of them. Both files are about the call. This
;;; file is about the effect of the call on the connection. A program needs
;;; that effect, and no test checked it.
;;;
;;; This is the failure that the file prevents. A socket is an I/O router in
;;; CLIPS, and it is easy to read the end of input as a socket that is
;;; finished. A router that removed the logical name at EOF, or a
;;; close-connection call after an EOF, would also close the reply path. The
;;; server would already have the request, and the loss would show as a client
;;; that waits and not as an error.
;;;
;;; The code must ignore SIGPIPE, for the cause that closed-peer.clp gives. A
;;; write to a stream with no write side raises SIGPIPE, and its default action
;;; ends the process before any check can report a result.
;;;
;;; The two ends are in this one process, and here that needs no special care.
;;; No code below waits for data that is not already available. The end of
;;; input arrives when the other side shuts down. As a result, each read in
;;; this file answers immediately, although each socket blocks.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(test-suite "half-close")
(test-plan 46)

(defglobal ?*port-wr*    = 19421)
(defglobal ?*port-poll*  = 19422)
(defglobal ?*port-grace* = 19423)
(defglobal ?*port-rd*    = 19424)
(defglobal ?*port-both*  = 19425)

;;=====================================================================
;; SHUT_WR: the peer reads the end of input and still replies.
;;
;; This is the purpose of the operation, in the sequence that a request and
;; response protocol uses.
;;=====================================================================
(deffunction run-shut-wr-tests ()
   ;; This check comes before any failure. A call that succeeds does not clear
   ;; errno. As a result, the code must read the correct end of input below
   ;; before this file causes an EPIPE.
   (expect-eq "errno is clean before anything has failed" 0 (errno))

   (bind ?p (tcp-connected-pair ?*port-wr*))
   (bind ?srv   (nth$ 1 ?p))
   (bind ?a     (nth$ 2 ?p))
   (bind ?b     (nth$ 3 ?p))
   (bind ?aname (nth$ 4 ?p))
   (bind ?bname (nth$ 5 ?p))

   ;; The client sends its full request and then reports that it sends no
   ;; more data.
   (printout ?aname "the whole request" crlf)
   (expect-true "the request flushes" (flush-connection ?a))
   (expect-true "the write side can be shut down" (shutdown-connection ?a SHUT_WR))

   ;; The server reads the request and then reads the end of it.
   (expect-eq "the peer still reads what was sent" "the whole request"
              (readline ?bname))

   (bind ?eof (readline ?bname))
   (expect-eq "and then reads end of input" EOF ?eof)
   (expect-true "which is the symbol EOF" (symbolp ?eof))
   (expect-false "not the string" (stringp ?eof))

   ;; A correct end of input is not a failure, and errno keeps no value.
   ;; read-timeouts.clp makes the same statement about a peer that closed.
   ;; This is the half-close condition, and it is more important here, because
   ;; the connection still operates and a program continues to use it.
   (expect-eq "an orderly end of input leaves errno alone" 0 (errno))

   (expect-eq "reading again gives end of input again" EOF (readline ?bname))
   (expect-eq "and get-char agrees" -1 (get-char ?bname))

   ;;=================================================================
   ;; The other direction still carries data.
   ;;=================================================================
   (printout ?bname "the whole reply" crlf)
   (expect-true "the peer can still write" (flush-connection ?b))
   (expect-eq "and the shut-down end still reads" "the whole reply"
              (readline ?aname))

   ;; The end of input did not remove the router of the peer. If the router
   ;; had removed the name, the write above would go nowhere, and the read
   ;; would be the call that shows the problem.
   (expect-eq "the shut-down socket keeps its logical name" ?aname
              (get-socket-logical-name ?a))
   (expect-eq "and its descriptor is still live" 0 (get-timeout ?a))

   ;;=================================================================
   ;; What the end with the shutdown can no longer do.
   ;;
   ;; closed-peer.clp writes five times before it gets EPIPE, because a
   ;; reset must come back from the peer first. This test needs one write.
   ;; This program closed its own write side, and the refusal is local and
   ;; immediate.
   ;;=================================================================
   (printout ?aname "this cannot go" crlf)
   (expect-false "a write after SHUT_WR is refused" (flush-connection ?a))
   (expect-errno "with EPIPE" EPIPE)

   (close-connection ?a)
   (close-connection ?b)
   (close-connection ?srv))

;;=====================================================================
;; How poll tells a half-close from a full hangup.
;;
;; A server that watches a socket needs the two answers, and they are
;; different questions. POLLIN says that a read does not wait. A socket at the
;; end of input is always readable. As a result, a loop that reads POLLIN as
;; "there is work" continues without result on a peer that is finished.
;; POLLHUP says that the conversation is complete. POLLHUP stays clear while
;; one half is open, and that is the condition where the server still owes a
;; reply.
;;=====================================================================
(deffunction run-poll-tests ()
   (bind ?p (tcp-connected-pair ?*port-poll*))
   (bind ?srv   (nth$ 1 ?p))
   (bind ?a     (nth$ 2 ?p))
   (bind ?b     (nth$ 3 ?p))
   (bind ?aname (nth$ 4 ?p))
   (bind ?bname (nth$ 5 ?p))

   (expect-false "an open connection is not hung up" (poll ?b 100 POLLHUP))

   (shutdown-connection ?a SHUT_WR)
   (readline ?bname)

   (expect-true "a socket at end of input reads without waiting"
                (poll ?b 100 POLLIN))
   (expect-false "but is not hung up while the reply is still owed"
                 (poll ?b 100 POLLHUP))
   (expect-true "and a poll naming no flags still reports it" (poll ?b 100))
   (expect-false "the shut-down end is not hung up either"
                 (poll ?a 100 POLLHUP))

   ;; The other half now closes, and the answer changes.
   (shutdown-connection ?b SHUT_WR)
   (readline ?aname)

   (expect-true "once both directions are down, POLLHUP" (poll ?b 100 POLLHUP))

   (close-connection ?a)
   (close-connection ?b)
   (close-connection ?srv))

;;=====================================================================
;; The full sequence, as a protocol runs it.
;;
;; The steps are the request, the end of the request, the reply, the end of the
;; reply, and the close of the two ends. The sections above test the steps
;; separately. This section is here because the sequence must operate, and
;; because the examples that read to the end of input have this shape.
;;=====================================================================
(deffunction run-graceful-tests ()
   (bind ?p (tcp-connected-pair ?*port-grace*))
   (bind ?srv   (nth$ 1 ?p))
   (bind ?a     (nth$ 2 ?p))
   (bind ?b     (nth$ 3 ?p))
   (bind ?aname (nth$ 4 ?p))
   (bind ?bname (nth$ 5 ?p))

   (printout ?aname "GET /thing" crlf)
   (flush-connection ?a)
   (expect-true "client finishes sending" (shutdown-connection ?a SHUT_WR))

   (expect-eq "server reads the request" "GET /thing" (readline ?bname))
   (expect-eq "server reads the end of it" EOF (readline ?bname))

   (printout ?bname "200 here it is" crlf)
   (flush-connection ?b)
   (expect-true "server finishes replying" (shutdown-connection ?b SHUT_WR))

   (expect-eq "client reads the reply" "200 here it is" (readline ?aname))
   (expect-eq "client reads the end of it" EOF (readline ?aname))

   ;; empty-connection reads until there is no more data. On a blocking
   ;; socket that is a wait with no end, unless the peer shut down. After a
   ;; shutdown the end of input arrives immediately. As a result, this call is
   ;; safe here and only here. tls-session-io.clp needs a non-blocking socket
   ;; for the same operation.
   (expect-true "a drained half-closed connection empties" (empty-connection ?a))
   (expect-eq "with nothing left to read" -1 (get-char ?aname))

   ;; A second shutdown. A rule that fires more than one time does this, and
   ;; the second call must not damage the connection. The test does not check
   ;; the result. Linux gives TRUE, and other systems report ENOTCONN for a
   ;; direction that is already closed.
   (shutdown-connection ?a SHUT_WR)
   (expect-eq "shutting down twice changes nothing" EOF (readline ?aname))

   (close-connection ?a)
   (close-connection ?b)
   (close-connection ?srv))

;;=====================================================================
;; SHUT_RD: the code keeps the useful half.
;;
;; This is the opposite of the section above, and it is the less useful of the
;; two. A program that stops to read usually has no more data to send. The
;; important point is that a stop of the reads does not stop the writes.
;;
;; The test checks only the condition with an empty queue. The result of a read
;; with bytes already in the queue is not the same on each system. Linux gives
;; the bytes that arrived and reports the end of input after the queue is
;; empty. As a result, a test that sends first and reads after would check one
;; platform only.
;;=====================================================================
(deffunction run-shut-rd-tests ()
   (bind ?p (tcp-connected-pair ?*port-rd*))
   (bind ?srv   (nth$ 1 ?p))
   (bind ?a     (nth$ 2 ?p))
   (bind ?b     (nth$ 3 ?p))
   (bind ?aname (nth$ 4 ?p))
   (bind ?bname (nth$ 5 ?p))

   (expect-true "the read side can be shut down" (shutdown-connection ?a SHUT_RD))

   (bind ?eof (readline ?aname))
   (expect-eq "a read gives end of input rather than waiting" EOF ?eof)
   (expect-true "as the symbol" (symbolp ?eof))

   (printout ?aname "still able to send" crlf)
   (expect-true "and the write side still works" (flush-connection ?a))
   (expect-eq "with the peer receiving it" "still able to send" (readline ?bname))

   (close-connection ?a)
   (close-connection ?b)
   (close-connection ?srv))

;;=====================================================================
;; SHUT_RDWR: the two directions. This is not the same as a close.
;;
;; Each stream is finished, but this program still owns the descriptor and must
;; give it back. No other test in this suite separates the two operations.
;;=====================================================================
(deffunction run-shut-rdwr-tests ()
   (bind ?p (tcp-connected-pair ?*port-both*))
   (bind ?srv   (nth$ 1 ?p))
   (bind ?a     (nth$ 2 ?p))
   (bind ?b     (nth$ 3 ?p))
   (bind ?aname (nth$ 4 ?p))
   (bind ?bname (nth$ 5 ?p))

   (expect-true "both directions can be shut down"
                (shutdown-connection ?a SHUT_RDWR))

   (expect-eq "reads give end of input" EOF (readline ?aname))

   (printout ?aname "no chance" crlf)
   (expect-false "writes are refused" (flush-connection ?a))
   (expect-errno "with EPIPE" EPIPE)

   (expect-eq "and the peer reads end of input" EOF (readline ?bname))

   ;; A shutdown is not a close. The socket is still a router, it still has
   ;; its name, and the program must still give the descriptor back.
   (expect-eq "the socket keeps its logical name" ?aname
              (get-socket-logical-name ?a))
   (expect-eq "and its descriptor is still live" 0 (get-timeout ?a))
   (expect-true "closing it is still this program's to do" (close-connection ?a))
   (expect-false "and after that the name resolves to nothing"
                 (flush-connection ?aname))

   (close-connection ?b)
   (close-connection ?srv))

(deffunction run-tests ()
   (expect-true "SIGPIPE can be ignored" (signal SIGPIPE SIG_IGN))
   (run-shut-wr-tests)
   (run-poll-tests)
   (run-graceful-tests)
   (run-shut-rd-tests)
   (run-shut-rdwr-tests))

(run-tests)
(test-summary)
