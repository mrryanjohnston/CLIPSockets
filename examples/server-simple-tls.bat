; The TLS version of server-simple.bat. It has the same shape: create, bind,
; listen, accept, read and write. TLS adds a context at the start, and one
; tls-accept call between the accept of the client and the first read.
;
; The repository does not hold the certificate and the key, and you must make
; them first. They name localhost, 127.0.0.1 and ::1. A client must connect
; with one of those names, or the verification fails.
;
;   ./tests/fixtures/regenerate.sh
;   ./clips -f2 examples/server-simple-tls.bat
;   ./clips -f2 examples/client-tls.bat

; Without this line, a client that closes during a reply would end this
; server.
(signal SIGPIPE SIG_IGN)
;(watch all)

(defrule always =>
	(assert (socket (create-socket AF_INET SOCK_STREAM))))

; One context serves each connection. It holds the certificate, the key and the
; settings. The data of each connection is on its socket.
(defrule context =>
	(bind ?ctx (tls-create-context TLS_SERVER))
	(tls-context-use-certificate-file ?ctx "tests/fixtures/server.pem")
	(tls-context-use-private-key-file ?ctx "tests/fixtures/server-key.pem")
	(assert (tls-context ?ctx)))

(defrule bind (socket ?fd) =>
	(setsockopt ?fd SOL_SOCKET SO_REUSEADDR 1)
	(assert (bound ?fd (bind-socket ?fd 127.0.0.1 8888))))

(defrule listen (socket ?fd) (bound ?fd ?name) =>
	(assert (listening ?fd (listen ?fd))))

(defrule accept (socket ?fd) (bound ?fd ?name) (listening ?fd TRUE) =>
	(assert (client ?fd (accept ?fd))))

; This socket blocks. As a result, tls-accept runs the handshake to its end and
; gives TRUE. Only a non-blocking socket needs tls-handshake to continue the
; work, and server-complex-tls.clp shows that.
(defrule handshake (tls-context ?ctx) (client ?fd ?cfd) =>
	(assert (encrypted ?cfd (tls-accept ?ctx ?cfd))))

(defrule read (client ?fd ?cfd) (encrypted ?cfd TRUE) =>
	(bind ?cname (get-socket-logical-name ?cfd))
	(println "Client connected over " (tls-version ?cfd)
		" using " (tls-cipher ?cfd))
	(assert (received ?cfd (readline ?cname))))

(defrule write
	(socket ?fd) (bound ?fd ?name)
	(client ?fd ?cfd)
	(received ?cfd ?message)
	=>
	(bind ?cname (get-socket-logical-name ?cfd))
	(println "Client sent " ?message)
	(setsockopt ?cfd IPPROTO_TCP TCP_NODELAY 1)
	(set-not-buffered ?cfd)
	(printout ?cname "Hi, client! You sent: " ?message crlf)
	(flush-connection ?cname)
	; tls-shutdown sends the TLS close message. shutdown-connection would
	; close the socket below the session. The client would then see a
	; connection that stops and not a correct end.
	(tls-shutdown ?cfd)
	(close-connection ?cfd)
	(close-connection ?name))

(defrule handshake-failed (client ?fd ?cfd) (encrypted ?cfd FALSE) =>
	(println "Handshake failed. Was the client speaking TLS?")
	(close-connection ?cfd))

(reset)
(run)
(exit)
