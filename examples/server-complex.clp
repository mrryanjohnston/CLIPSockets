(deftemplate socket
	(slot fd)
	(slot current-time (default-dynamic (time)))
	(slot next-client-fd (default nil))
	(slot bound (default nil))
	(slot listening (default nil))
	(slot client-waiting (default nil))
	(slot clients-connected (default 0))
	(slot max-clients (default 1000)))
(deftemplate client
	(slot fd)
	(slot socketfd)
	(slot name (default nil))
	(slot ready-to-read (default nil))
	(slot ready-to-write (default nil))
	(slot created-at)
	(slot delayed-until (default nil))
	(slot delayed-until-increment (default 0.01))
	(slot max-life-time (default 5))
	(slot max-message-length (default 512))
	(slot timeouts (default 0))
	(multislot last-ascii-codes (type INTEGER) (default 10))
	(multislot raw-ascii-codes (type INTEGER)))

(defrule always =>
	(assert
		(socket (fd (create-socket AF_INET SOCK_STREAM)))))

(defrule bind-sock
	?f <- (socket (fd ?fd) (bound nil))
	=>
	(setsockopt ?fd SOL_SOCKET SO_REUSEADDR 1)
	(modify ?f (bound
		(bind-socket ?fd 127.0.0.1 8888))))

(defrule listen-sock
	?f <- (socket
		(fd ?fd)
		(bound 127.0.0.1:8888)
		(listening nil))
	=>
	(modify ?f
		(listening (listen ?fd 1000))))

(defrule check-for-pending-client
	"Poll socket file descriptor to see if there is a client waiting to be accepted"
	?f <- (socket
		(bound ?bound)
		(listening TRUE)
		(client-waiting nil))
	=>
	;(println "[SERVER] Polling socket " ?bound "...")
	(modify ?f
		(current-time (time))
		(client-waiting
		(poll ?bound 0 POLLIN))))

(defrule block-and-wait-indefinitely
	"There is no waiting client, we don't have any clients waiting to be served"
	?f <- (socket
		(fd ?socketfd)
		(listening TRUE)
		(client-waiting FALSE)
		(clients-connected 0)
		(current-time ?currentTime))
	=>
	;(println "[SERVER] Waiting for connection... (ctrl+z and kill to exit)")
	; setting this to 0 sets this back to waiting indefinitely
	(set-timeout ?socketfd 0)
	(bind ?clientfd (accept ?socketfd))
	(bind ?time (time))
	(assert (client
	 	(socketfd ?socketfd)
		(created-at ?time)
		(delayed-until ?time)
		(fd ?clientfd)))
	(modify ?f
		(client-waiting nil)
		(clients-connected 1)
		(current-time ?time)))

(defrule accept-pending-client
	"If a client is waiting to be accepted and we're below max threshold, accept them"
	?f <- (socket
		(fd ?fd)
		(listening TRUE)
		(client-waiting TRUE)
		(clients-connected ?clientsConnected)
		(max-clients ?maxClients&:(< ?clientsConnected ?maxClients)))
	=>
	;(println "[SERVER] Accepting pending client...")
	(bind ?time (time))
	(assert (client
		(socketfd ?fd)
		(created-at ?time)
		(delayed-until ?time)
		(fd
			(accept ?fd))))
	(modify ?f
		(current-time ?time)
		(client-waiting nil)
		(clients-connected (+ 1 ?clientsConnected))))

(defrule block-and-poll-milliseconds
"When all connected clients are ?delayed-until sometime after socket's ?current-time, block on poll for that long"
	?f <- (socket
		(fd ?socketfd)
		(listening TRUE)
		(client-waiting FALSE)
		(clients-connected ?clientsConnected)
		(current-time ?currentTime))
	?c <- (client
		(fd ?clientfd)
		(socketfd ?socketfd)
		(delayed-until ?delayedUntil&:(> ?delayedUntil ?currentTime)))
	(not (client
		(fd ~?clientfd)
		(socketfd ?socketfd)
		(delayed-until ?d&:(> ?delayedUntil ?d))))
	=>
	;(println "[SERVER] set the timeout to the difference between delayedUntil and current time...")
	(bind ?waiting (poll ?socketfd
		(integer (* 100
			(- ?delayedUntil ?currentTime)))
		POLLIN))
	(bind ?time (time))
	(modify ?f
		(current-time ?time)
		(client-waiting ?waiting))
)

