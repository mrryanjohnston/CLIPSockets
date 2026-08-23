;;; What the three buffer modes do.
;;;
;;; Six files in this suite call set-line-buffered, set-not-buffered or
;;; set-fully-buffered. Each of them checks the same item: that the call gave
;;; TRUE. That is a test of a return value. A function that recorded the
;;; request and changed nothing would pass all six tests.
;;;
;;; The modes need more than that, because the incorrect mode is a class of
;;; server defect that looks like a network problem. A reply that goes to a
;;; fully buffered connection stays in this process until 4 KiB of data is
;;; behind it. The client then waits for an answer that the server wrote, and
;;; the server waits for the next request. The client does not send that
;;; request until it gets its answer. Nothing failed, and nothing moves again.
;;;
;;; As a result, this file tests each mode by what the peer can see. It asks
;;; the peer with (poll), which reports on the descriptor and not on any buffer
;;; in this process. Data in a buffer here did not reach the socket, and poll
;;; on the other end reports that condition.
;;;
;;; The code sets each mode on a connection that carried no data yet. C defines
;;; setvbuf only before the first read or write on a stream. As a result, a
;;; program that wants a mode must ask for it as soon as it has the socket. The
;;; last section is about that rule.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(test-suite "buffering-modes")
(test-plan 36)

(defglobal ?*base* = 19391)

;;; A connected pair on a port of its own. As a result, each section starts
;;; with a stream that no code used. Gives the listen socket, the client end
;;; and the accepted end.
(deffunction pair (?offset)
   (bind ?port (+ ?*base* ?offset))
   (bind ?pair (tcp-connected-pair ?port))
   (return (create$ (nth$ 1 ?pair) (nth$ 2 ?pair) (nth$ 3 ?pair))))

(deffunction shut (?srv ?cli ?acc)
   (close-connection ?cli)
   (close-connection ?acc)
   (close-connection ?srv))

;;; Reads ?n characters and gives them as a multifield of codes. The tests use
;;; it where there is no newline to read to, and that condition is the purpose
;;; of the no-buffer mode.
(deffunction chars (?name ?n)
   (bind ?out (create$))
   (bind ?i 0)
   (while (< ?i ?n) do
      (bind ?out (create$ ?out (get-char ?name)))
      (bind ?i (+ ?i 1)))
   (return ?out))

;;=====================================================================
;; Fully buffered, which is what a connection starts as
;;=====================================================================
;;; No code below asks for a mode. This is the behaviour that a program gets
;;; when it sets no mode, and it is the behaviour that keeps a reply in the
;;; process that wrote it.
(deffunction run-default-tests ()
   (bind ?p (pair 0))
   (bind ?srv (nth$ 1 ?p)) (bind ?cli (nth$ 2 ?p)) (bind ?acc (nth$ 3 ?p))
   (bind ?cname (get-socket-logical-name ?cli))
   (bind ?aname (get-socket-logical-name ?acc))

   (printout ?cname "half a line")
   (expect-false "by default a partial write reaches nobody"
                 (poll ?acc 100 POLLIN))

   ;; A newline is not a flush here. This is the important difference. The
   ;; same printout on a line-buffered connection already went out.
   (printout ?cname " and the rest" crlf)
   (expect-false "and a newline does not release it either"
                 (poll ?acc 100 POLLIN))

   (expect-true "an explicit flush does" (flush-connection ?cli))
   (expect-true "after which the peer sees it" (poll ?acc 1000 POLLIN))
   (expect-eq "with both halves joined and intact"
              "half a line and the rest" (readline ?aname))

   (shut ?srv ?cli ?acc))

;;=====================================================================
;; Line buffered
;;=====================================================================
(deffunction run-line-tests ()
   (bind ?p (pair 1))
   (bind ?srv (nth$ 1 ?p)) (bind ?cli (nth$ 2 ?p)) (bind ?acc (nth$ 3 ?p))
   (bind ?cname (get-socket-logical-name ?cli))
   (bind ?aname (get-socket-logical-name ?acc))

   (expect-true "line buffering is accepted" (set-line-buffered ?cli))

   (printout ?cname "a line in")
   (expect-false "a line-buffered write with no newline waits"
                 (poll ?acc 100 POLLIN))

   ;; There is no flush call in this section. The newline is the flush, and
   ;; that is the cause to select this mode for a line protocol.
   (printout ?cname " two pieces" crlf)
   (expect-true "the newline sends it with no flush asked for"
                (poll ?acc 1000 POLLIN))
   (expect-eq "and the line arrives whole"
              "a line in two pieces" (readline ?aname))

   ;; The socket stays line buffered for each line, and not only for the
   ;; first one.
   (printout ?cname "second line" crlf)
   (expect-true "later lines go the same way" (poll ?acc 1000 POLLIN))
   (expect-eq "and arrive in order" "second line" (readline ?aname))

   (shut ?srv ?cli ?acc))

