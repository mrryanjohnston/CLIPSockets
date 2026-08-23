;;; The code gives each descriptor back.
;;;
;;; No other test here can see a descriptor that the code did not close. No
;;; function gives an incorrect answer, no code stops, and AddressSanitizer
;;; reports nothing. An open descriptor is not a lost pointer, and the memory
;;; behind it is still available. The process collects descriptors until it
;;; reaches its limit. A test that opens twelve sockets never reaches that
;;; limit. A server that operates for one week does reach it.
;;;
;;; As a result, this file reads the count directly. /proc/self/fd has one
;;; entry for each open descriptor, and (scandir) lists the entries. The check
;;; is the length of that list before and after some hundred cycles. The test
;;; uses exact equality and not a limit. A loss of one descriptor for each
;;; connection and a loss of one for each one thousand connections are the same
;;; defect, and a threshold finds only one of them.
;;;
;;; This works on Linux only, and this library is for Linux. If /proc is not
;;; present, the count is zero and the test skips the checks that need it. The
;;; test does not fail, because an absent /proc says nothing about
;;; descriptors.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(test-suite "descriptor-leaks")
(test-plan 13)

(defglobal ?*port*  = 19341)
(defglobal ?*dead*  = 19342)
(defglobal ?*udp-a* = 19343)
(defglobal ?*udp-b* = 19344)
(defglobal ?*path*  = "/tmp/clipsockets-test-leaks.sock")

(deffunction have-proc ()
   (return (> (count-open-descriptors) 0)))

;;; One full connection: the listen socket, the client and the accepted end.
;;; The function closes each of them.
(deffunction connect-cycle (?port)
   (bind ?pair (tcp-connected-pair ?port))
   (bind ?srv (nth$ 1 ?pair))
   (bind ?cli (nth$ 2 ?pair))
   (bind ?acc (nth$ 3 ?pair))
   ;; The function sends data. As a result, the cycle covers a connection
   ;; that carried data and not only a connection that opened. A descriptor
   ;; that a full buffer holds is a different loss from a descriptor that a
   ;; missing close holds.
   (printout (get-socket-logical-name ?cli) "traffic" crlf)
   (flush-connection ?cli)
   (readline (get-socket-logical-name ?acc))
   (close-connection ?cli)
   (close-connection ?acc)
   (close-connection ?srv))

