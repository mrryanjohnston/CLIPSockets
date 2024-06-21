(deffacts defaults
	;(ip 127.0.0.1)
	(ip 0.0.0.0)
	;(port 8888)
	(port 8000)
	(max-simultaneous-requests 50)
	(current-time (time))
	(unstarted-requests 1234)
	(started-requests 0)
	(finished-requests 0)
	(running-requests 0)
	(message-to-send "GET / HTTP/1.1")
	(max-lifetime 5)); seconds

(defrule start-request
	(ip ?ip)
	(port ?port)
	(max-simultaneous-requests ?maxTotal)
	?currentTime <-
		(current-time ?time)
	?unstartedRequests <-
		(unstarted-requests ?requestsLeft&:(> ?requestsLeft 0))
	?startedRequests <-
		(started-requests ?started)
	?runningRequests <-
		(running-requests ?running&:(< ?running ?maxTotal))
	(finished-requests ?finished)
	=>
	;(println "DEBUG: Starting request number " (+ 1 ?started))
	(bind ?socket (create-socket AF_INET SOCK_STREAM))
	;(fcntl-add-status-flags ?socket O_NONBLOCK)
	(setsockopt ?socket IPPROTO_TCP TCP_NODELAY 1)
	(set-not-buffered ?socket)
	(set-timeout ?socket 0)
	(retract ?currentTime ?startedRequests ?runningRequests ?unstartedRequests)
	(bind ?name (connect ?socket ?ip ?port))
	(bind ?now (time))
	(assert
		(unstarted-requests (- ?requestsLeft 1))
		(started-requests (+ ?started 1))
		(running-requests (+ ?running 1))
		(current-time ?now)
		(request-started-at (+ 1 ?started) ?now)
		(request
			?socket
			?name
			(+ 1 ?started)
			0 1 ; begin fibb counting
			?now)))

(defrule backoff-and-sleep
"when there are no requests that can be acted upon, sleep"
	(max-simultaneous-requests ?maxTotal)
	?currentTime <-
		(current-time ?time)
	?fact <- (request
		?socket
		?name&~FALSE
		?id
		?previousRetryDelay
		?retryDelay
		?delayUntil&:(< ?time ?delayUntil))
	(not (request ? ? ? ? ? ?d&:(< ?d ?delayUntil)))
	(not (message-from-server ?id $?))
	=>
	(bind ?seconds (- ?delayUntil ?time))
	;(println "[DEBUG]: Server not ready. Waiting for " ?seconds "s.")
	(sleep ?seconds)
	(retract ?currentTime)
	(bind ?now (time))
	(assert (current-time ?now)))

(defrule check-if-server-ready-to-receive-data
	?currentTime <- (current-time ?time)
	?fact <- (request
		?socket
		?name&~FALSE
		?id
		?previousRetryDelay
		?retryDelay
		?delayUntil&:(>= ?time ?delayUntil))
	(not (request ? ? ? ? ? ?d&:(< ?d ?delayUntil)))
	(not (ready-to-write ?id ?))
	=>
	;(println "[DEBUG] Request " ?id ": Checking if server is ready to receive data")
	(retract ?currentTime)
	(bind ?now (time))
	(assert
		(current-time ?now)
		(ready-to-write
			?id
			(poll ?name 0 POLLOUT) ?now)))

(defrule check-if-server-ready-to-receive-data-after-delay
	?currentTime <- (current-time ?time)
	?fact <- (request
		?socket
		?name&~FALSE
		?id
		?previousRetryDelay
		?retryDelay
		?delayUntil&:(< ?time ?delayUntil))
	(not (request ? ? ? ? ? ?d&:(< ?d ?delayUntil)))
	(not (ready-to-write ?id ? ?))
	=>
	;(println "[DEBUG] Request " ?id ": Checking if server is ready to receive data with delay of " (integer (- ?delayUntil ?time)) "ms.")
	(retract ?currentTime)
	(bind ?poll (poll ?name (integer (- ?delayUntil ?time)) POLLIN))
	(bind ?now (time))
	(assert
		(current-time ?now)
		(ready-to-write ?id ?poll ?now)))

(defrule retry-checking-for-ready-to-receive
	?currentTime <- (current-time ?time)
	?fact <- (request
		?socket
		?name
		?id
		?previousRetryDelay
		?retryDelay
		?delayUntil)
	(not (request ? ? ? ? ? ?d&:(< ?d ?delayUntil)))
	?fail <- (ready-to-write
		?id
		FALSE
		?)
	=>
	;(println "[DEBUG] Request " ?id ": Failed check for ready to receive. Increasing delay to " (+ ?retryDelay ?previousRetryDelay) "ms.")
	(retract ?currentTime ?fact ?fail)
	(bind ?now (time))
	(assert (current-time ?now)
		(request
			?socket
			?name
			?id
			?retryDelay
			(+ ?previousRetryDelay ?retryDelay)
			(+ ?now (/ ?retryDelay 1000)))))

(defrule send-message-to-successful-request
	(message-to-send ?messageToSend)
	?currentTime <- (current-time ?time)
	?fact <- (request
		?socket
		?name
		?id
		?previousRetryDelay
		?retryDelay
		?delayUntil&:(>= ?time ?delayUntil))
	(not (request ? ? ? ? ? ?d&:(< ?d ?delayUntil)))
	(ready-to-write ?id TRUE ?)
	(not (written-at ?id ?))
	=>
	;(println "[DEBUG] Request " ?id ": server responded that it was ready to write. Writing...")
	(printout ?name ?messageToSend crlf crlf)
	(flush-connection ?name)
	(retract ?currentTime ?fact)
	(bind ?now (time))
	(assert 
		(current-time ?now)
		(written-at ?id ?now)
		(request
			?socket
			?name
			?id
			?previousRetryDelay
			?retryDelay
			?now)))

