; This client writes to sockets too: without ignoring SIGPIPE a server that
; closes a connection first would kill this process before it can report.
(signal SIGPIPE SIG_IGN)
;(watch all)
(load examples/ab-clone.clp)
(reset)
(run)
(exit)
