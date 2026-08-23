;;; A server whose full loop is a set of rules.
;;;
;;; Each other test in this suite calls the socket functions in sequence from a
;;; deffunction. That is not the purpose of this library. Sockets are in CLIPS
;;; so that the rules do the work. A descriptor goes in a fact, a rule matches
;;; the condition of that fact, and the engine selects the next action.
;;; examples/server-complex.clp has that shape, and no test covers it.
;;;
;;; The difference is important. The right side of a rule runs inside (run),
;;; which is inside the control loop of the engine. Each item that the socket
;;; functions use comes from there and not from the top level. Those items are
;;; the router list, the logical names and the buffers behind them. A logical
;;; name is a string in a fact slot, and that string must still address the
;;; correct socket several rule firings after the call that made it returns.
;;;
;;; The server stops by itself. It serves the number of clients that the test
;;; gives it, and it then has no activations left. As a result, (run) returns
;;; because there is no more work, and not because other code stopped it. The
;;; facts hold the count of the work. (run) gives no value here, and there is
;;; no count of firings to check.
;;;
;;; Each client connects and sends before the code calls (run). A real server
;;; does not meet its clients in this manner. But it is the only method to put
;;; the two ends of each connection in one process, and to also prevent a block
;;; of the engine on a peer that waits for the engine.

(load* "tests/lib/expect.clp")
(test-suite "rule-driven-server")
(test-plan 21)

(defglobal ?*port*    = 19361)
(defglobal ?*clients* = 3)

;;; The listen socket, and the quantity of its work that is complete.
(deftemplate server
   (slot fd)
   (slot name)
   (slot expected)
   (slot accepted (default 0))
   (slot served   (default 0)))

;;; One accepted connection. It moves from accepted, to answered, to
;;; closed.
(deftemplate conn
   (slot fd)
   (slot name)
   (slot request (default nil))
   (slot replied (default nil)))

;;; The request that each client sent. The test compares the replies with
;;; it.
(deftemplate expected-reply
   (slot request)
   (slot reply))

;;=====================================================================
;; The server, as rules
;;=====================================================================

;;; Takes the next client, while the server expects more clients.
;;;
;;; The counter in the fact lets this rule fire more than one time. A modify of
;;; the server fact retracts the fact and asserts it again. As a result, the
;;; pattern matches the new value and the engine activates the rule again.
;;; Without a slot that changes, the engine uses the activation at the first
;;; firing.
(defrule accept-a-client
   ?s <- (server (fd ?fd) (expected ?want) (accepted ?got))
   (test (< ?got ?want))
   =>
   (bind ?cfd (accept ?fd))
   ;; The code gets the name here and keeps it. Other rules then use it in
   ;; later firings. No code gets the name from the descriptor again.
   (assert (conn (fd ?cfd) (name (get-socket-logical-name ?cfd))))
   (modify ?s (accepted (+ ?got 1))))

;;; Reads the request from a connection that gave no request yet.
(defrule read-a-request
   ?c <- (conn (fd ?fd) (name ?name) (request nil))
   =>
   ;; Without a timeout, a client that sent nothing would stop the engine
   ;; here. The suite would then report a timeout of the file and not a
   ;; failure. With a timeout, the read stops, the request slot gets the
   ;; symbol EOF, and the checks at the end report the problem.
   ;;
   ;; The call gives seconds and microseconds. The form with one argument
   ;; takes microseconds, and two seconds in that form is a long number that
   ;; is easy to misread.
   (set-timeout ?fd 2 0)
   (modify ?c (request (readline ?name))))

;;; Sends the reply.
(defrule send-a-reply
   ?c <- (conn (name ?name) (fd ?fd) (request ?req&~nil) (replied nil))
   =>
   ;; The reply comes from the request. As a result, a reply on the incorrect
   ;; connection is a different string, and the test can see the error.
   (printout ?name "reply to " ?req crlf)
   (flush-connection ?fd)
   (modify ?c (replied TRUE)))

;;; Closes the connection and adds it to the count of served clients.
(defrule close-a-served-connection
   ?c <- (conn (fd ?fd) (replied TRUE))
   ?s <- (server (served ?n))
   =>
   (close-connection ?fd)
   (retract ?c)
   (modify ?s (served (+ ?n 1))))

;;=====================================================================
;; Driving it
;;=====================================================================

;;; Connects ?n clients, and each client sends a request of its own.
;;;
;;; Gives the client descriptors. Each of them stays open, and the test can
;;; read the replies after the engine stops.
(deffunction start-clients (?n)
   (bind ?fds (create$))
   (bind ?i 1)
   (while (<= ?i ?n) do
      (bind ?c (create-socket AF_INET SOCK_STREAM))
      (bind ?name (connect ?c 127.0.0.1 ?*port*))
      (bind ?request (str-cat "request from client " ?i))
      (printout ?name ?request crlf)
      (flush-connection ?c)
      (assert (expected-reply (request ?request)
                              (reply (str-cat "reply to " ?request))))
      (bind ?fds (create$ ?fds ?c))
      (bind ?i (+ ?i 1)))
   (return ?fds))

