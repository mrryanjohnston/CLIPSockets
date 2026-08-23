; HTTPS from parts that already exist. This example has no new CLIPS code.
; server-complex.clp controls the connections, server-complex-tls.clp adds the
; handshake, and server-http-file.clp answers the requests. None of the three
; files knows about the other two.
;
; The repository does not hold the certificate, and you must make it first. The
; certificate names localhost, and you must use that name and not
; 127.0.0.1:
;
;   ./tests/fixtures/regenerate.sh
;   ./clips -f2 examples/server-http-file-tls.bat
;   curl --cacert tests/fixtures/ca.pem https://localhost:8888/
;
; To send a usual file, the server needs the (mimetype) function, and that
; function needs a MAGIC=1 build. The directory lists and /styles.css operate
; with each build.

; Without this line, a client that closes during a reply would end this
; server.
(signal SIGPIPE SIG_IGN)
;(watch all)
(load examples/server-complex.clp)
(load examples/server-complex-tls.clp)
(load examples/server-http-file.clp)
(reset)
(run)
(exit)
