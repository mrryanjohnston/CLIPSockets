; server-complex.clp with TLS in place of plaintext. This file loads the base
; file without a change, and server-complex-tls.clp adds the handshake.
;
;   ./clips -f2 examples/server-complex-tls.bat
;   ./clips -f2 examples/client-tls.bat

; Without this line, a client that closes during a reply would end this
; server.
(signal SIGPIPE SIG_IGN)
;(watch all)
(load examples/server-complex.clp)
(load examples/server-complex-tls.clp)
(reset)
(run)
(facts)
(exit)
