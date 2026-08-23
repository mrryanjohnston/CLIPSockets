;;; (poll) argument forms and every event symbol it accepts.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(test-suite "poll-events")
(test-plan 12)

(defglobal ?*port* = 18920)

(deffunction run-tests ()
   (bind ?pair (tcp-connected-pair ?*port*))
   (bind ?srv (nth$ 1 ?pair))
   (bind ?cli (nth$ 2 ?pair))
   (bind ?cfd (nth$ 3 ?pair))
   (bind ?conn (nth$ 4 ?pair))
   (bind ?cname (nth$ 5 ?pair))

   ;; With no event named, poll watches for anything at all. An idle socket has
   ;; nothing readable but is writable, so the two-argument form reports TRUE.
   (expect-true "two-argument form polls for any event" (poll ?cfd 100))

   ;; A freshly connected socket is writable and not yet readable.
   (expect-true  "POLLOUT on an idle connection" (poll ?cfd 100 POLLOUT))
   (expect-false "POLLIN with nothing sent yet" (poll ?cfd 100 POLLIN))

   ;; Error-ish events do not fire on a healthy socket.
   (expect-false "POLLERR on a healthy socket" (poll ?cfd 100 POLLERR))
   (expect-false "POLLHUP on a healthy socket" (poll ?cfd 100 POLLHUP))
   (expect-false "POLLNVAL on a healthy socket" (poll ?cfd 100 POLLNVAL))
   (expect-false "POLLPRI with no urgent data" (poll ?cfd 100 POLLPRI))

   ;; Several events at once, and by logical name rather than descriptor.
   (expect-true "POLLIN and POLLOUT together" (poll ?cfd 100 POLLIN POLLOUT))
   (expect-true "polling by logical name" (poll ?cname 100 POLLOUT))

   ;; Once the peer writes, the socket becomes readable.
   (printout ?conn "wake up" crlf)
   (flush-connection ?cli)
   (expect-true "POLLIN after the peer writes" (poll ?cfd 5000 POLLIN))

   ;; And once the peer goes away, the connection hangs up.
   (close-connection ?cli)
   (expect-true "POLLHUP after the peer closes" (poll ?cfd 5000 POLLIN POLLHUP))

   (expect-false "an unknown event symbol is rejected" (poll ?cfd 100 NOT_AN_EVENT))

   (close-connection ?cfd)
   (close-connection ?srv))

(run-tests)
(test-summary)
