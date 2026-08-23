; The TLS version of client.bat. It connects to server-simple-tls.bat.
;
; The repository does not hold the certificate authority, and you must make it
; first. A client on the internet asks for the system trust store instead, and
; client-https.bat shows that.
;
;   ./tests/fixtures/regenerate.sh
;   ./clips -f2 examples/server-simple-tls.bat
;   ./clips -f2 examples/client-tls.bat

(defrule connect =>
	(bind ?ctx (tls-create-context TLS_CLIENT))
	; Without this call the client has no authorities to compare the server
	; with, and each certificate is untrusted.
	(tls-context-load-verify-locations ?ctx "tests/fixtures/ca.pem")

	(bind ?socket (create-socket AF_INET SOCK_STREAM))
	(bind ?connection (connect ?socket 127.0.0.1 8888))
	(println "[CLIENT] Connected to " ?connection)

	; The hostname is not only for the server. The code compares the
	; certificate with this name. As a result, the name must be a name that
	; the certificate holds. An address that a resolver accepts is not
	; sufficient.
	(assert (session ?socket ?connection (tls-connect ?ctx ?socket localhost))))

(defrule handshake-failed
	(session ?socket ? FALSE)
	=>
	(println "[CLIENT] TLS handshake failed")
	(close-connection ?socket))

(defrule exchange
	(session ?socket ?connection TRUE)
	=>
	(println "[CLIENT] " (tls-version ?socket) ", " (tls-cipher ?socket)
		", via " (tls-backend))
	(println "[CLIENT] Server certificate: " (tls-peer-subject ?socket))
	(println "[CLIENT] Verification: " (tls-verify-result ?socket))

	(println "[CLIENT] Type a message to send to server")
	(printout t "[CLIENT] followed by the ENTER key: ")
	(bind ?msg (readline))

	; From this point the socket reads and writes in the same manner as a
	; plaintext socket. The logical name did not change, but the code behind
	; that name did change.
	(printout ?connection ?msg crlf)
	(flush-connection ?socket)
	(println "[CLIENT] Wrote message. Waiting for response...")
	(println "[CLIENT] The server said: " (readline ?connection))

	(tls-shutdown ?socket)
	(close-connection ?socket))

(reset)
(run)
(exit)