(defrule check-if-server-has-sent-us-data
	?currentTime <- (current-time ?time)
	?fact <- (request
		?socket
		?name
		?id
		?previousRetryDelay
		?retryDelay
		?delayUntil&:(>= ?time ?delayUntil))
	(not (request ? ? ? ? ? ?d&:(< ?d ?delayUntil)))
	(ready-to-write ?id TRUE ?)
	(written-at ?id ?)
	(not (has-data-to-read ?id ? ?))
	=>
	;(println "[DEBUG] Request " ?id ": Checking if server has sent us data")
	(retract ?currentTime)
	(bind ?now (time))
	(assert
		(current-time ?now)
		(has-data-to-read
			?id
			(poll ?name 0 POLLOUT)
			?now)))

(defrule check-if-server-has-sent-us-data-after-delay
	?currentTime <- (current-time ?time)
	?fact <- (request
		?socket
		?name&~FALSE
		?id
		?previousRetryDelay
		?retryDelay
		?delayUntil&:(< ?time ?delayUntil))
	(not (request ? ? ? ? ? ?d&:(< ?d ?delayUntil)))
	(written-at ?id ?)
	(not (has-data-to-read ?id ? ?))
	=>
	;(println "[DEBUG] Request " ?id ": Checking if server has sent us data with delay of " (integer (- ?delayUntil ?time)) "ms.")
	(retract ?currentTime)
	(bind ?poll (poll ?name (integer (- ?delayUntil ?time)) POLLOUT))
	(bind ?now (time))
	(assert
		(current-time ?now)
		(has-data-to-read ?id ?poll ?now)))

(defrule retry-checking-for-data
	?currentTime <- (current-time ?time)
	?fact <- (request
		?socket
		?name
		?id
		?previousRetryDelay
		?retryDelay
		?delayUntil)
	(not (request ? ? ? ? ? ?d&:(< ?d ?delayUntil)))
	(written-at ?id ?)
	?fail <- (has-data-to-read ?id FALSE ?)
	=>
	;(println "[DEBUG] Request " ?id ": Retrying check if server is ready to receive data")
	(retract ?fact ?fail)
	(bind ?now (time))
	(assert
		(current-time ?now)
		(request
			?socket
			?name
			?id
			?retryDelay 
			(+ ?retryDelay ?previousRetryDelay)
			(+ ?now (/ ?retryDelay 1000)))))

(defrule read-from-server
	?runningRequests <- (running-requests ?running)
	?finishedRequests <- (finished-requests ?finished)
	?currentTime <- (current-time ?time)
	?fact <- (request
		?socket
		?name
		?id
		?previousRetryDelay
		?retryDelay
		?delayUntil&:(>= ?time ?delayUntil))
	(not (request ? ? ? ? ? ?d&:(< ?d ?delayUntil)))
	(request-started-at ?id ?started)
	(ready-to-write ?id TRUE ?)
	(written-at ?id ?)
	(has-data-to-read ?id TRUE ?)
	(not (message-from-server ?id $?))
	=>
	;(println "[DEBUG] Request " ?id ": server responded. Reading...")
	(bind ?messageFromServer (readline ?name))
	;(println "[DEBUG] Request " ?id ": " ?messageFromServer)
	(empty-connection ?name)
	(close-connection ?name)
	(retract
		?currentTime
		?runningRequests
		?finishedRequests
		?fact)
	(bind ?now (time))
	(assert
		(current-time ?now)
		(message-from-server ?id (- ?now ?started) (explode$ ?messageFromServer))
		(running-requests (- ?running 1))
		(finished-requests (+ ?finished 1))))

(defrule report
	(ip ?ip)
	(port ?port)
	(max-simultaneous-requests ?max)
	(unstarted-requests 0)
	(running-requests 0)
	(finished-requests ?finished)
	(message-from-server ? ?fastest $?)
	(message-from-server ? ?slowest $?)
	(not (message-from-server ? ?f&:(< ?f ?fastest) $?))
	(not (message-from-server ? ?s&:(> ?s ?slowest) $?))
	(not (reported))
	=>
	(println "Test results for " ?ip ":" ?port)
	(println "Total requests: " ?finished)
	(println "Maximum simultaneous requests: " ?max)
	(bind ?successes (find-all-facts ((?message message-from-server)) (and (eq (nth$ 5 ?message:implied) OK) (= (nth$ 4 ?message:implied) 200))))
	(bind ?failures (find-all-facts ((?message message-from-server)) (or (neq (nth$ 5 ?message:implied) OK) (<> (nth$ 4 ?message:implied) 200))))
	(bind ?all (create$ ?successes ?failures))
	(bind ?totalElapsed 0)
	(loop-for-count (?cnt 1 (length$ ?all))
		(bind ?message (nth$ ?cnt ?all))
		(bind ?totalElapsed (+ ?totalElapsed (nth$ 2 (fact-slot-value ?message implied))))
	)
	(println "Successful: " (* 100 (/ (length$ ?successes) ?finished)) %)
	(println "Failed: " (* 100 (/ (length$ ?failures) ?finished)) %)
	(println "Avg request time: " (/ ?totalElapsed ?finished) " seconds")
	(println "Fastest request time: " ?fastest " seconds")
	(println "Slowest request time: " ?slowest " seconds")
	(println "Total time taken by requests: " ?totalElapsed " seconds")
	(assert (reported)))