(defrule get-client-name
	?f <- (socket
		(fd ?socketfd)
		(current-time ?currentTime)
		(clients-connected ?clientsConnected))
	?c <- (client
		(fd ?clientfd&~FALSE)
		(socketfd ?socketfd)
		(name nil)
		(delayed-until ?delayedUntil&:(< ?delayedUntil ?currentTime)))
	=>
	;(println "[SERVER] Client accepted!")

	(fcntl-add-status-flags ?clientfd O_NONBLOCK)
	(setsockopt ?clientfd IPPROTO_TCP TCP_NODELAY 1)
	(modify ?c
		(name (get-socket-logical-name ?clientfd)))
	(modify ?f (current-time (time))))

(defrule could-not-accept-client
	?f <- (socket
		(fd ?socketfd)
		(current-time ?currentTime)
		(clients-connected ?clientsConnected))
	?c <- (client
		(fd FALSE)
		(socketfd ?socketfd))
	=>
	;(println "[SERVER] Client failed to accepted")
	(retract ?c)
	(modify ?f (current-time (time))))

(defrule check-client-ready-to-read
	?f <- (socket
		(fd ?socketfd)
		(listening TRUE)
		(current-time ?currentTime)
		(client-waiting ?waiting)
		(max-clients ?maxClients)
		(clients-connected ?clientsConnected))
	?c <- (client
		(fd ?clientfd)
		(socketfd ?socketfd)
		(name ?name&~nil)
		(delayed-until ?delayedUntil&:(<= ?delayedUntil ?currentTime))
		(ready-to-read nil)
		(max-life-time ?maxLifeTime)
		(created-at ?createdAt&:(<= (- ?currentTime ?createdAt) ?maxLifeTime)))
	(not (client (socketfd ?socketfd) (delayed-until ?d&:(> ?delayedUntil ?d))))
	(test (or
		(eq ?waiting FALSE)
		(and (eq ?waiting TRUE) (= ?clientsConnected ?maxClients))))
	=>
	;(println "[SERVER] Check if client " ?name " is ready to be read...")
	(modify ?c (ready-to-read
		(poll ?name 0 POLLIN)))
	(modify ?f (current-time (time))))

(defrule failed-ready-to-read
"Increase timeout counter for client who is not ready to read"
	?f <- (socket
		(fd ?socketfd)
		(listening TRUE)
		(current-time ?currentTime)
		(client-waiting ?waiting)
		(max-clients ?maxClients)
		(clients-connected ?clientsConnected))
	?c <- (client
		(fd ?clientfd)
		(socketfd ?socketfd)
		(name ?name&~nil)
		(delayed-until ?delayedUntil&:(< ?delayedUntil ?currentTime))
		(delayed-until-increment ?delayedUntilIncrement)
		(ready-to-read FALSE)
		(timeouts ?timeouts)
		(max-life-time ?maxLifeTime)
		(created-at ?createdAt&:(<= (- ?currentTime ?createdAt) ?maxLifeTime)))
	(not (client (socketfd ?socketfd) (delayed-until ?d&:(> ?delayedUntil ?d))))
	(test (or
		(eq ?waiting FALSE)
		(and (eq ?waiting TRUE) (= ?clientsConnected ?maxClients))))
	=>
	;(println "[SERVER] Client " ?name " failed to get ready to read before " ?timeout "ms")
	(bind ?time (time))
	(modify ?c
		(ready-to-read nil)
		(timeouts (+ 1 ?timeouts))
		(delayed-until (+ ?time ?delayedUntilIncrement))
		(delayed-until-increment (+ 0.001 ?delayedUntilIncrement)))
	(modify ?f (client-waiting nil) (current-time ?time)))

