; The DTLS client for server-dtls.bat.
;
;   ./tests/fixtures/regenerate.sh
;   ./clips -f2 examples/server-dtls.bat
;   ./clips -f2 examples/client-dtls.bat
;
; The code must connect the socket before (dtls-connect). A datagram socket
; with no peer has no destination for a ClientHello. The TLS client needs no
; such step, because (connect) on a stream socket already made the connection.
;
; This file shows the two methods to read. (dtls-recv) takes one record and
; keeps its limits. (readline) also operates, through the same logical name
; that a plaintext UDP socket has. But a record has no end mark. As a result, a
; reader cannot find where one message stops and the next one starts, and a
; lost datagram leaves a gap that the reader cannot see. Use the record
; functions when the limits have a meaning.

(defrule connect =>
	(bind ?ctx (tls-create-context DTLS_CLIENT))
	(tls-context-load-verify-locations ?ctx "tests/fixtures/ca.pem")

	(bind ?sock (create-socket AF_INET SOCK_DGRAM))
	(bind ?name (connect ?sock 127.0.0.1 9443))
	(println "[CLIENT] connected to " ?name)

	(if (not (dtls-connect ?ctx ?sock localhost)) then
		(println "[CLIENT] handshake failed")
		(close-connection ?sock)
		(return))

	(println "[CLIENT] " (tls-version ?sock) " " (tls-cipher ?sock))
	(println "[CLIENT] server certificate: " (tls-peer-subject ?sock))

	; This is TRUE only when the code checked the peer and the check passed.
	; A connection with no verification gives FALSE. As a result, this test
	; has the meaning that it appears to have.
	(if (neq TRUE (tls-verify-result ?sock)) then
		(println "[CLIENT] refusing an unverified server")
		(close-connection ?sock)
		(return))

	(dtls-send ?sock "hello over dtls")

	(if (poll ?sock 5000 POLLIN) then
		(bind ?mf (dtls-recv ?sock))
		(if (neq ?mf FALSE) then
			(println "[CLIENT] reply: " (nth$ 2 ?mf)))
	 else
		(println "[CLIENT] no reply"))

	(tls-shutdown ?sock)
	(close-connection ?sock)
	(tls-free-context ?ctx)
	(println "[CLIENT] done."))

(reset)
(run)
(exit)
