;;; One poll loop over four types of socket at the same time.
;;;
;;; poll-events.clp gives the meaning of each event symbol on one TCP
;;; connection. That file is the vocabulary. This file is the sentence. A
;;; server does not watch one socket. It watches each socket that it owns in
;;; one loop, and it uses the readable descriptor to select the necessary work.
;;; The listen socket, the accepted connections, a UDP socket and a unix socket
;;; are all in the same set here. Each of them needs a different call after it
;;; reports POLLIN.
;;;
;;; POLLIN does not have the same meaning on each of them, and that is the
;;; purpose of this file:
;;;
;;;   on a listen socket      a connection waits for an accept call, and there
;;;                           is nothing to read. A readline here would block
;;;                           on a socket that carries no data
;;;   on a connection         bytes are available to read
;;;   on a datagram socket    one complete datagram is in the queue, and the
;;;                           code must take it with rcvfrom and not read it
;;;                           as a stream
;;;
;;; As a result, a dispatcher must keep the type of each descriptor with the
;;; descriptor. The loop below keeps two parallel multifields for this, and it
;;; adds to both of them when it accepts a connection. A test with one socket
;;; cannot reach that condition: a descriptor that enters the set while the
;;; code reads the set.
;;;
;;; The first section is about the problem that decides the shape of the loop.
;;; Read that section before the loop.
;;;
;;; The code queues each message before the loop starts. The two ends of each
;;; connection are in this process. As a result, no peer can wait for the loop
;;; that waits for that peer.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(test-suite "mixed-poll-loop")
(test-plan 30)

(defglobal ?*tcp-port*  = 19381)
(defglobal ?*udp-port*  = 19382)
(defglobal ?*udp-from*  = 19383)
(defglobal ?*demo-port* = 19384)
(defglobal ?*path*      = "/tmp/clipsockets-test-mixed.sock")

;;; The number of events of each type that the loop must find before its
;;; budget ends.
(defglobal ?*want-accept* = 2)
(defglobal ?*want-tcp*    = 3)
(defglobal ?*want-udp*    = 3)
(defglobal ?*want-unix*   = 2)

;;; Reads each line that a stream socket has. It stops and does not block when
;;; there are no more lines.
;;;
;;; The timeout ends the loop. A read with no data waits on a socket whose peer
;;; is still open. The peers here are in this process and cannot close until
;;; the loop completes its work. With a timeout the read stops and gives EOF. A
;;; peer that closed gives the same answer. read-timeouts.clp tells the two
;;; conditions apart with errno. Here the code does not need that difference,
;;; because in both conditions the socket has no more data now.
(deffunction drain (?fd)
   (set-timeout ?fd 100000)
   (bind ?name (get-socket-logical-name ?fd))
   (bind ?lines (create$))
   (bind ?line (readline ?name))
   (while (neq ?line EOF) do
      (bind ?lines (create$ ?lines ?line))
      (bind ?line (readline ?name)))
   (return ?lines))

;;; Counts the entries of a multifield that are in the multifield more than
;;; one time.
(deffunction duplicates ($?mf)
   (bind ?n 0)
   (bind ?i 1)
   (while (<= ?i (length$ ?mf)) do
      (bind ?j (+ ?i 1))
      (while (<= ?j (length$ ?mf)) do
         (if (eq (nth$ ?i ?mf) (nth$ ?j ?mf)) then (bind ?n (+ ?n 1)))
         (bind ?j (+ ?j 1)))
      (bind ?i (+ ?i 1)))
   (return ?n))

;;=====================================================================
;; Why the loop reads each available line
;;=====================================================================
;;; poll(2) reports on a descriptor. readline reads through a buffer above
;;; that descriptor, and the code fills that buffer in blocks. A read of one
;;; line takes each byte that arrived, gives back the first line and keeps the
;;; remainder. The kernel is then empty and poll reports that condition, but a
;;; complete line is already in this process and waits for a read.
;;;
;;; As a result, a loop with one read for each readable socket stops and holds
;;; the answer. It stops on the traffic that makes a server busy: two requests
;;; that arrive close together and that one read takes. Such a loop looks
;;; correct in a test that sends one line at a time.
;;;
;;; This is the plaintext form of the value that tls-pending gives for a TLS
;;; session. There the library knows how much data it holds, and (poll)
;;; includes that data. On a plain socket no code can ask stdio the same
;;; question. As a result, the handler must read each available line and must
;;; not wait for a second report.
(deffunction run-buffering-trap-tests ()
   (bind ?pair (tcp-connected-pair ?*demo-port*))
   (bind ?srv (nth$ 1 ?pair))
   (bind ?cli (nth$ 2 ?pair))
   (bind ?acc (nth$ 3 ?pair))
   (bind ?cname (nth$ 4 ?pair))
   (bind ?aname (nth$ 5 ?pair))

   ;; Two lines, one write and one flush. They reach the kernel together.
   (printout ?cname "first request" crlf "second request" crlf)
   (flush-connection ?cli)

   (expect-true "the socket reports both lines as readable"
                (poll ?acc 1000 POLLIN))
   (expect-eq "the first line is read" "first request" (readline ?aname))

   ;; The problem, as a check. No data is lost and nothing failed, and one
   ;; more call gives the second line. But the descriptor has no data, and a
   ;; loop that uses only poll does not make that call.
   (expect-false "yet the descriptor now looks quiet" (poll ?acc 0 POLLIN))
   (expect-eq "while the second line was in the buffer all along"
              "second request" (readline ?aname))

   (close-connection ?cli)
   (close-connection ?acc)
   (close-connection ?srv))

