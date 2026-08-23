; TLS for server-complex.clp. This file changes no line of that file.
;
; Load server-complex.clp first, and then load this file. No code here changes
; the socket deftemplate or the client deftemplate. The TLS data is in separate
; facts.
;
; The repository does not hold the certificate and the key, and you must make
; them first.
;
;   ./tests/fixtures/regenerate.sh
;   ./clips -f2 examples/server-complex-tls.bat
;   ./clips -f2 examples/client-tls.bat
;
; Two ideas give this file its shape.
;
; The first idea is how the code keeps a client away from the base file during
; its handshake. Each rule in server-complex.clp that reads or writes a client
; asks for a client whose name is not nil. As a result, a rule does not match a
; client with no name. This file asserts a client with no name, and it gives
; the client a name at the end of the handshake. From that moment the rules of
; the base file match the client, and they read and write through the session
; and not through the socket.
;
; The second idea is that a second handshake attempt is not a loop. Each call
; into the library asserts its result, and a separate rule matches each result.
; As a result, the patterns hold the branches and the actions do not. The
; engine matches each other rule between the attempts, and one slow handshake
; does not stop the server.

;;=====================================================================
;; The context: one context for each connection. It holds the certificate,
;; the key and the settings. The data of each connection is on its socket.
;;=====================================================================

(defrule tls-create-server-context
	(not (tls-context-handle ?))
	=>
	(assert (tls-context-handle (tls-create-context TLS_SERVER))))

(defrule tls-load-server-credentials
	(tls-context-handle ?ctx&~FALSE)
	(not (tls-context-configured ?ctx $?))
	=>
	(assert (tls-context-configured ?ctx
		(tls-context-use-certificate-file ?ctx "tests/fixtures/server.pem")
		(tls-context-use-private-key-file ?ctx "tests/fixtures/server-key.pem"))))

(defrule tls-server-context-ready
	(tls-context-configured ?ctx TRUE TRUE)
	=>
	(assert (tls-context ?ctx)))

(defrule tls-server-context-unusable
	(or (tls-context-handle FALSE)
		(tls-context-configured ? FALSE ?)
		(tls-context-configured ? ? FALSE))
	=>
	(println "[SERVER] Could not build a TLS context. Run "
		"tests/fixtures/regenerate.sh to create the certificate and key.")
	(halt))

;;=====================================================================
;; Accepting a client
;;=====================================================================

; The next two rules replace the rules with the same names in
; server-complex.clp. server-http-file.clp replaces
; end-of-message-received-respond-to-client in the same manner. A load of this
; file after the base file replaces the definitions, and the base file itself
; does not change.
;
; These rules replace the originals and do not add to them, because of what the
; originals assert. They assert a client with a name, and each read rule in the
; base file matches such a client. A new rule here could not always reach that
; client first. Among rules of equal salience, CLIPS fires the rule that it
; activated last, and the rule bodies of the base file decide that. A client
; with no name closes the window instead. There is then no moment where a
; client is both plaintext and visible to the base file.
;
; Each of the two rules reports only that a connection arrived. The next
; rule decides what to do with it.

(defrule block-and-wait-indefinitely
	"There is no waiting client, we don't have any clients waiting to be served"
	?f <- (socket
		(fd ?socketfd)
		(listening TRUE)
		(client-waiting FALSE)
		(clients-connected 0))
	=>
	; A value of 0 makes the socket wait with no time limit.
	(set-timeout ?socketfd 0)
	(bind ?clientfd (accept ?socketfd))
	(bind ?time (time))
	(modify ?f
		(client-waiting nil)
		(clients-connected 1)
		(current-time ?time))
	(assert (accepted ?socketfd ?clientfd ?time)))

(defrule accept-pending-client
	"If a client is waiting to be accepted and we're below max threshold, accept them"
	?f <- (socket
		(fd ?fd)
		(listening TRUE)
		(client-waiting TRUE)
		(clients-connected ?clientsConnected)
		(max-clients ?maxClients&:(< ?clientsConnected ?maxClients)))
	=>
	(set-timeout ?fd 1)
	(bind ?clientfd (accept ?fd))
	(bind ?time (time))
	(modify ?f
		(current-time ?time)
		(client-waiting nil)
		(clients-connected (+ 1 ?clientsConnected)))
	(assert (accepted ?fd ?clientfd ?time)))

; A client with no name is a client that waits.
(defrule tls-hold-accepted-client
	?a <- (accepted ?socketfd ?fd&~FALSE ?time)
	=>
	(retract ?a)
	(fcntl-add-status-flags ?fd O_NONBLOCK)
	(setsockopt ?fd IPPROTO_TCP TCP_NODELAY 1)
	(assert (client
		(socketfd ?socketfd)
		(fd ?fd)
		(name nil)
		(created-at ?time)
		(delayed-until ?time)))
	; The code keeps the name here, because the slot of the client cannot
	; hold it yet. This fact is the only other copy.
	(assert (tls-holding ?fd (get-socket-logical-name ?fd) fresh (+ (time) 5) 0)))

; The could-not-accept-client rule of the base file reports a failed accept
; through a client fact. As a result, there must still be such a fact for that
; rule to find.
(defrule tls-accept-failed
	?a <- (accepted ?socketfd FALSE ?time)
	=>
	(retract ?a)
	(assert (client (socketfd ?socketfd) (fd FALSE) (created-at ?time))))

;;=====================================================================
;; Starting the session
;;=====================================================================

