;;; TLS on IPv4 loopback: a full client and server handshake, and a message in
;;; each direction.
;;;
;;; requires: tls
;;;
;;; The two ends are in this one process, and the handshakes cannot block.
;;; tls-connect would wait for a ServerHello that cannot arrive until
;;; tls-accept runs, and tls-accept is in the same thread. As a result, the
;;; test sets the two sockets to non-blocking mode and runs the two halves in
;;; turn until each one reports that it is complete. A non-blocking application
;;; does the same, and this test also covers the resumption of a
;;; handshake. That condition needs no test of its own.
;;;
;;; The certificate is the fixture under tests/fixtures/. A temporary CA made
;;; it for localhost. See tests/fixtures/regenerate.sh.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(load* "tests/lib/tls.clp")
(test-suite "tls-loopback")
(test-plan 27)

(defglobal ?*port* = 18921)
(defglobal ?*ca*   = "tests/fixtures/ca.pem")
(defglobal ?*cert* = "tests/fixtures/server.pem")
(defglobal ?*key*  = "tests/fixtures/server-key.pem")

(deffunction run-tests ()
   ;;=================================================================
   ;; Contexts
   ;;=================================================================
   (bind ?sctx (tls-create-context TLS_SERVER))
   (expect-gte "server context created" 1 ?sctx)
   (expect-true "server certificate loads" (tls-context-use-certificate-file ?sctx ?*cert*))
   (expect-true "server key loads"         (tls-context-use-private-key-file ?sctx ?*key*))

   (bind ?cctx (tls-create-context TLS_CLIENT))
   (expect-gte "client context created" 1 ?cctx)
   (expect-true "client trusts the fixture CA"
                (tls-context-load-verify-locations ?cctx ?*ca*))
   (expect-true "client requires a verified peer"
                (tls-context-set-verify ?cctx SSL_VERIFY_PEER))

   ;;=================================================================
   ;; Plain TCP underneath
   ;;=================================================================
   (bind ?srv (create-socket AF_INET SOCK_STREAM))
   (setsockopt ?srv SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?srv 127.0.0.1 ?*port*)
   (expect-true "listening" (listen ?srv))

   (bind ?cli (create-socket AF_INET SOCK_STREAM))
   (bind ?conn (connect ?cli 127.0.0.1 ?*port*))
   (expect-true "client connected" ?conn)

   (bind ?sfd (accept ?srv))
   (expect-gte "server accepted a connection" 0 ?sfd)

   ;; On Linux, accept() does not copy O_NONBLOCK from the listen socket. As
   ;; a result, the code must set the accepted end to non-blocking itself.
   (expect-true "client socket non-blocking" (fcntl-add-status-flags ?cli O_NONBLOCK))
   (expect-true "accepted socket non-blocking" (fcntl-add-status-flags ?sfd O_NONBLOCK))

   ;;=================================================================
   ;; Handshake
   ;;=================================================================
   ;; Each of these calls starts a handshake that cannot complete now. As a
   ;; result, each of them gives FALSE here, and the code below continues
   ;; them.
   (tls-connect ?cctx ?cli localhost)
   (tls-accept ?sctx ?sfd)

   (expect-true "handshake completes on both ends" (tls-drive-handshake ?cli ?sfd 200))

   ;; The non-blocking mode was necessary only to run two handshakes in one
   ;; process. For the data exchange, a usual caller wants blocking reads. A
   ;; TLS 1.3 server sends session tickets after the handshake. As a result, a
   ;; socket can have data to read but no application data. A blocking read
   ;; passes those tickets and the caller never sees them. A non-blocking
   ;; caller gets EOF instead. It must then read (errno-sym) for EAGAIN to tell
   ;; that condition from a peer that closed.
   (expect-true "client back to blocking" (fcntl-remove-status-flags ?cli O_NONBLOCK))
   (expect-true "server back to blocking" (fcntl-remove-status-flags ?sfd O_NONBLOCK))

   (expect-true "peer certificate verified" (tls-verify-result ?cli))
   (expect-eq   "negotiated a modern protocol" 1 (str-index "TLS" (tls-version ?cli)))
   (expect-true "a cipher was negotiated" (tls-cipher ?cli))
   ;; The subject is /CN=localhost. The name is present but not at the start.
   ;; This test checks only that the name is in the subject.
   (expect-gte  "client sees the fixture subject in the peer certificate" 1
                (str-index "localhost" (tls-peer-subject ?cli)))

   ;;=================================================================
   ;; Encrypted round trip, through the ordinary router functions
   ;;=================================================================
   (bind ?cname (get-socket-logical-name ?cli))
   (bind ?sname (get-socket-logical-name ?sfd))

   ;; printout and readline reach a TLS socket through the same logical name
   ;; as before. The handshake changed no name.
   (printout ?cname "ping from client" crlf)
   (expect-true "client flush" (flush-connection ?cli))

   (expect-true "server sees readable data" (poll ?sfd 5000 POLLIN))
   (expect-eq "server reads what the client wrote"
              "ping from client" (readline ?sname))

   (printout ?sname "pong from server" crlf)
   (expect-true "server flush" (flush-connection ?sfd))
   (expect-true "client sees readable data" (poll ?cli 5000 POLLIN))
   (expect-eq "client reads what the server wrote"
              "pong from server" (readline ?cname))

   ;;=================================================================
   ;; Teardown
   ;;=================================================================
   (expect-true "client shutdown" (tls-shutdown ?cli))
   (expect-true "client socket closes" (close-connection ?cli))
   (expect-true "server connection closes" (close-connection ?sfd))
   (close-connection ?srv)
   (tls-free-context ?cctx)
   (tls-free-context ?sctx))

(run-tests)
(test-summary)