(defrule check-ready-to-write
	?f <- (socket
		(fd ?socketfd)
		(listening TRUE)
		(current-time ?currentTime)
		(max-clients ?maxClients)
		(client-waiting ?waiting)
		(clients-connected ?clientsConnected))
	?c <- (client
		(fd ?clientfd)
		(socketfd ?socketfd)
		(delayed-until ?delayedUntil&:(<= ?delayedUntil ?currentTime))
		(name ?name&~nil)
		(ready-to-write nil)
		(max-life-time ?maxLifeTime)
		(created-at ?createdAt&:(<= (- ?currentTime ?createdAt) ?maxLifeTime)))
	(not (client (socketfd ?socketfd) (delayed-until ?d&:(> ?delayedUntil ?d))))
	(test (or
		(eq ?waiting FALSE)
		(and (eq ?waiting TRUE) (= ?clientsConnected ?maxClients))))
	=>
	;(println "[SERVER] Read message from client: " ?message)
	(modify ?c (ready-to-write (poll ?name 0 POLLOUT))))
	;(modify ?f (client-waiting nil) (current-time (time))))

(defrule failed-ready-to-write-check
	?f <- (socket
		(fd ?socketfd)
		(listening TRUE)
		(current-time ?currentTime)
		(max-clients ?maxClients)
		(client-waiting ?waiting)
		(clients-connected ?clientsConnected))
	?c <- (client
		(fd ?clientfd)
		(socketfd ?socketfd)
		(delayed-until ?delayedUntil&:(<= ?delayedUntil ?currentTime))
		(delayed-until-increment ?delayedUntilIncrement)
		(name ?name&~nil)
		(ready-to-read TRUE)
		(ready-to-write FALSE)
		(timeouts ?timeouts)
		(max-life-time ?maxLifeTime)
		(created-at ?createdAt&:(<= (- ?currentTime ?createdAt) ?maxLifeTime)))
	(not (client (socketfd ?socketfd) (delayed-until ?d&:(> ?delayedUntil ?d))))
	(test (or
		(eq ?waiting FALSE)
		(and (eq ?waiting TRUE) (= ?clientsConnected ?maxClients))))
	=>
	(bind ?time (time))
	;(println "[SERVER] Client " ?name " failed to get ready to write before " ?timeout "ms")
	(modify ?c
		(ready-to-write nil)
		(timeouts (+ 1 ?timeouts))
		(delayed-until (+ ?time ?delayedUntilIncrement))
		(delayed-until-increment (* 2 ?delayedUntilIncrement)))
	(modify ?f
		(client-waiting nil)
		(current-time ?time)))

(defrule got-eof-from-client-before-getting-line-feed
	?f <- (socket (fd ?sfd) (clients-connected ?clientsConnected))
	?c <- (client
		(socketfd ?sfd)
		(name ?name) 
		(timeouts ?timeouts)
		(delayed-until-increment ?delayedUntilIncrement)
		(raw-ascii-codes $?rawAsciiCodes -1))
	=>
	(bind ?time (time))
	;(println "[SERVER] Client " ?name " failed to send an END_OF_MESSAGE character (newline) before " ?timeout "ms")
	(modify ?c
		(raw-ascii-codes ?rawAsciiCodes)
		(timeouts (+ 1 ?timeouts))
		(delayed-until (+ ?time ?delayedUntilIncrement))
		(delayed-until-increment (* 2 ?delayedUntilIncrement)))
	(modify ?f
		(client-waiting nil)
		(current-time ?time)))

(defrule end-of-message-received-respond-to-client
"We respond to the client once we get a new line character as signified by ascii 10"
	?f <- (socket
		(fd ?sfd)
		(clients-connected ?clientsConnected))
	?c <- (client
		(socketfd ?sfd)
		(name ?name)
		(last-ascii-codes $?lastAsciiCodes)
		(raw-ascii-codes
			$?rawAsciiCodes
			$?rawLastAsciiCodes&:(eq ?rawLastAsciiCodes ?lastAsciiCodes)))
	=>
	;(println "[SERVER] Writing to client " ?name "...")
	(bind ?message "")
	(loop-for-count (?cnt 1 (length$ ?rawAsciiCodes))
		(bind ?message (format nil "%s%c" ?message (nth$ ?cnt ?rawAsciiCodes))))
	(printout ?name "Hello, client! You sent: " ?message crlf)
	;(println "[SERVER] Cleaning up client connection " ?name "...")
	(flush-connection ?name)
	(empty-connection ?name)
	(shutdown-connection ?name)
	(close-connection ?name)
	(retract ?c)
	(modify ?f
		(current-time (time))
		(client-waiting nil)
		(clients-connected (- ?clientsConnected 1)))
)

