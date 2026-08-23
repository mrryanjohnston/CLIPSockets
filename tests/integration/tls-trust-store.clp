;;; What the trust store of a context holds.
;;;
;;; requires: tls
;;;
;;; There are three rules, and each backend must obey them, whatever names its
;;; library uses:
;;;
;;;   A context that names no authority verifies against the system store.
;;;   That is what a caller with no setting means, and it is the safe choice.
;;;   The other reading is an empty store. An empty store refuses each peer,
;;;   and a user would see that immediately. The system store accepts what the
;;;   machine already trusts, and a user would not see that.
;;;
;;;   A named authority becomes trusted.
;;;
;;;   A named authority and a request for the system store trust both of them.
;;;
;;; The fixture CA gives the answer. The suite makes that CA, and no system
;;; trust store has it. As a result, the question "is the fixture CA trusted
;;; here" gives the answer directly. A handshake against the fixture server
;;; completes when the CA is trusted, and it fails when the CA is not trusted.
;;;
;;; This file does not check if a named authority replaces the system store or
;;; adds to it, and that is on purpose. The libraries differ: s2n replaces and
;;; OpenSSL adds. To tell them apart needs a certificate from a real public
;;; authority, and this suite cannot hold one.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(load* "tests/lib/tls.clp")
(test-suite "tls-trust-store")
(test-plan 8)

(defglobal ?*port* = 19441)
(defglobal ?*ca*   = "tests/fixtures/ca.pem")
(defglobal ?*cert* = "tests/fixtures/server.pem")
(defglobal ?*key*  = "tests/fixtures/server-key.pem")

;;; Runs one handshake against a server with the fixture certificate, and
;;; tells if the handshake completed. The client context is the value under
;;; test, and each other value stays the same.
(deffunction fixture-server-accepts (?cctx ?port)
   (bind ?sctx (tls-create-context TLS_SERVER))
   (tls-context-use-certificate-file ?sctx ?*cert*)
   (tls-context-use-private-key-file ?sctx ?*key*)

   (bind ?pair (tcp-connected-pair ?port))
   (bind ?srv (nth$ 1 ?pair))
   (bind ?cli (nth$ 2 ?pair))
   (bind ?sfd (nth$ 3 ?pair))
   (fcntl-add-status-flags ?cli O_NONBLOCK)
   (fcntl-add-status-flags ?sfd O_NONBLOCK)

   (tls-connect ?cctx ?cli localhost)
   (tls-accept ?sctx ?sfd)
   (bind ?ok (tls-drive-handshake ?cli ?sfd 20 5))

   (close-connection ?cli)
   (close-connection ?sfd)
   (close-connection ?srv)
   (tls-free-context ?sctx)

   (return ?ok))

(deffunction run-tests ()
   ;;=================================================================
   ;; Nothing named
   ;;=================================================================
   ;; The system store, which does not have the fixture CA.
   (bind ?c1 (tls-create-context TLS_CLIENT))
   (expect-true  "a client verifies by default"
                 (tls-context-set-verify ?c1 SSL_VERIFY_PEER))
   (expect-false "a context that named no authority does not trust the fixture CA"
                 (fixture-server-accepts ?c1 ?*port*))
   (tls-free-context ?c1)

   ;;=================================================================
   ;; The system store asked for by name
   ;;=================================================================
   ;; The code must accept a request for the default and must not refuse it.
   ;; The request must also leave the store as it was.
   (bind ?c2 (tls-create-context TLS_CLIENT))
   (expect-true  "the default paths can be asked for on a fresh context"
                 (tls-context-set-default-verify-paths ?c2))
   (expect-false "and asking for them does not trust the fixture CA either"
                 (fixture-server-accepts ?c2 (+ ?*port* 1)))
   (tls-free-context ?c2)

   ;;=================================================================
   ;; An authority named
   ;;=================================================================
   (bind ?c3 (tls-create-context TLS_CLIENT))
   (expect-true "the fixture CA loads"
                (tls-context-load-verify-locations ?c3 ?*ca*))
   (expect-true "and naming it makes the fixture server trusted"
                (fixture-server-accepts ?c3 (+ ?*port* 2)))
   (tls-free-context ?c3)

   ;;=================================================================
   ;; Both
   ;;=================================================================
   ;; This check finds a store that the code makes in one pass and that uses
   ;; the named authority as a cause to ignore the request for the system
   ;; store. It also finds two calls that interfere with each other. It cannot
   ;; find a system store that the code does not load, for the cause at the
   ;; top of this file. No certificate here comes from a public authority, and
   ;; an absent system store looks the same as a present one.
   (bind ?c4 (tls-create-context TLS_CLIENT))
   (expect-true "the system store can be added alongside a named authority"
                (tls-context-set-default-verify-paths ?c4))
   (tls-context-load-verify-locations ?c4 ?*ca*)
   (expect-true "and the named authority is still trusted with it there"
                (fixture-server-accepts ?c4 (+ ?*port* 3)))
   (tls-free-context ?c4))

(run-tests)
(test-summary)