;;=====================================================================
;; Not buffered
;;=====================================================================
(deffunction run-unbuffered-tests ()
   (bind ?p (pair 2))
   (bind ?srv (nth$ 1 ?p)) (bind ?cli (nth$ 2 ?p)) (bind ?acc (nth$ 3 ?p))
   (bind ?cname (get-socket-logical-name ?cli))
   (bind ?aname (get-socket-logical-name ?acc))

   (expect-true "buffering can be turned off" (set-not-buffered ?cli))

   ;; There is no newline and no flush, and the data is already on the
   ;; network. A protocol with no lines needs this mode. Examples are a length
   ;; prefix and a binary header. In such a protocol there is nothing to start
   ;; a line-buffered flush.
   (printout ?cname "abc")
   (expect-true "an unbuffered write leaves at once" (poll ?acc 1000 POLLIN))

   ;; The code reads one character at a time, because there is no newline to
   ;; read to.
   (expect-eq "and arrives byte for byte"
              (create$ 97 98 99) (chars ?aname 3))

   (printout ?cname "d")
   (expect-true "a single character goes out on its own"
                (poll ?acc 1000 POLLIN))
   (expect-eq "and is exactly what was written" 100 (get-char ?aname))

   (shut ?srv ?cli ?acc))

;;=====================================================================
;; Fully buffered, asked for
;;=====================================================================
(deffunction run-full-tests ()
   (bind ?p (pair 3))
   (bind ?srv (nth$ 1 ?p)) (bind ?cli (nth$ 2 ?p)) (bind ?acc (nth$ 3 ?p))
   (bind ?cname (get-socket-logical-name ?cli))
   (bind ?aname (get-socket-logical-name ?acc))

   (expect-true "full buffering is accepted" (set-fully-buffered ?cli))

   (printout ?cname "held back" crlf)
   (expect-false "a whole line is still held" (poll ?acc 100 POLLIN))
   (flush-connection ?cli)
   (expect-eq "until it is flushed" "held back" (readline ?aname))

   ;;=================================================================
   ;; Closing flushes what is owed
   ;;=================================================================
   ;; This behaviour hides the defect. A server that writes a reply to a fully
   ;; buffered connection and never flushes it still delivers the reply, while
   ;; it closes the connection. When that server starts to keep connections
   ;; open, the replies stop, and no one changed the code.
   (printout ?cname "written and never flushed" crlf)
   (expect-false "the last line is unflushed" (poll ?acc 100 POLLIN))
   (close-connection ?cli)
   (expect-eq "and closing sends it anyway"
              "written and never flushed" (readline ?aname))

   (close-connection ?acc)
   (close-connection ?srv))

;;=====================================================================
;; The mode belongs to the socket
;;=====================================================================
;;; Two connections to one server in one process, in different modes at the
;;; same time. Code that keeps the mode at a different location, for example in
;;; a global or in the router list, passes each section above and fails
;;; here.
(deffunction run-per-socket-tests ()
   (bind ?port (+ ?*base* 4))
   (bind ?srv (create-socket AF_INET SOCK_STREAM))
   (setsockopt ?srv SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?srv 127.0.0.1 ?port)
   (listen ?srv)

   (bind ?c1 (create-socket AF_INET SOCK_STREAM))
   (bind ?n1 (connect ?c1 127.0.0.1 ?port))
   (bind ?a1 (accept ?srv))
   (bind ?c2 (create-socket AF_INET SOCK_STREAM))
   (bind ?n2 (connect ?c2 127.0.0.1 ?port))
   (bind ?a2 (accept ?srv))

   (set-not-buffered ?c1)
   (set-fully-buffered ?c2)

   (printout ?n1 "unbuffered goes" crlf)
   (printout ?n2 "buffered stays" crlf)

   (expect-true "the unbuffered connection has already sent"
                (poll ?a1 1000 POLLIN))
   (expect-false "while the buffered one beside it has not"
                 (poll ?a2 100 POLLIN))

   (flush-connection ?c2)
   (expect-eq "each peer then reads its own"
              "unbuffered goes" (readline (get-socket-logical-name ?a1)))
   (expect-eq "and only its own"
              "buffered stays" (readline (get-socket-logical-name ?a2)))

   (close-connection ?c1) (close-connection ?c2)
   (close-connection ?a1) (close-connection ?a2)
   (close-connection ?srv))

