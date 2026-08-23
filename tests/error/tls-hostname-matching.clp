;;; What a name in a certificate can cover.
;;;
;;; requires: tls
;;;
;;; The code must not accept a certificate for one name as a certificate for a
;;; different name. The important conditions are the wildcards. There "covers"
;;; is not string equality but a rule, and one library can apply that rule
;;; incorrectly while the other libraries are correct.
;;;
;;; This is more important than it looks. Five of the six backends do this
;;; comparison inside the library, and many people read that code. s2n has no
;;; such call. It gives each name from the certificate of the peer to a
;;; callback and asks for yes or no. As a result, the rule on that backend is
;;; the code of CLIPSockets. These checks hold the two to the same standard.
;;;
;;; No name here goes to a resolver. The code opens the socket to 127.0.0.1 and
;;; verifies only the name that it gives to (tls-connect). As a result, the
;;; names below do not have to exist.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(load* "tests/lib/tls.clp")
(test-suite "tls-hostname-matching")
(test-plan 7)

(defglobal ?*port* = 18941)
(defglobal ?*ca*   = "tests/fixtures/ca.pem")
(defglobal ?*cert* = "tests/fixtures/wildcard.pem")
(defglobal ?*key*  = "tests/fixtures/wildcard-key.pem")

;;; One handshake against the wildcard certificate, with ?name as the name to
;;; verify. Gives TRUE if the client accepted the certificate of the server.
;;; The function closes each socket in both conditions. A handshake that fails
;;; still leaves two sockets and a listen socket.
(deffunction offered-as (?sctx ?name ?port)
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

   (close-connection ?cli)
   (close-connection ?sfd)
   (close-connection ?srv)
   (tls-free-context ?cctx)

   (return ?ok))

(deffunction run-tests ()
   (bind ?sctx (tls-create-context TLS_SERVER))
   (tls-context-use-certificate-file ?sctx ?*cert*)
   (tls-context-use-private-key-file ?sctx ?*key*)

   ;;=================================================================
   ;; The wildcard covers exactly one label
   ;;=================================================================
   ;; This condition must continue to operate. Code that refuses each name
   ;; would pass each other check in this file.
   (expect-true "*.wild.clipsockets covers one label"
                (offered-as ?sctx a.wild.clipsockets ?*port*))

   ;; A wildcard is not the domain itself. A certificate for the subdomains
   ;; of a domain is not a certificate for that domain.
   (expect-false "*.wild.clipsockets does not cover the bare domain"
                 (offered-as ?sctx wild.clipsockets (+ ?*port* 1)))

   ;; A wildcard covers one label and not more. If a star covered dots, a
   ;; certificate for *.example.com would cover each host below that name, at
   ;; any depth and under any owner.
   (expect-false "*.wild.clipsockets does not cover two labels"
                 (offered-as ?sctx a.b.wild.clipsockets (+ ?*port* 2)))

   ;; This file does not check a name that starts with a dot, and that is on
   ;; purpose. Such a name looks like an empty first label and is not one.
   ;; OpenSSL reads a first dot on the requested name as "any subdomain of
   ;; this name". That is a convention of OpenSSL for the requested name, and
   ;; not a rule about the certificate. A check of either answer would make
   ;; the input convention of one library into a security property.

   ;;=================================================================
   ;; An address is not a name
   ;;=================================================================
   ;; The certificate has DNS:*.0.0.1. As a usual wildcard name, that entry
   ;; matches 127.0.0.1: one label and then 0.0.1. But an address matches only
   ;; an address that the certificate names, and this certificate names no
   ;; address.
   ;;
   ;; An error here is more important than it looks. Anyone can get a
   ;; certificate for a name that they control. A wildcard that matched
   ;; addresses would make such a certificate into a certificate for a host
   ;; that they do not control.
   (expect-false "a wildcard does not cover an address"
                 (offered-as ?sctx 127.0.0.1 (+ ?*port* 4)))

   ;; This is the other direction, and it is necessary. Code that refused
   ;; each address would pass the check above and would stop each connection
   ;; to an address. This certificate has IP:127.0.0.1, and it names the
   ;; address of this connection.
   (bind ?ipctx (tls-create-context TLS_SERVER))
   (tls-context-use-certificate-file ?ipctx "tests/fixtures/server.pem")
   (tls-context-use-private-key-file ?ipctx "tests/fixtures/server-key.pem")

   ;; Each library must refuse the wildcard above. But a library cannot
   ;; always accept a correct address. A library that cannot find an iPAddress
   ;; entry in a certificate cannot match one, and a refusal is then the only
   ;; correct answer.
   (if (backend-lacks address-verification)
      then (test-skip "an address the certificate does name is accepted"
                      (str-cat (tls-backend) " " (tls-backend-version)
                               " does not parse iPAddress entries out of subjectAltName"))
      else (expect-true "an address the certificate does name is accepted"
                        (offered-as ?ipctx 127.0.0.1 (+ ?*port* 5))))

   (expect-true "address context frees" (tls-free-context ?ipctx))

   (expect-true "server context frees" (tls-free-context ?sctx)))

(run-tests)
(test-summary)
