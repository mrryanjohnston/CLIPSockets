; Reads a page from a real site over HTTPS. The code checks the certificate
; against the system trust store and not against a certificate authority of
; this project.
;
;   ./clips -f2 examples/client-https.bat
;
; This is the one example that needs an internet connection. Each other example
; uses a server on the loopback address.
;
; It also needs the system trust store at the location that the build of the
; TLS library uses. A packaged library usually has that location. But a library
; under a prefix of its own looks under that prefix and can find no store. Each
; certificate is then untrusted and the handshake fails. In that condition,
; load a bundle with tls-context-load-verify-locations. Debian and Ubuntu keep
; their bundle at /etc/ssl/certs/ca-certificates.crt.

(defglobal ?*host* = example.com)
(defglobal ?*max-lines* = 20)

(defrule resolve =>
	(assert (addresses (resolve-domain-name ?*host*))))

(defrule could-not-resolve
	(addresses FALSE)
	=>
	(println "[CLIENT] Could not resolve " ?*host*))

; resolve-domain-name gives each address for the name, and some of them are
; IPv6. This socket is AF_INET. An address with a colon in it is IPv6.
(defrule pick-an-ipv4-address
	(addresses $? ?address&:(eq FALSE (str-index ":" ?address)) $?)
	(not (chosen ?))
	=>
	(println "[CLIENT] " ?*host* " is at " ?address)
	(assert (chosen ?address)))

(defrule connect
	(chosen ?address)
	=>
	(bind ?socket (create-socket AF_INET SOCK_STREAM))
	(assert (connected ?socket (connect ?socket ?address 443))))

(defrule could-not-connect
	(connected ?socket FALSE)
	=>
	(println "[CLIENT] Could not connect to " ?*host*)
	(close-connection ?socket))

(defrule start-tls
	(connected ?socket ?connection&~FALSE)
	=>
	(bind ?ctx (tls-create-context TLS_CLIENT))
	; These are the authorities that the operating system trusts. A browser
	; starts from the same list. For a private CA, use
	; tls-context-load-verify-locations.
	(tls-context-set-default-verify-paths ?ctx)
	(tls-context-set-min-proto-version ?ctx TLS1_2_VERSION)
	; The code sends the hostname to the server, and the server then knows
	; which certificate to send. The code also compares that certificate with
	; the same hostname.
	(assert (session ?socket ?connection (tls-connect ?ctx ?socket ?*host*))))

(defrule handshake-failed
	(session ?socket ? FALSE)
	=>
	(println "[CLIENT] TLS handshake with " ?*host* " failed")
	(close-connection ?socket))

(defrule request
	(session ?socket ?connection TRUE)
	=>
	(println "[CLIENT] " (tls-version ?socket) ", " (tls-cipher ?socket))
	(println "[CLIENT] Certificate: " (tls-peer-subject ?socket))
	(println "[CLIENT] Verification: " (tls-verify-result ?socket))
	; HTTP needs CRLF. But crlf writes only LF, unless the build of CLIPS set
	; useFullCRLF. As a result, this code writes cr and lf separately, and the
	; request is then correct with each build.
	(printout ?connection
		"GET / HTTP/1.1" cr lf
		"Host: " ?*host* cr lf
		"User-Agent: CLIPSockets" cr lf
		"Connection: close" cr lf cr lf)
	(flush-connection ?socket)
	(assert (reading ?socket ?connection 0)))

(defrule read-a-line
	?r <- (reading ?socket ?connection ?count&:(< ?count ?*max-lines*))
	=>
	(retract ?r)
	(assert (line ?socket ?connection ?count (readline ?connection))))

(defrule show-a-line
	?l <- (line ?socket ?connection ?count ?text&~EOF)
	=>
	(retract ?l)
	(println ?text)
	(assert (reading ?socket ?connection (+ ?count 1))))

(defrule end-of-response
	?l <- (line ?socket ? ? EOF)
	=>
	(retract ?l)
	(println "[CLIENT] Server closed the connection.")
	(assert (finished ?socket)))

(defrule seen-enough
	?r <- (reading ?socket ? ?count&:(>= ?count ?*max-lines*))
	=>
	(retract ?r)
	(println "[CLIENT] Stopping after " ?*max-lines* " lines.")
	(assert (finished ?socket)))

(defrule close-up
	?f <- (finished ?socket)
	=>
	(retract ?f)
	(tls-shutdown ?socket)
	(close-connection ?socket))

(reset)
(run)
(exit)
