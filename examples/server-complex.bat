; A client that hangs up mid-response would otherwise kill this server.
(signal SIGPIPE SIG_IGN)
;(watch all)
(load examples/server-complex.clp)
(reset)
(run)
(facts)
(exit)