; The socket is non-blocking. As a result, tls-accept starts the handshake and
; does not complete it. The code records the two answers together, because a
; failure also frees the session. tls-pending tells the two conditions apart:
; with no session there is no pending count.
(defrule tls-start-session
	(tls-context ?ctx)
	?h <- (tls-holding ?fd ?name fresh ?deadline ?)
	=>
	(retract ?h)
	(assert (tls-upgrade ?fd ?name ?deadline (tls-accept ?ctx ?fd) (tls-pending ?fd))))

(defrule tls-session-started
	?u <- (tls-upgrade ?fd ?name ?deadline TRUE ?)
	=>
	(retract ?u)
	(assert (tls-holding ?fd ?name done ?deadline 0)))

(defrule tls-session-unfinished
	?u <- (tls-upgrade ?fd ?name ?deadline FALSE ?pending&~FALSE)
	=>
	(retract ?u)
	(assert (tls-holding ?fd ?name handshaking ?deadline 0)))

(defrule tls-session-refused
	?u <- (tls-upgrade ?fd ?name ?deadline FALSE FALSE)
	=>
	(retract ?u)
	(assert (tls-holding ?fd ?name failed ?deadline 0)))

;;=====================================================================
;; Carrying an unfinished handshake to the end
;;=====================================================================

; A retract and a new assert let this rule run again. A rule fires one time for
; each activation. As a result, a field in the fact must change before the
; engine schedules another attempt. The deadline decides when the code stops,
; and a count does not. No code in this loop waits, and a count would reach its
; limit in microseconds.
;
; The code reads errno at the same time as the result. Any call between the two
; would change errno, and errno is the only value that separates "call again"
; from "this connection is finished". errno-sym gives its name later.
(defrule tls-continue-handshake
	?h <- (tls-holding ?fd ?name handshaking ?deadline ?tries)
	(socket (fd ?socketfd) (current-time ?currentTime))
	(client (fd ?fd) (socketfd ?socketfd)
		(delayed-until ?delayedUntil&:(<= ?delayedUntil ?currentTime)))
	(test (<= (time) ?deadline))
	=>
	(retract ?h)
	(assert (tls-attempt ?fd ?name ?deadline ?tries (tls-handshake ?fd) (errno))))

(defrule tls-handshake-completed
	?a <- (tls-attempt ?fd ?name ?deadline ? TRUE ?)
	=>
	(retract ?a)
	(assert (tls-holding ?fd ?name done ?deadline 0)))

(defrule tls-handshake-wants-more-data
	?a <- (tls-attempt ?fd ?name ?deadline ?tries FALSE ?e&:(eq (errno-sym ?e) EAGAIN))
	?c <- (client (fd ?fd) (delayed-until-increment ?increment))
	=>
	(retract ?a)
	; No code here waits, and no code here polls. This rule only moves the
	; delayed-until slot of the client forward, and the base file does the
	; remainder. The tls-continue-handshake rule above does not match again
	; until the current-time of the socket reaches that value.
	; block-and-poll-milliseconds moves the current-time, and it polls the
	; listen socket for exactly that time.
	;
	; As a result, the wait always does one of two useful things. Below the
	; client limit, a connection during that poll starts the server early and
	; the server accepts the connection. At the limit, or with no new
	; connection, the poll is a wait of a known length and not a loop with no
	; result. In both conditions the engine can match each other rule, and a
	; handshake in progress stops nothing.
	;
	; The step increases in the same manner as in failed-ready-to-read. As a
	; result, a slow peer costs less and less time. The deadline decides when
	; the code stops, and a number of attempts does not. With no fixed wait, a
	; count gives no data about the time.
	(bind ?now (time))
	(modify ?c
		(delayed-until (+ ?now ?increment))
		(delayed-until-increment (+ 0.001 ?increment)))
	(assert (tls-holding ?fd ?name handshaking ?deadline (+ ?tries 1))))

(defrule tls-handshake-errored
	?a <- (tls-attempt ?fd ?name ?deadline ? FALSE ?e&:(neq (errno-sym ?e) EAGAIN))
	=>
	(retract ?a)
	(assert (tls-holding ?fd ?name failed ?deadline 0)))

(defrule tls-handshake-took-too-long
	?h <- (tls-holding ?fd ?name handshaking ?deadline ?)
	(test (> (time) ?deadline))
	=>
	(retract ?h)
	(assert (tls-holding ?fd ?name failed ?deadline 0)))

;;=====================================================================
;; Handing the client over, or giving up on it
;;=====================================================================

; The new name gives the client to the base file. From this point the rules of
; the base file match this client, and they read it and write it in the same
; manner as a plaintext client.
(defrule tls-release-client
	?h <- (tls-holding ?fd ?name done ? ?)
	?c <- (client (fd ?fd) (name nil))
	=>
	(retract ?h)
	(modify ?c (name ?name)))

; This rule must change clients-connected in the same manner as the cleanup
; rules of the base file. Without that, the server never waits for a new
; connection again.
(defrule tls-drop-client
	?h <- (tls-holding ?fd ?name failed ? ?)
	?f <- (socket (fd ?socketfd) (clients-connected ?connected))
	?c <- (client (fd ?fd) (socketfd ?socketfd))
	=>
	(println "[SERVER] Handshake with " ?name " failed. Closing...")
	(close-connection ?fd)
	(retract ?h ?c)
	(modify ?f
		(current-time (time))
		(client-waiting nil)
		(clients-connected (- ?connected 1))))
