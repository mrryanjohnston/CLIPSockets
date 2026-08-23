;;; What (tls-verify-result) means, and what it must not mean.
;;;
;;; requires: tls
;;;
;;; The usual use of this function must be safe:
;;;
;;;    (if (tls-verify-result ?s) then <trust the peer>)
;;;
;;; In CLIPS each value except FALSE is true. As a result, that test reads each
;;; value except FALSE as permission. The function has one task: it gives
;;; a value other than FALSE only when the code authenticated the peer.
;;;
;;; There are three conditions, and the last one needs this file:
;;;
;;;   TRUE     the code asked for a check, and the peer passed it.
;;;   a string the code asked for a check, and the peer failed it. The string
;;;            gives the cause, and that is why this is not a boolean.
;;;   FALSE    the code asked for no check, and it knows nothing about this
;;;            peer.
;;;
;;; The libraries do not give that last answer themselves. Ask OpenSSL for a
;;; verification result on a connection with no verification, for example on a
;;; server that never asked its client for a certificate. OpenSSL gives
;;; X509_V_OK, because no check ran and no check failed. With that value the
;;; function would answer "yes, authenticated" about a peer that gave no
;;; identity.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(load* "tests/lib/tls.clp")
(test-suite "tls-verify-result")
(test-plan 8)

(defglobal ?*port* = 18953)
(defglobal ?*ca*   = "tests/fixtures/ca.pem")
(defglobal ?*cert* = "tests/fixtures/server.pem")
(defglobal ?*key*  = "tests/fixtures/server-key.pem")

;;; The two ends of one complete handshake. The test asks each end about the
;;; other one. The code keeps them in globals, because the sockets must stay
;;; open while the checks run.
(defglobal ?*cli* = FALSE)
(defglobal ?*sfd* = FALSE)
(defglobal ?*srv* = FALSE)

(deffunction connect-pair (?cctx ?sctx ?name ?port)
   (bind ?*srv* (create-socket AF_INET SOCK_STREAM))
   (setsockopt ?*srv* SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?*srv* 127.0.0.1 ?port)
   (listen ?*srv*)

   (bind ?*cli* (create-socket AF_INET SOCK_STREAM))
   (connect ?*cli* 127.0.0.1 ?port)
   (bind ?*sfd* (accept ?*srv*))
   (fcntl-add-status-flags ?*cli* O_NONBLOCK)
   (fcntl-add-status-flags ?*sfd* O_NONBLOCK)

   (tls-connect ?cctx ?*cli* ?name)
   (tls-accept ?sctx ?*sfd*)

   (return (tls-drive-handshake ?*cli* ?*sfd* 100 5)))

(deffunction close-pair ()
   (close-connection ?*cli*)
   (close-connection ?*sfd*)
   (close-connection ?*srv*))

(deffunction run-tests ()
   (bind ?sctx (tls-create-context TLS_SERVER))
   (tls-context-use-certificate-file ?sctx ?*cert*)
   (tls-context-use-private-key-file ?sctx ?*key*)

   ;;=================================================================
   ;; A peer that was checked and passed
   ;;=================================================================
   (bind ?cctx (tls-create-context TLS_CLIENT))
   (tls-context-load-verify-locations ?cctx ?*ca*)

   (expect-true "handshake completes" (connect-pair ?cctx ?sctx localhost ?*port*))

   ;; The client asked for verification and the check passed. This is the
   ;; only condition where the answer can be TRUE.
   (expect-eq "a verified peer reports TRUE" TRUE (tls-verify-result ?*cli*))

   ;;=================================================================
   ;; The other end of that same handshake
   ;;=================================================================
   ;; The server never asked the client for a certificate, and it knows
   ;; nothing about the client. A TRUE here is the failure that this file
   ;; prevents. The value that means "authenticated" on the line above would
   ;; mean "no check" on this line, and no code could tell them apart.
   (expect-eq "a peer that was never checked reports FALSE"
              FALSE (tls-verify-result ?*sfd*))

   (close-pair)

   ;;=================================================================
   ;; A client that deliberately turned checking off
   ;;=================================================================
   ;; There is no trust store and no verification. This handshake succeeds
   ;; against a certificate that the client cannot check. The caller can make
   ;; that choice, but the function must not then report the peer as
   ;; verified.
   (bind ?open (tls-create-context TLS_CLIENT))
   (expect-true "verification can be turned off"
                (tls-context-set-verify ?open SSL_VERIFY_NONE))

   (expect-true "handshake completes without verification"
                (connect-pair ?open ?sctx localhost (+ ?*port* 1)))
   (expect-eq "an unverified peer reports FALSE"
              FALSE (tls-verify-result ?*cli*))

   (close-pair)
   (tls-free-context ?open)

   (expect-true "client context frees" (tls-free-context ?cctx))
   (expect-true "server context frees" (tls-free-context ?sctx)))

(run-tests)
(test-summary)