(deffunction run-tests ()
   (if (not (have-proc)) then
      ;; Each check in this file needs the count. As a result, there is one
      ;; skip and not thirteen. The plan must also change to one. At 13 the
      ;; runner would report a file that skipped correctly as a file that
      ;; stopped in the middle.
      (test-plan 1)
      (test-skip "descriptor accounting" "/proc/self/fd is not readable here")
      (return))

   ;;=================================================================
   ;; The measurement itself
   ;;=================================================================
   (bind ?base (count-open-descriptors))
   (expect-eq "the count is steady when nothing happens" ?base (count-open-descriptors))

   (bind ?s (create-socket AF_INET SOCK_STREAM))
   (expect-eq "an open socket is one more" (+ ?base 1) (count-open-descriptors))
   (close-connection ?s)
   (expect-eq "and closing it gives the descriptor back" ?base (count-open-descriptors))

   ;;=================================================================
   ;; Connections, in bulk
   ;;=================================================================
   (bind ?i 0)
   (while (< ?i 200) do
      (connect-cycle ?*port*)
      (bind ?i (+ ?i 1)))
   (expect-eq "200 full connections leak nothing" ?base (count-open-descriptors))

   ;;=================================================================
   ;; Closing by logical name rather than by descriptor
   ;;=================================================================
   ;; This is a different path through the router list, and a loss is more
   ;; probable here. The code must find the router for the name before it can
   ;; close the socket. If that search fails in the middle, the descriptor can
   ;; stay open while the router is already out of the list.
   (bind ?j 0)
   (while (< ?j 50) do
      (bind ?pair (tcp-connected-pair ?*port*))
      (bind ?srv (nth$ 1 ?pair))
      (bind ?acc (nth$ 3 ?pair))
      (bind ?cname (nth$ 4 ?pair))
      (close-connection ?cname)
      (close-connection (get-socket-logical-name ?acc))
      (close-connection (get-socket-logical-name ?srv))
      (bind ?j (+ ?j 1)))
   (expect-eq "closing by name leaks nothing" ?base (count-open-descriptors))

   ;;=================================================================
   ;; Many connections open at once
   ;;=================================================================
   ;; The loop above never has more than three open descriptors. As a result,
   ;; a router list with incorrect links between several entries would pass
   ;; that loop.
   (bind ?srv (create-socket AF_INET SOCK_STREAM))
   (setsockopt ?srv SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?srv 127.0.0.1 ?*port*)
   (listen ?srv)

   (bind ?clients (create$))
   (bind ?accepted (create$))
   (bind ?k 0)
   (while (< ?k 30) do
      (bind ?c (create-socket AF_INET SOCK_STREAM))
      (connect ?c 127.0.0.1 ?*port*)
      (bind ?clients (create$ ?clients ?c))
      (bind ?accepted (create$ ?accepted (accept ?srv)))
      (bind ?k (+ ?k 1)))

   (expect-eq "30 open connections account for 61 descriptors"
              (+ ?base 61) (count-open-descriptors))

   ;; The code closes them in a sequence that is different from the sequence
   ;; of the opens. A list of routers with one link for each entry is most
   ;; likely to lose an entry in that condition.
   (bind ?k 30)
   (while (> ?k 0) do
      (close-connection (nth$ ?k ?clients))
      (bind ?k (- ?k 1)))
   (bind ?k 1)
   (while (<= ?k 30) do
      (close-connection (nth$ ?k ?accepted))
      (bind ?k (+ ?k 1)))
   (close-connection ?srv)
   (expect-eq "closing 30 out of order leaks nothing" ?base (count-open-descriptors))

   ;;=================================================================
   ;; Calls that fail
   ;;=================================================================
   ;; A loss hides in this path. A function that opens a descriptor and then
   ;; fails at the next step must close that descriptor. No other check here
   ;; covers a failure.
   (bind ?m 0)
   (while (< ?m 50) do
      (bind ?c (create-socket AF_INET SOCK_STREAM))
      ;; No socket listens on this port, and the connect call fails.
      (connect ?c 127.0.0.1 ?*dead*)
      (close-connection ?c)
      (bind ?m (+ ?m 1)))
   (expect-eq "50 refused connections leak nothing" ?base (count-open-descriptors))

   ;; A socket that the code opens and closes, with no other operation.
   (bind ?n 0)
   (while (< ?n 100) do
      (bind ?c (create-socket AF_INET SOCK_STREAM))
      (close-connection ?c)
      (bind ?n (+ ?n 1)))
   (expect-eq "100 unused sockets leak nothing" ?base (count-open-descriptors))

   ;;=================================================================
   ;; The other socket kinds
   ;;=================================================================
   (bind ?p 0)
   (while (< ?p 50) do
      (bind ?a (create-socket AF_INET SOCK_DGRAM))
      (setsockopt ?a SOL_SOCKET SO_REUSEADDR 1)
      (bind-socket ?a 127.0.0.1 ?*udp-a*)
      (bind ?b (create-socket AF_INET SOCK_DGRAM))
      (setsockopt ?b SOL_SOCKET SO_REUSEADDR 1)
      (bind-socket ?b 127.0.0.1 ?*udp-b*)
      (sendto ?a AF_INET "127.0.0.1" ?*udp-b* "datagram")
      (rcvfrom ?b)
      (close-connection ?a)
      (close-connection ?b)
      (bind ?p (+ ?p 1)))
   (expect-eq "50 UDP pairs leak nothing" ?base (count-open-descriptors))

   (bind ?q 0)
   (while (< ?q 50) do
      (remove ?*path*)
      (bind ?srv2 (create-socket AF_UNIX SOCK_STREAM))
      (bind-socket ?srv2 /tmp/clipsockets-test-leaks.sock)
      (listen ?srv2)
      (bind ?cli2 (create-socket AF_UNIX SOCK_STREAM))
      (connect ?cli2 /tmp/clipsockets-test-leaks.sock)
      (bind ?acc2 (accept ?srv2))
      (close-connection ?cli2)
      (close-connection ?acc2)
      (close-connection ?srv2)
      (bind ?q (+ ?q 1)))
   (remove ?*path*)
   (expect-eq "50 unix connections leak nothing" ?base (count-open-descriptors))

   ;;=================================================================
   ;; Half close
   ;;=================================================================
   ;; A shutdown does not free a descriptor. As a result, the code must still
   ;; close the socket after a shutdown. A close that decided that the socket
   ;; was already gone would lose the descriptor here.
   (bind ?r 0)
   (while (< ?r 50) do
      (bind ?srv3 (create-socket AF_INET SOCK_STREAM))
      (setsockopt ?srv3 SOL_SOCKET SO_REUSEADDR 1)
      (bind-socket ?srv3 127.0.0.1 ?*port*)
      (listen ?srv3)
      (bind ?cli3 (create-socket AF_INET SOCK_STREAM))
      (connect ?cli3 127.0.0.1 ?*port*)
      (bind ?acc3 (accept ?srv3))
      (shutdown-connection ?cli3 SHUT_WR)
      (close-connection ?cli3)
      (close-connection ?acc3)
      (close-connection ?srv3)
      (bind ?r (+ ?r 1)))
   (expect-eq "50 half-closed connections leak nothing" ?base (count-open-descriptors))

   ;;=================================================================
   ;; Everything at once, once more
   ;;=================================================================
   ;; The count was equal to the first value at each step above. As a result,
   ;; a loss that starts only after much work would show in the section after
   ;; it. This check makes the same statement one time at the end, where it
   ;; names the file and not one section.
   (expect-eq "the process ends with the descriptors it started with"
              ?base (count-open-descriptors)))

(run-tests)
(test-summary)
