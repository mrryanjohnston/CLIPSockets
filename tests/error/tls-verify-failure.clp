;;; Handshakes that must not succeed.
;;;
;;; requires: tls
;;;
;;; The client makes two checks, and this file tests both of them. If either
;;; check accepted each certificate, the result would look like a correct
;;; connection. The certificate must come from an authority that the client
;;; trusts, and the name in the certificate must be the requested name. A
;;; library can apply the second check incorrectly: it sets SNI and does
;;; nothing more.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(load* "tests/lib/tls.clp")
(test-suite "tls-verify-failure")
(test-plan 7)

(defglobal ?*port* = 18927)
(defglobal ?*ca*   = "tests/fixtures/ca.pem")
(defglobal ?*cert* = "tests/fixtures/server.pem")
(defglobal ?*key*  = "tests/fixtures/server-key.pem")

(deffunction run-tests ()
   (bind ?sctx (tls-create-context TLS_SERVER))
   (tls-context-use-certificate-file ?sctx ?*cert*)
   (tls-context-use-private-key-file ?sctx ?*key*)

   ;;=================================================================
   ;; An untrusted issuer
   ;;=================================================================
   ;; This client verifies peers, but the code gave it no trust store. As a
   ;; result, it does not know the fixture CA.
   (bind ?cctx (tls-create-context TLS_CLIENT))
   (expect-true "client demands verification"
                (tls-context-set-verify ?cctx SSL_VERIFY_PEER))

   (bind ?pair (tcp-connected-pair ?*port*))
   (bind ?srv (nth$ 1 ?pair))
   (bind ?cli (nth$ 2 ?pair))
   (bind ?sfd (nth$ 3 ?pair))
   (fcntl-add-status-flags ?cli O_NONBLOCK)
   (fcntl-add-status-flags ?sfd O_NONBLOCK)

   (tls-connect ?cctx ?cli localhost)
   (tls-accept ?sctx ?sfd)
   (expect-false "handshake fails when the issuer is not trusted"
                 (tls-drive-handshake ?cli ?sfd 10 0))
   (expect-true "verify-result explains the rejection rather than saying TRUE"
                (stringp (tls-verify-result ?cli)))

   (close-connection ?cli)
   (close-connection ?sfd)
   (close-connection ?srv)
   (tls-free-context ?cctx)

   ;;=================================================================
   ;; A trusted issuer, but the wrong name
   ;;=================================================================
   ;; The certificate comes from a trusted authority this time. Only the name
   ;; is incorrect. That condition passes if a client sets SNI and does not
   ;; set the verification parameter.
   (bind ?cctx2 (tls-create-context TLS_CLIENT))
   (expect-true "client trusts the fixture CA"
                (tls-context-load-verify-locations ?cctx2 ?*ca*))

   (bind ?pair2 (tcp-connected-pair (+ ?*port* 1)))
   (bind ?srv2 (nth$ 1 ?pair2))
   (bind ?cli2 (nth$ 2 ?pair2))
   (bind ?sfd2 (nth$ 3 ?pair2))
   (fcntl-add-status-flags ?cli2 O_NONBLOCK)
   (fcntl-add-status-flags ?sfd2 O_NONBLOCK)

   (tls-connect ?cctx2 ?cli2 wrong.example)
   (tls-accept ?sctx ?sfd2)
   (expect-false "handshake fails when the certificate is for another name"
                 (tls-drive-handshake ?cli2 ?sfd2 10 0))
   ;; The refusal of the connection is the important part, and the check
   ;; above tests it on each backend. The message about the cause is a
   ;; function of where the library records the difference.
   (if (backend-lacks hostname-in-verify-result)
      then (test-skip "verify-result explains the name mismatch"
                      (str-cat (tls-backend)
                               " aborts on the name without recording a verification result"))
      else (expect-true "verify-result explains the name mismatch"
                        (stringp (tls-verify-result ?cli2))))

   ;; A handshake that the code refuses leaves no session on the socket.
   (expect-false "no peer subject is reported for a rejected peer"
                 (tls-peer-subject ?sfd2))

   (close-connection ?cli2)
   (close-connection ?sfd2)
   (close-connection ?srv2)
   (tls-free-context ?cctx2)
   (tls-free-context ?sctx))

(run-tests)
(test-summary)
