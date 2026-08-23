;;; DTLS on IPv4 loopback: a full client and server handshake, and a message in
;;; each direction.
;;;
;;; requires: tls
;;; requires: dtls
;;;
;;; The two ends are in this one process, and neither handshake can block. The
;;; client would wait for a ServerHello that cannot arrive until the server
;;; runs, and the server is in the same thread. The two sockets are
;;; non-blocking, and the code runs the two halves in turn.
;;;
;;; A DTLS server has no accept() call. It replies to each ClientHello on a
;;; bound socket until one comes back with a cookie that agrees with the
;;; address that sent it. Only then is there an association. (dtls-accept) does
;;; that full exchange. As a result, a client that gets no reply yet makes the
;;; call give FALSE with EAGAIN. For that cause the loop below calls it again
;;; and does not read the first FALSE as a failure. The exchange has no state
;;; by design, and to start it again costs nothing.
;;;
;;; The certificate is the fixture under tests/fixtures/. A temporary CA made
;;; it for localhost. See tests/fixtures/regenerate.sh.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(load* "tests/lib/tls.clp")
(test-suite "dtls-loopback")
(test-plan 19)

(defglobal ?*sport* = 18971)
(defglobal ?*cport* = 18972)
(defglobal ?*ca*    = "tests/fixtures/ca.pem")
(defglobal ?*cert*  = "tests/fixtures/server.pem")
(defglobal ?*key*   = "tests/fixtures/server-key.pem")

;;; tests/lib/tls.clp has the handshake function, and dtls-router-io.clp also
;;; uses it. That function cannot be in tests/lib/socket.clp. The plaintext
;;; tests also load that file, and a dtls-* name in a TLS=0 build does not
;;; parse and would stop each of those tests.

(deffunction run-tests ()
   ;;=================================================================
   ;; Contexts
   ;;=================================================================
   (bind ?sctx (tls-create-context DTLS_SERVER))
   (expect-gte "server context created" 1 ?sctx)
   (expect-true "server certificate loads" (tls-context-use-certificate-file ?sctx ?*cert*))
   (expect-true "server key loads"         (tls-context-use-private-key-file ?sctx ?*key*))

   (bind ?cctx (tls-create-context DTLS_CLIENT))
   (expect-gte "client context created" 1 ?cctx)
   (expect-true "client trusts the fixture CA"
                (tls-context-load-verify-locations ?cctx ?*ca*))
   (expect-true "DTLS 1.2 is accepted as a floor"
                (tls-context-set-min-proto-version ?cctx DTLS1_2_VERSION))
   (expect-false "a TLS version is refused on a DTLS context"
                 (tls-context-set-min-proto-version ?cctx TLS1_2_VERSION))

   ;;=================================================================
   ;; Sockets
   ;;=================================================================
   (bind ?srv (create-socket AF_INET SOCK_DGRAM))
   (setsockopt ?srv SOL_SOCKET SO_REUSEADDR 1)
   (expect-true "server bound" (bind-socket ?srv 127.0.0.1 ?*sport*))

   (bind ?cli (create-socket AF_INET SOCK_DGRAM))
   (setsockopt ?cli SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?cli 127.0.0.1 ?*cport*)
   (bind ?cname (connect ?cli 127.0.0.1 ?*sport*))
   (expect-true "client connected to the server port" ?cname)

   (fcntl-add-status-flags ?srv O_NONBLOCK)
   (fcntl-add-status-flags ?cli O_NONBLOCK)

   ;;=================================================================
   ;; Handshake
   ;;=================================================================
   (expect-true "both ends complete the handshake"
                (dtls-drive-handshake ?cctx ?sctx ?cli ?srv 200))

   (expect-eq "the negotiated protocol is DTLS 1.2" DTLSv1.2 (tls-version ?cli))
   (expect-true "a cipher was agreed" (tls-cipher ?cli))
   (expect-true "the client verified the server" (tls-verify-result ?cli))
   (expect-true "the client can name the peer" (tls-peer-subject ?cli))

   ;;=================================================================
   ;; Records
   ;;=================================================================
   (expect-eq "the client sends a record" 5 (dtls-send ?cli "hello"))
   (expect-true "the server sees it arrive" (poll ?srv 5000 POLLIN))

   (bind ?mf (dtls-recv ?srv))
   (expect-length "dtls-recv returns two fields" 2 ?mf)
   (expect-eq "field 1 is the byte count" 5 (nth$ 1 ?mf))
   (expect-eq "field 2 is the payload" "hello" (nth$ 2 ?mf))

   ;;=================================================================
   ;; Teardown
   ;;=================================================================
   (tls-shutdown ?cli)
   (close-connection ?cli)
   (close-connection ?srv)
   (tls-free-context ?cctx)
   (tls-free-context ?sctx))

(run-tests)
(test-summary)