(defrule timeout-client
"We timeout the client when the client's age is bigger than its maxLifeTime"
	?f <- (socket
		(fd ?socketfd)
		(current-time ?currentTime)
		(clients-connected ?clientsConnected)
		(listening TRUE))
	?c <- (client
		(name ?name)
		(socketfd ?socketfd)
		(max-life-time ?maxLifeTime)
		(created-at ?createdAt
				&:(> (- ?currentTime ?createdAt) ?maxLifeTime)))
=>
	;(println "[SERVER] Client " ?name " took too long. Closing...")
	(printout ?name "You took too long to send anything. Bye :(" crlf)
	;(println "[SERVER] Cleaning up client connection " ?name "...")
	(flush-connection ?name)
	(empty-connection ?name)
	(shutdown-connection ?name)
	(close-connection ?name)
	(retract ?c)
	(modify ?f
		(current-time (time))
		(client-waiting nil)
		(clients-connected (- ?clientsConnected 1)))
)

(defrule get-first-char-of-message
"get the first character sent by the client that has waited long enough"
	?f <- (socket
		(fd ?socketfd)
		(listening TRUE)
		(current-time ?currentTime)
		(max-clients ?maxClients)
		(client-waiting ?waiting)
		(clients-connected ?clientsConnected))
	?c <- (client
		(fd ?clientfd)
		(socketfd ?socketfd)
		; it's waited long enough
		(delayed-until ?delayedUntil
			&:(<= ?delayedUntil ?currentTime))
		(name ?name&~nil)
		(ready-to-read TRUE)
		(max-life-time ?maxLifeTime)
		; it's still alive
		(created-at ?createdAt
			&:(<= (- ?currentTime ?createdAt) ?maxLifeTime))
		; has not yet received data
		(raw-ascii-codes))
	(not (client
		(socketfd ?socketfd)
		(delayed-until ?d&:(> ?delayedUntil ?d))))
	(test (or
		(eq ?waiting FALSE)
		(and
			(eq ?waiting TRUE)
			(= ?clientsConnected ?maxClients))))
	=>
	;(println "[SERVER] Client " ?name " is ready to be read!")
	;(println "[SERVER] Setting client " ?name " to not buffered: " (set-not-buffered ?clientfd))
	;(println "[SERVER] Reading client " ?name "...")
	(bind ?time (time))
	(set-not-buffered ?clientfd)
	(modify ?c
		(delayed-until ?time)
		(raw-ascii-codes (get-char ?name)))
	(modify ?f
		(client-waiting nil)
		(current-time ?time)))

(defrule get-next-char-while-message-not-done
"continue reading for a client"
	?f <- (socket
		(fd ?socketfd)
		(listening TRUE)
		(current-time ?currentTime)
		(clients-connected ?clientsConnected)
		(max-clients ?maxClients)
		(client-waiting ?waiting))
	?c <- (client
		(socketfd ?socketfd)
		(name ?name)
		(ready-to-read TRUE)
		(max-life-time ?maxLifeTime)
		; it's waited long enough
		(delayed-until ?delayedUntil
			&:(<= ?delayedUntil ?currentTime))
		; it's still alive
		(created-at ?createdAt
			&:(<= (- ?currentTime ?createdAt) ?maxLifeTime))
		(max-message-length ?maxMessageLength)
		(last-ascii-codes $?lastAsciiCodes)
		(raw-ascii-codes
			$?rawAsciiCodes
			&:(> (length$ ?rawAsciiCodes) 0)
			&:(>= (nth$ (length$ ?rawAsciiCodes) ?rawAsciiCodes) 0)
			&:(<= (nth$ (length$ ?rawAsciiCodes) ?rawAsciiCodes) 127)
			&:(neq ?lastAsciiCodes
				(create$
					(subseq$
						?rawAsciiCodes
						(- (length$ ?rawAsciiCodes) (length$ ?lastAsciiCodes))
						(length$ ?rawAsciiCodes)))
					)))
	(not (client
		(socketfd ?socketfd)
		(delayed-until ?d&:(> ?delayedUntil ?d))))
	(test (<
		(length$ ?rawAsciiCodes)
		?maxMessageLength))
	(test (or
		(eq ?waiting FALSE)
		(and
			(eq ?waiting TRUE)
			(= ?clientsConnected ?maxClients))))
	=>
	(modify ?f
		(current-time (time)))
	(modify ?c
		(raw-ascii-codes ?rawAsciiCodes
			(get-char ?name))))

