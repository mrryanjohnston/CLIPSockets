;;; Two certificates on one context.
;;;
;;; requires: tls
;;;
;;; A caller gives a context the name of a file, and the context then sends
;;; that certificate. A second file must mean that the context sends the second
;;; certificate. The setting is the certificate, and not the first certificate.
;;;
;;; This check is necessary because the libraries do three different things,
;;; and two of the three give TRUE in each condition:
;;;
;;;   OpenSSL   replaces the pair.
;;;   GnuTLS    keeps the new pair and the old pair, and selects one at the
;;;             handshake.
;;;   s2n       takes the certificate and its key together and does not do
;;;             that two times. It read the second pair, parsed it, reported
;;;             success, and discarded it. The context continued to send the
;;;             first certificate.
;;;
;;; The last one is the important condition. A program that replaces a
;;; certificate usually has a cause for the change. A TRUE result while the old
;;; certificate stays on the network is the worst of the three answers.
;;;
;;; The check uses the behaviour and not a return value. It checks which
;;; certificate the server sends to a peer. That is the part that a caller
;;; needs, and it is the only part where the three libraries can agree.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(load* "tests/lib/tls.clp")
(test-suite "tls-context-replace")
(test-plan 7)

(defglobal ?*port*       = 18947)
(defglobal ?*ca*         = "tests/fixtures/ca.pem")
(defglobal ?*cert*       = "tests/fixtures/server.pem")
(defglobal ?*key*        = "tests/fixtures/server-key.pem")
(defglobal ?*wild-cert*  = "tests/fixtures/wildcard.pem")
(defglobal ?*wild-key*   = "tests/fixtures/wildcard-key.pem")

;;; The subject that the client read. The code keeps it where the caller can
;;; read it, because (offered-subject) closes the session before it
;;; returns.
(defglobal ?*subject* = FALSE)

;;; Connects to a server that uses ?sctx, verifies ?name, and puts the subject
;;; of the certificate that the server sent in ?*subject*. Gives TRUE if the
;;; handshake completed.
(deffunction offered-subject (?sctx ?name ?port)
   (bind ?cctx (tls-create-context TLS_CLIENT))
   (tls-context-load-verify-locations ?cctx ?*ca*)

   (bind ?pair (tcp-connected-pair ?port))
   (bind ?srv (nth$ 1 ?pair))
   (bind ?cli (nth$ 2 ?pair))
   (bind ?sfd (nth$ 3 ?pair))
   (fcntl-add-status-flags ?cli O_NONBLOCK)
   (fcntl-add-status-flags ?sfd O_NONBLOCK)

   (tls-connect ?cctx ?cli ?name)
   (tls-accept ?sctx ?sfd)
   (bind ?ok (tls-drive-handshake ?cli ?sfd 100))

   (bind ?*subject* FALSE)
   (if ?ok then (bind ?*subject* (tls-peer-subject ?cli)))

   (close-connection ?cli)
   (close-connection ?sfd)
   (close-connection ?srv)
   (tls-free-context ?cctx)

   (return ?ok))

(deffunction run-tests ()
   ;;=================================================================
   ;; A context serves what it was last told to serve
   ;;=================================================================
   ;; The wildcard certificate goes on the context first, and this check
   ;; shows that the server uses it. A test that only checked the second
   ;; certificate would also pass if the code never installed the first
   ;; one.
   (bind ?sctx (tls-create-context TLS_SERVER))
   (expect-true "wildcard certificate loads"
                (tls-context-use-certificate-file ?sctx ?*wild-cert*))
   (expect-true "wildcard key loads"
                (tls-context-use-private-key-file ?sctx ?*wild-key*))

   (expect-true "the first certificate is the one presented"
                (offered-subject ?sctx a.wild.clipsockets ?*port*))
   (expect-gte "and it is the wildcard certificate" 1
               (str-index "wild.clipsockets" ?*subject*))

   ;;=================================================================
   ;; Now replace it
   ;;=================================================================
   ;; The test does not check the return values of the two calls, and that is
   ;; on purpose. A backend that keeps a certificate and its key together sees
   ;; a pair that does not agree between the two calls: the new certificate
   ;; and the old key. A report of that condition is correct. The important
   ;; part is the final content of the context.
   (tls-context-use-certificate-file ?sctx ?*cert*)
   (tls-context-use-private-key-file ?sctx ?*key*)

   (expect-true "a replaced certificate is accepted by a peer expecting it"
                (offered-subject ?sctx localhost (+ ?*port* 1)))
   (expect-gte "the second certificate is the one presented" 1
               (str-index "localhost" ?*subject*))

   (expect-true "server context frees" (tls-free-context ?sctx)))

(run-tests)
(test-summary)
