(watch all)
(defrule always =>
	(assert (socket (create-socket AF_INET6 SOCK_STREAM))))

(defrule bind (socket ?fd) =>
	(setsockopt ?fd SOL_SOCKET SO_REUSEADDR 1)
	(assert (bound ?fd (bind-socket ?fd ::1 8888))))

(defrule listen (socket ?fd) (bound ?fd ?name) =>
	(assert (listening ?fd (listen ?fd)))
)

(defrule accept (socket ?fd) (bound ?fd ?name) (listening ?fd TRUE) =>
	(assert (client ?fd (accept ?fd)))
)

(defrule read (socket ?fd) (bound ?fd ?name) (listening ?fd TRUE) (client ?fd ?cfd) =>
	(println "Client sent " (readline (get-socket-logical-name ?cfd)))
)

(defrule write (socket ?fd) (bound ?fd ?name) (listening ?fd TRUE) (client ?fd ?cfd) =>
	(bind ?cname (get-socket-logical-name ?cfd))
	(fcntl-add-status-flags ?cfd O_NONBLOCK)
	(setsockopt ?cfd IPPROTO_TCP TCP_NODELAY 1)
	(set-not-buffered ?cfd)
	(printout ?cname "Hi, client!" crlf)
	(flush-connection ?cname)
	(empty-connection ?name)
	(shutdown-connection ?name)
	(close-connection ?name)
)
(reset)
(run)
(exit)
