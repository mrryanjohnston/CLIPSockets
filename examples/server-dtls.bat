; A DTLS echo server: the UDP equivalent of server-simple-tls.bat.
;
; The repository does not hold the certificate authority, and you must make it
; first:
;
;   ./tests/fixtures/regenerate.sh
;   ./clips -f2 examples/server-dtls.bat
;   ./clips -f2 examples/client-dtls.bat
;
; Two items here have no equivalent in the TLS servers.
;
; There is no (accept) call. UDP has no connection to accept, and (dtls-accept)
; does that work. It answers the first ClientHello with a cookie, and it does
; no other work until a ClientHello comes back with that cookie from the same
; address. Only then is there a peer to serve. Until that time the call gives
; FALSE. For that cause the loop below calls it again and does not read the
; first FALSE as a failure.
;
; The socket also becomes a connected socket. (dtls-accept) connects it to the
; peer that it checked. From that point the kernel discards the datagrams from
; each other address. One socket serves one client. A server that needs more
; than one client at a time makes one socket for each client.

(defrule serve =>
	(bind ?ctx (tls-create-context DTLS_SERVER))
	(tls-context-use-certificate-file ?ctx "tests/fixtures/server.pem")
	(tls-context-use-private-key-file ?ctx "tests/fixtures/server-key.pem")

	(bind ?sock (create-socket AF_INET SOCK_DGRAM))
	(setsockopt ?sock SOL_SOCKET SO_REUSEADDR 1)
	(bind ?name (bind-socket ?sock 127.0.0.1 9443))
	(println "[SERVER] waiting on " ?name)

	; This socket blocks. As a result, each call waits for data and does not
	; run without a result. A non-blocking server polls between the calls
	; instead, and tests/integration/dtls-loopback.clp does that.
	(while (not (dtls-accept ?ctx ?sock)) do
		(println "[SERVER] cookie sent, waiting for the client to return it"))

	(println "[SERVER] handshake complete: " (tls-version ?sock)
	         " " (tls-cipher ?sock))

	; One record in and one record out. (dtls-recv) gives the byte count and
	; the data. (rcvfrom) gives the same fields and also the address. There is
	; only one peer now, and there is no address to give.
	(bind ?mf (dtls-recv ?sock))
	(if (neq ?mf FALSE) then
		(println "[SERVER] " (nth$ 1 ?mf) " bytes: " (nth$ 2 ?mf))
		(dtls-send ?sock (str-cat "ECHO: " (nth$ 2 ?mf))))

	(tls-shutdown ?sock)
	(close-connection ?sock)
	(tls-free-context ?ctx)
	(println "[SERVER] done."))

(reset)
(run)
(exit)