(defrule message-too-long
"We disconnect a client when message is too long"
	?f <- (socket
		(fd ?socketfd)
		(listening TRUE)
		(current-time ?currentTime)
		(clients-connected ?clientsConnected)
		(max-clients ?maxClients)
		(client-waiting ?waiting))
	?c <- (client
		(socketfd ?socketfd)
		(name ?name)
		(ready-to-read TRUE)
		(max-life-time ?maxLifeTime)
		; it's waited long enough
		(delayed-until ?delayedUntil
			&:(<= ?delayedUntil ?currentTime))
		; it's still alive
		(created-at ?createdAt
			&:(<= (- ?currentTime ?createdAt) ?maxLifeTime))
		(max-message-length ?maxMessageLength)
		(last-ascii-codes $?lastAsciiCodes)
		(raw-ascii-codes
			$?rawAsciiCodes
			; wasn't the end of the message
			$?rawLastAsciiCodes
				&:(neq ?rawLastAsciiCodes ?lastAsciiCodes)
				&:(=
					(+ (length$ ?rawLastAsciiCodes) (length$ ?rawAsciiCodes))
					?maxMessageLength)))
	(not (client
		(socketfd ?socketfd)
		(delayed-until ?d&:(> ?delayedUntil ?d))))
	(test (or
		(eq ?waiting FALSE)
		(and
			(eq ?waiting TRUE)
			(= ?clientsConnected ?maxClients))))
=>
	;(println "[SERVER] Client " ?name " took too long. Closing...")
	(printout ?name "Your message was too long; 512 characters max (including the newline at the end). Bye :(" crlf)
	;(println "[SERVER] Cleaning up client connection " ?name "...")
	(flush-connection ?name)
	(empty-connection ?name)
	(shutdown-connection ?name)
	(close-connection ?name)
	(retract ?c)
	(modify ?f
		(current-time (time))
		(client-waiting nil)
		(clients-connected (- ?clientsConnected 1)))
)

(defrule next-char-not-UTF-8
"not valid UTF-8 character"
	?f <- (socket
		(fd ?socketfd)
		(listening TRUE)
		(current-time ?currenttime)
		(clients-connected ?clientsconnected)
		(max-clients ?maxclients)
		(client-waiting ?waiting))
	?c <- (client
		(socketfd ?socketfd)
		(name ?name)
		(ready-to-read TRUE)
		(max-life-time ?maxlifetime)
		; it's waited long enough
		(delayed-until ?delayeduntil
			&:(<= ?delayeduntil ?currenttime))
		; it's still alive
		(created-at ?createdat
			&:(<= (- ?currenttime ?createdat) ?maxlifetime))
		(max-message-length ?maxmessagelength)
		(raw-ascii-codes
			$?rawasciicodes
				; including ?lastasciicode message is still bellow maxmessagelength threshold
				&:(< (length$ ?rawasciicodes) (- ?maxmessagelength 1))
			?lastasciicode
				; was not within valid utf-8 range
				&:(> ?lastasciicode 127)))
	(not (client
		(socketfd ?socketfd)
		(delayed-until ?d&:(> ?delayeduntil ?d))))
	(test (or
		(eq ?waiting FALSE)
		(and
			(eq ?waiting TRUE)
			(= ?clientsconnected ?maxclients))))
	=>
	;(println "[SERVER] Client " ?name " took too long. Closing...")
	(printout ?name "Your message contained non UTF-8 characters. Bye :(" crlf)
	;(println "[SERVER] Cleaning up client connection " ?name "...")
	(flush-connection ?name)
	(empty-connection ?name)
	(shutdown-connection ?name)
	(close-connection ?name)
	(retract ?c)
	(modify ?f
		(current-time (time))
		(client-waiting nil)
		(clients-connected (- ?clientsconnected 1))))
