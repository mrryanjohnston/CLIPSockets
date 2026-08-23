;;; A free of a context while its sessions still operate.
;;;
;;; requires: tls
;;;
;;; (tls-free-context) frees the handle. It must not stop the connections that
;;; the code already made from that context. A caller cannot know which of
;;; those connections still operate. A server that makes one context, accepts
;;; connections from it and then frees it does a usual operation.
;;;
;;; The library decides if that operation is safe, and this file exists for
;;; that cause. OpenSSL counts the references to an SSL_CTX, and a session
;;; keeps the SSL_CTX itself. GnuTLS, mbedTLS and s2n each keep the context in
;;; the session as a pointer and copy no data. As a result, a free leaves each
;;; session with a pointer to memory that is free. The behaviour of OpenSSL is
;;; correct, and the other three must give the same behaviour. The C code and
;;; not the library must do that work.
;;;
;;; The test checks the two sequences, because they fail differently. A
;;; handshake that is in progress reads the trust store at the time of the
;;; free, and it stops inside certificate verification. A session that
;;; completed needs the trust store no more and can continue by chance. That
;;; condition is worse, because it hides the defect.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(load* "tests/lib/tls.clp")
(test-suite "tls-context-lifetime")
(test-plan 23)

(defglobal ?*port* = 18931)
(defglobal ?*ca*   = "tests/fixtures/ca.pem")
(defglobal ?*cert* = "tests/fixtures/server.pem")
(defglobal ?*key*  = "tests/fixtures/server-key.pem")

;;; A connected and accepted pair of non-blocking sockets. The code starts the
;;; handshake on the two ends and completes neither of them. Gives the client,
;;; the server and the listen socket.
(deffunction started-pair (?cctx ?sctx ?port)
   (bind ?pair (tcp-connected-pair ?port))
   (bind ?srv (nth$ 1 ?pair))
   (bind ?cli (nth$ 2 ?pair))
   (bind ?sfd (nth$ 3 ?pair))

   (fcntl-add-status-flags ?cli O_NONBLOCK)
   (fcntl-add-status-flags ?sfd O_NONBLOCK)

   (tls-connect ?cctx ?cli localhost)
   (tls-accept ?sctx ?sfd)

   (return (create$ ?cli ?sfd ?srv)))

;;; A message in each direction on a pair that completed its handshake. The
;;; function uses the logical names, as a caller does. This must continue to
;;; operate after the code frees the context.
(deffunction exchange (?cli ?sfd ?what)
   (bind ?cname (get-socket-logical-name ?cli))
   (bind ?sname (get-socket-logical-name ?sfd))
   (printout ?cname ?what crlf)
   (flush-connection ?cli)
   (poll ?sfd 5000 POLLIN)
   (return (readline ?sname)))

;;=====================================================================
;; Freed while the handshake is still in flight
;;=====================================================================
(deffunction free-during-handshake ()
   (bind ?sctx (tls-create-context TLS_SERVER))
   (expect-gte  "server context created" 1 ?sctx)
   (expect-true "server certificate loads" (tls-context-use-certificate-file ?sctx ?*cert*))
   (expect-true "server key loads"         (tls-context-use-private-key-file ?sctx ?*key*))

   (bind ?cctx (tls-create-context TLS_CLIENT))
   (expect-gte  "client context created" 1 ?cctx)
   (expect-true "client trusts the fixture CA"
                (tls-context-load-verify-locations ?cctx ?*ca*))

   (bind ?pair (started-pair ?cctx ?sctx ?*port*))
   (bind ?cli (nth$ 1 ?pair))
   (bind ?sfd (nth$ 2 ?pair))
   (bind ?srv (nth$ 3 ?pair))

   ;; The two handshakes are incomplete, and neither of them has its
   ;; certificates yet. At this moment the code is about to read the trust
   ;; store.
   (expect-true "client context frees mid-handshake" (tls-free-context ?cctx))
   (expect-true "server context frees mid-handshake" (tls-free-context ?sctx))

   (expect-true "handshake completes after its context was freed"
                (tls-drive-handshake ?cli ?sfd 200))

   (fcntl-remove-status-flags ?cli O_NONBLOCK)
   (fcntl-remove-status-flags ?sfd O_NONBLOCK)

   (expect-true "the peer certificate was still verified" (tls-verify-result ?cli))
   (expect-eq   "data still crosses the connection"
                "after freeing mid-handshake" (exchange ?cli ?sfd "after freeing mid-handshake"))

   (expect-true "client shutdown" (tls-shutdown ?cli))
   (expect-true "client closes"   (close-connection ?cli))
   (expect-true "server closes"   (close-connection ?sfd))
   (close-connection ?srv))

;;=====================================================================
;; Freed once both handshakes are done
;;=====================================================================
(deffunction free-after-handshake ()
   (bind ?sctx (tls-create-context TLS_SERVER))
   (tls-context-use-certificate-file ?sctx ?*cert*)
   (tls-context-use-private-key-file ?sctx ?*key*)

   (bind ?cctx (tls-create-context TLS_CLIENT))
   (tls-context-load-verify-locations ?cctx ?*ca*)

   (bind ?pair (started-pair ?cctx ?sctx (+ ?*port* 1)))
   (bind ?cli (nth$ 1 ?pair))
   (bind ?sfd (nth$ 2 ?pair))
   (bind ?srv (nth$ 3 ?pair))

   (expect-true "handshake completes" (tls-drive-handshake ?cli ?sfd 200))

   (fcntl-remove-status-flags ?cli O_NONBLOCK)
   (fcntl-remove-status-flags ?sfd O_NONBLOCK)

   (expect-true "client context frees after the handshake" (tls-free-context ?cctx))
   (expect-true "server context frees after the handshake" (tls-free-context ?sctx))

   ;; Each check below uses sessions whose context is free.
   (expect-true "the session still reports a cipher"  (tls-cipher ?cli))
   (expect-true "the session still reports a version" (tls-version ?cli))
   (expect-true "the peer certificate is still readable"
                (tls-peer-subject ?cli))
   (expect-eq   "data still crosses the connection"
                "after freeing post-handshake" (exchange ?cli ?sfd "after freeing post-handshake"))

   (expect-true "client shutdown" (tls-shutdown ?cli))
   (expect-true "client closes"   (close-connection ?cli))
   (expect-true "server closes"   (close-connection ?sfd))
   (close-connection ?srv))

(free-during-handshake)
(free-after-handshake)
(test-summary)