;;=====================================================================
;; Buffering on the reading side
;;=====================================================================
;;; A program usually selects a mode for the writes, but the buffer also
;;; serves the reads. There the mode decides if (poll) gives a correct answer.
;;;
;;; A buffered read of one line takes each byte that arrived and keeps the
;;; bytes that it does not give back. As a result, the descriptor becomes quiet
;;; while a line is still in this process. mixed-poll-loop.clp is about that
;;; problem. A reader with no buffer takes only the requested bytes and leaves
;;; the remainder in the kernel, where poll can see them.
;;;
;;; This is the choice. A read with no buffer costs one system call for each
;;; character. It is also the only mode where a poll loop with one read for
;;; each readable socket is correct.
(deffunction run-read-side-tests ()
   (bind ?p (pair 5))
   (bind ?srv (nth$ 1 ?p)) (bind ?cli (nth$ 2 ?p)) (bind ?acc (nth$ 3 ?p))
   (bind ?cname (get-socket-logical-name ?cli))
   (bind ?aname (get-socket-logical-name ?acc))

   (expect-true "the reader is made unbuffered" (set-not-buffered ?acc))

   ;; The two lines go out in one write. As a result, a reader with a buffer
   ;; would take both of them and report an empty socket after the first
   ;; line.
   (printout ?cname "line one" crlf "line two" crlf)
   (flush-connection ?cli)

   (expect-eq "the first line reads back" "line one" (readline ?aname))
   (expect-true "and the descriptor still shows the second"
                (poll ?acc 1000 POLLIN))
   (expect-eq "which reads back in turn" "line two" (readline ?aname))
   (expect-false "and then the socket is genuinely empty"
                 (poll ?acc 100 POLLIN))

   (shut ?srv ?cli ?acc))

;;=====================================================================
;; Ask before the first byte
;;=====================================================================
;;; C does not define setvbuf after a read or a write on a stream. The C
;;; library can ignore the request and can report nothing. The call still gives
;;; TRUE, because it did the requested work and has nothing to check.
;;;
;;; As a result, this section does not check that a late change fails. It
;;; checks that the code does not report a late change. This is why each
;;; section above sets its mode before any data, and why a program must do the
;;; same.
(deffunction run-ordering-tests ()
   (bind ?p (pair 6))
   (bind ?srv (nth$ 1 ?p)) (bind ?cli (nth$ 2 ?p)) (bind ?acc (nth$ 3 ?p))
   (bind ?cname (get-socket-logical-name ?cli))
   (bind ?aname (get-socket-logical-name ?acc))

   (set-line-buffered ?cli)
   (printout ?cname "traffic has now moved" crlf)
   (expect-eq "the connection has carried a line"
              "traffic has now moved" (readline ?aname))

   (expect-true "a mode asked for afterwards is still answered TRUE"
                (set-fully-buffered ?cli))

   ;; The connection still operates, in whichever mode it has. A write and
   ;; then a flush arrives in each mode. This is why a program that calls a
   ;; flush itself does not depend on the mode.
   (printout ?cname "still working" crlf)
   (flush-connection ?cli)
   (expect-eq "and the connection carries on regardless"
              "still working" (readline ?aname))

   (shut ?srv ?cli ?acc))

;;=====================================================================
;; The setters take a name as well as a descriptor
;;=====================================================================
(deffunction run-name-tests ()
   (bind ?p (pair 7))
   (bind ?srv (nth$ 1 ?p)) (bind ?cli (nth$ 2 ?p)) (bind ?acc (nth$ 3 ?p))
   (bind ?cname (get-socket-logical-name ?cli))
   (bind ?aname (get-socket-logical-name ?acc))

   (expect-true "a mode can be set by logical name"
                (set-line-buffered ?cname))

   ;; The call also reached the stream that the name gives. The code did not
   ;; accept the call and then discard it.
   (printout ?cname "set by name" crlf)
   (expect-true "and it took effect on that socket" (poll ?acc 1000 POLLIN))
   (expect-eq "with the line intact" "set by name" (readline ?aname))

   (shut ?srv ?cli ?acc))

(run-default-tests)
(run-line-tests)
(run-unbuffered-tests)
(run-full-tests)
(run-per-socket-tests)
(run-read-side-tests)
(run-ordering-tests)
(run-name-tests)
(test-summary)