(deffunction the-server ()
   (return (nth$ 1 (find-all-facts ((?f server)) TRUE))))

(deffunction run-tests ()
   ;;=================================================================
   ;; Set up outside the engine
   ;;=================================================================
   (bind ?srv (create-socket AF_INET SOCK_STREAM))
   (setsockopt ?srv SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?srv 127.0.0.1 ?*port*)
   (expect-true "the listening socket is listening" (listen ?srv))

   (bind ?clients (start-clients ?*clients*))
   (expect-eq "every client connected" ?*clients* (length$ ?clients))
   (expect-eq "and each one has a reply waiting to be checked"
              ?*clients* (length$ (find-all-facts ((?f expected-reply)) TRUE)))

   ;; No rule ran yet. Each rule above waits for a server fact, and that fact
   ;; does not exist.
   (expect-eq "no connections before the engine runs"
              0 (length$ (find-all-facts ((?f conn)) TRUE)))

   ;;=================================================================
   ;; Hand it to the engine
   ;;=================================================================
   (assert (server (fd ?srv)
                   (name (get-socket-logical-name ?srv))
                   (expected ?*clients*)))

   (run)

   ;;=================================================================
   ;; What the engine did
   ;;=================================================================
   ;; The counts are in the facts. (run) gives no value here, and the test
   ;; cannot check it directly. The counters give the same data, and they give
   ;; it for each rule. Only the accept rule increases accepted, and only the
   ;; close rule increases served. As a result, when both counters reach the
   ;; number of clients, each stage ran for each client.
   (bind ?s (the-server))
   (expect-eq "the server accepted every client"
              ?*clients* (fact-slot-value ?s accepted))
   (expect-eq "and served every one of them"
              ?*clients* (fact-slot-value ?s served))

   ;; The rule that closed each connection also retracted its fact. As a
   ;; result, the engine left no fact behind.
   (expect-eq "no connection facts are left"
              0 (length$ (find-all-facts ((?f conn)) TRUE)))

   ;; The engine stopped because there was no more work, and not because
   ;; other code stopped it. A second (run) call shows this. An engine with an
   ;; activation left would serve a client, and the counters would change.
   (run)
   (expect-eq "running again serves nobody new"
              ?*clients* (fact-slot-value (the-server) served))
   (expect-eq "and makes no new connections"
              0 (length$ (find-all-facts ((?f conn)) TRUE)))

   ;;=================================================================
   ;; What reached the wire
   ;;=================================================================
   ;; The rules wrote the replies during their firings, to names from fact
   ;; slots. Each client reads its own reply.
   (bind ?i 1)
   (while (<= ?i ?*clients*) do
      (bind ?fd (nth$ ?i ?clients))
      (bind ?want (str-cat "reply to request from client " ?i))
      (expect-eq (str-cat "client " ?i " got its own reply")
                 ?want (readline (get-socket-logical-name ?fd)))
      (bind ?i (+ ?i 1)))

   ;; The server also closed its end, and each client sees the end of
   ;; input.
   (bind ?j 1)
   (while (<= ?j ?*clients*) do
      (bind ?fd (nth$ ?j ?clients))
      (expect-eq (str-cat "client " ?j " sees the server close")
                 EOF (readline (get-socket-logical-name ?fd)))
      (bind ?j (+ ?j 1)))

   ;;=================================================================
   ;; A second round on the same listening socket
   ;;=================================================================
   ;; The engine is idle, and the server fact is still present with its
   ;; counters at their limits. An increase of the expected number activates
   ;; the accept rule again. A server that operates for a long time is in this
   ;; condition each time a client arrives after a quiet period.
   (bind ?k 1)
   (while (<= ?k ?*clients*) do
      (close-connection (nth$ ?k ?clients))
      (bind ?k (+ ?k 1)))

   (bind ?more (start-clients 2))
   (modify (the-server) (expected (+ ?*clients* 2)))

   (run)

   (bind ?s2 (the-server))
   (expect-eq "the server accepted the new clients too"
              (+ ?*clients* 2) (fact-slot-value ?s2 accepted))
   (expect-eq "and served them" (+ ?*clients* 2) (fact-slot-value ?s2 served))
   (expect-eq "leaving no connection facts again"
              0 (length$ (find-all-facts ((?f conn)) TRUE)))

   (expect-eq "the fourth client got its own reply"
              "reply to request from client 1"
              (readline (get-socket-logical-name (nth$ 1 ?more))))
   (expect-eq "the fifth client got its own reply"
              "reply to request from client 2"
              (readline (get-socket-logical-name (nth$ 2 ?more))))

   ;;=================================================================
   ;; Teardown
   ;;=================================================================
   (close-connection (nth$ 1 ?more))
   (close-connection (nth$ 2 ?more))
   (expect-true "the listening socket closes" (close-connection ?srv)))

(run-tests)
(test-summary)