;;=====================================================================
;; The loop
;;=====================================================================
(deffunction run-loop-tests ()
   (bind ?lsn (create-socket AF_INET SOCK_STREAM))
   (setsockopt ?lsn SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?lsn 127.0.0.1 ?*tcp-port*)
   (expect-true "the TCP listener is listening" (listen ?lsn))

   (bind ?udp (create-socket AF_INET SOCK_DGRAM))
   (setsockopt ?udp SOL_SOCKET SO_REUSEADDR 1)
   (expect-true "the UDP socket binds"
                (bind-socket ?udp 127.0.0.1 ?*udp-port*))

   (remove ?*path*)
   (bind ?usrv (create-socket AF_UNIX SOCK_STREAM))
   (bind-socket ?usrv /tmp/clipsockets-test-mixed.sock)
   (expect-true "the unix listener is listening" (listen ?usrv))

   ;; The code makes the unix pair before the loop, and the loop watches only
   ;; its client end. As a result, the loop has a unix socket with a stream on
   ;; it, and it needs no second accept path.
   (bind ?ucli (create-socket AF_UNIX SOCK_STREAM))
   (bind ?uname (connect ?ucli /tmp/clipsockets-test-mixed.sock))
   (bind ?uacc (accept ?usrv))

   ;; Two TCP clients connect and send data. This code does not accept them.
   ;; The loop must see the listen socket and accept them itself. The second
   ;; client sends two lines together, which is the condition that the section
   ;; above describes.
   (bind ?t1 (create-socket AF_INET SOCK_STREAM))
   (bind ?tn1 (connect ?t1 127.0.0.1 ?*tcp-port*))
   (bind ?t2 (create-socket AF_INET SOCK_STREAM))
   (bind ?tn2 (connect ?t2 127.0.0.1 ?*tcp-port*))
   (printout ?tn1 "tcp from one" crlf)
   (printout ?tn2 "tcp from two" crlf "tcp from two again" crlf)
   (flush-connection ?t1)
   (flush-connection ?t2)

   ;; Three datagrams from a sender of their own. Each datagram is one message
   ;; with its own limits. As a result, the code never joins them, and the two
   ;; TCP lines above did join.
   (bind ?sender (create-socket AF_INET SOCK_DGRAM))
   (setsockopt ?sender SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?sender 127.0.0.1 ?*udp-from*)
   (sendto ?sender AF_INET "127.0.0.1" ?*udp-port* "datagram one")
   (sendto ?sender AF_INET "127.0.0.1" ?*udp-port* "datagram two")
   (sendto ?sender AF_INET "127.0.0.1" ?*udp-port* "datagram three")

   ;; Two lines on the unix connection, in one flush.
   (printout (get-socket-logical-name ?uacc)
             "unix line one" crlf "unix line two" crlf)
   (flush-connection ?uacc)

   ;; The listen socket reports that a connection waits. It reports nothing
   ;; else, because a listen socket carries no data and has nothing to read.
   (expect-true "the listener reports a waiting connection"
                (poll ?lsn 1000 POLLIN))

   (bind ?watch (create$ ?lsn ?udp ?ucli))
   (bind ?kind  (create$ listener udp unix))

   (bind ?accepted  0)
   (bind ?tcp-got   (create$))
   (bind ?udp-got   (create$))
   (bind ?unix-got  (create$))
   (bind ?passes    0)
   (bind ?done      FALSE)

   (while (and (< ?passes 100) (not ?done)) do
      (bind ?worked FALSE)
      (bind ?k 1)
      ;; The code reads the length in each cycle and not one time. An accept
      ;; call adds to the set that the loop reads, and the loop must reach the
      ;; new connection in this cycle and in each cycle after it.
      (while (<= ?k (length$ ?watch)) do
         (bind ?fd (nth$ ?k ?watch))
         (if (poll ?fd 0 POLLIN) then
            (bind ?worked TRUE)
            (switch (nth$ ?k ?kind)
               (case listener then
                  ;; This is the one type where POLLIN is not about data. A
                  ;; read here would block on a socket that carries no
                  ;; data.
                  (bind ?new (accept ?fd))
                  (bind ?watch (create$ ?watch ?new))
                  (bind ?kind (create$ ?kind tcp))
                  (bind ?accepted (+ ?accepted 1)))
               (case tcp then
                  (bind ?tcp-got (create$ ?tcp-got (drain ?fd))))
               (case udp then
                  ;; A datagram socket is not a stream. It is the one type
                  ;; here that needs no loop. One datagram in the queue is one
                  ;; rcvfrom call, and poll reports the next datagram
                  ;; separately. The data is the last field of the answer,
                  ;; whatever fields the family puts before it.
                  (bind ?mf (rcvfrom ?fd))
                  (bind ?udp-got
                        (create$ ?udp-got (nth$ (length$ ?mf) ?mf))))
               (case unix then
                  (bind ?unix-got (create$ ?unix-got (drain ?fd))))))
         (bind ?k (+ ?k 1)))

      (if (and (= ?accepted ?*want-accept*)
               (= (length$ ?tcp-got) ?*want-tcp*)
               (= (length$ ?udp-got) ?*want-udp*)
               (= (length$ ?unix-got) ?*want-unix*))
         then (bind ?done TRUE))

      ;; No socket was ready. The code waits on the kernel and does not use
      ;; its budget with no result. On loopback, with each message already
      ;; sent, the code usually does not reach this point. It prevents a
      ;; timeout of the file when an event is absent, and it gives a failure
      ;; instead.
      (if (and (not ?worked) (not ?done)) then (poll ?lsn 20 POLLIN))
      (bind ?passes (+ ?passes 1)))

   ;;=================================================================
   ;; What the loop found
   ;;=================================================================
   (expect-true "the loop finished on its own" ?done)
   (expect-lte "and did not need its whole budget" 99 ?passes)

   (expect-eq "both waiting connections were accepted"
              ?*want-accept* ?accepted)
   (expect-eq "the watch set grew by one per accept" 5 (length$ ?watch))
   (expect-eq "and the kinds stayed alongside it" 5 (length$ ?kind))

   ;; Each source delivered its own data, and no data went to the handler of
   ;; a different type.
   (expect-length "three TCP lines were read" 3 ?tcp-got)
   (expect-contains "the first client's line arrived" "tcp from one" ?tcp-got)
   (expect-contains "the second client's first line arrived"
                    "tcp from two" ?tcp-got)
   (expect-contains "and its second line, from the same read"
                    "tcp from two again" ?tcp-got)

   (expect-length "three datagrams were taken" 3 ?udp-got)
   (expect-contains "the first datagram arrived" "datagram one" ?udp-got)
   (expect-contains "the second datagram arrived" "datagram two" ?udp-got)
   (expect-contains "the third datagram arrived" "datagram three" ?udp-got)

   (expect-length "two unix lines were read" 2 ?unix-got)
   (expect-eq "the unix lines arrived in order"
              "unix line one" (nth$ 1 ?unix-got))
   (expect-eq "and in full" "unix line two" (nth$ 2 ?unix-got))

   ;;=================================================================
   ;; The set is quiet afterwards
   ;;=================================================================
   ;; The handler for each type read each byte from its own descriptors. A
   ;; socket that still reports POLLIN would show that the incorrect handler
   ;; read some data. Examples are a datagram that is not complete and a line
   ;; that stays in the buffer. The counts above cannot tell such a run from a
   ;; correct run.
   (bind ?still 0)
   (bind ?k 1)
   (while (<= ?k (length$ ?watch)) do
      (if (poll (nth$ ?k ?watch) 0 POLLIN) then (bind ?still (+ ?still 1)))
      (bind ?k (+ ?k 1)))
   (expect-eq "nothing in the set is still readable" 0 ?still)

   (expect-false "and no connection is waiting on the listener"
                 (poll ?lsn 0 POLLIN))

   ;;=================================================================
   ;; Four families, five names, no collisions
   ;;=================================================================
   ;; The loop used the name of each stream socket. If two sockets of
   ;; different families had the same name, the data of one peer would go to
   ;; the other peer. The counts above would still be correct.
   (bind ?names (create$))
   (bind ?k 1)
   (while (<= ?k (length$ ?watch)) do
      (bind ?names (create$ ?names (get-socket-logical-name (nth$ ?k ?watch))))
      (bind ?k (+ ?k 1)))
   (expect-eq "every watched socket has a name" 5 (length$ ?names))
   (expect-eq "and no two of them are the same" 0 (duplicates ?names))

   ;; Each name keeps the form of its family, in one process at one time.
   (expect-eq "the UDP socket is named for the address it bound"
              (sym-cat "127.0.0.1:" ?*udp-port*)
              (get-socket-logical-name ?udp))
   (expect-eq "the unix connection is named for the path it reached"
              1 (str-index "/tmp/clipsockets-test-mixed.sock#" ?uname))

   ;;=================================================================
   ;; Teardown
   ;;=================================================================
   (bind ?k 1)
   (while (<= ?k (length$ ?watch)) do
      (close-connection (nth$ ?k ?watch))
      (bind ?k (+ ?k 1)))
   (close-connection ?t1)
   (close-connection ?t2)
   (close-connection ?uacc)
   (close-connection ?usrv)
   (close-connection ?sender)
   (remove ?*path*))

(run-buffering-trap-tests)
(run-loop-tests)
(test-summary)
