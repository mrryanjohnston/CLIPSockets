;;; Client authentication: a server that asks for a certificate and applies
;;; the rule.
;;;
;;; requires: tls
;;;
;;; (tls-context-set-verify ?ctx SSL_VERIFY_PEER) on a server context says that
;;; the peer must give its identity. The refusal is the full value of that
;;; setting. If the server lets through a client that gives no certificate, the
;;; setting does nothing except make the program look correct.
;;;
;;; This needs a file of its own, because the four libraries do not agree on
;;; the meaning of "verify the peer" on a server. They also give no message
;;; about the difference:
;;;
;;;   mbedTLS, GnuTLS   ask for a certificate and fail the handshake without
;;;                     one. That is the correct behaviour.
;;;   OpenSSL           asks, checks the certificate that comes back, and
;;;                     accepts a client that sends none.
;;;                     SSL_VERIFY_FAIL_IF_NO_PEER_CERT closes that hole, and
;;;                     the code must set that flag.
;;;   s2n               has no equivalent setting on this path. Client
;;;                     authentication there is a policy of its own.
;;;
;;; Left alone, the libraries make the same CLIPS program safe or unsafe as a
;;; function of the build, and no message gives the answer. The checks below do
;;; not depend on a backend, and that is on purpose. They give the meaning of
;;; the function, and each backend must give that meaning.
;;;
;;; The two directions are both important. The refusal of the client with no
;;; certificate is the security property. The acceptance of the client with a
;;; correct certificate stops code that refuses each client from passing as a
;;; correction.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(load* "tests/lib/tls.clp")
(test-suite "tls-client-auth")
(test-plan 18)

(defglobal ?*port*       = 18935)
(defglobal ?*ca*         = "tests/fixtures/ca.pem")
(defglobal ?*cert*       = "tests/fixtures/server.pem")
(defglobal ?*key*        = "tests/fixtures/server-key.pem")
(defglobal ?*client-crt* = "tests/fixtures/client.pem")
(defglobal ?*client-key* = "tests/fixtures/client-key.pem")

;;; The subject that the server read from the certificate of the client. The
;;; code keeps it where the caller can read it, because (attempt) closes the
;;; two ends before it returns and the certificate goes with them.
(defglobal ?*subject* = FALSE)

;;; A server context that needs a certificate from each client, and that
;;; trusts the fixture CA to have made that certificate.
(deffunction demanding-server ()
   (bind ?ctx (tls-create-context TLS_SERVER))
   (tls-context-use-certificate-file ?ctx ?*cert*)
   (tls-context-use-private-key-file ?ctx ?*key*)
   (tls-context-load-verify-locations ?ctx ?*ca*)
   (return ?ctx))

;;; One attempt, with the given client context. Gives TRUE if the two ends
;;; completed a handshake. The function closes each socket in both conditions,
;;; because a handshake that fails still leaves two sockets and a listen
;;; socket.
(deffunction attempt (?cctx ?sctx ?port)
   (bind ?pair (tcp-connected-pair ?port))
   (bind ?srv (nth$ 1 ?pair))
   (bind ?cli (nth$ 2 ?pair))
   (bind ?sfd (nth$ 3 ?pair))

   (fcntl-add-status-flags ?cli O_NONBLOCK)
   (fcntl-add-status-flags ?sfd O_NONBLOCK)

   (tls-connect ?cctx ?cli localhost)
   (tls-accept ?sctx ?sfd)

   (bind ?ok (tls-drive-handshake ?cli ?sfd 200))

   ;; The code keeps this value so that the caller can read it before the
   ;; function closes the sockets.
   (bind ?*subject* FALSE)
   (if ?ok then (bind ?*subject* (tls-peer-subject ?sfd)))

   (close-connection ?cli)
   (close-connection ?sfd)
   (close-connection ?srv)

   (return ?ok))

(deffunction run-tests ()
   ;;=================================================================
   ;; The setting is accepted at all
   ;;=================================================================
   (bind ?sctx (demanding-server))
   (expect-gte  "server context created" 1 ?sctx)
   (expect-true "a server can be told to require a peer certificate"
                (tls-context-set-verify ?sctx SSL_VERIFY_PEER))

   ;;=================================================================
   ;; A client with nothing to show must be turned away
   ;;=================================================================
   (bind ?anon (tls-create-context TLS_CLIENT))
   (expect-gte  "anonymous client context created" 1 ?anon)
   (expect-true "anonymous client trusts the fixture CA"
                (tls-context-load-verify-locations ?anon ?*ca*))

   (expect-false "a client presenting no certificate is refused"
                 (attempt ?anon ?sctx ?*port*))

   (expect-true "anonymous client context frees" (tls-free-context ?anon))

   ;;=================================================================
   ;; A client with a certificate from the trusted CA must be let in
   ;;=================================================================
   (bind ?known (tls-create-context TLS_CLIENT))
   (expect-gte  "certified client context created" 1 ?known)
   (expect-true "certified client trusts the fixture CA"
                (tls-context-load-verify-locations ?known ?*ca*))
   (expect-true "client certificate loads"
                (tls-context-use-certificate-file ?known ?*client-crt*))
   (expect-true "client key loads"
                (tls-context-use-private-key-file ?known ?*client-key*))

   (expect-true "a client presenting a trusted certificate is admitted"
                (attempt ?known ?sctx (+ ?*port* 1)))
   (expect-gte  "the server sees the client's subject" 1
                (str-index "CLIPSockets Test Client" ?*subject*))

   (expect-true "certified client context frees" (tls-free-context ?known))

   ;;=================================================================
   ;; The server can also remove the condition
   ;;=================================================================
   ;; A server that stops to ask for a certificate must also stop to refuse
   ;; clients. Without that, the setting operates in one direction only.
   (expect-true "the requirement can be turned off"
                (tls-context-set-verify ?sctx SSL_VERIFY_NONE))

   (bind ?anon2 (tls-create-context TLS_CLIENT))
   (expect-true "second anonymous client trusts the fixture CA"
                (tls-context-load-verify-locations ?anon2 ?*ca*))
   (expect-true "once the requirement is lifted, an anonymous client is admitted"
                (attempt ?anon2 ?sctx (+ ?*port* 2)))
   (expect-true "second anonymous client context frees" (tls-free-context ?anon2))

   (expect-true "server context frees" (tls-free-context ?sctx)))

(run-tests)
(test-summary)
