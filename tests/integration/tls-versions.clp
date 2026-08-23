;;; The protocol version that a session agrees on.
;;;
;;; requires: tls
;;;
;;; A request for a minimum version on a context is one operation. A session
;;; that uses that version is a different operation, and only the second one
;;; has value. The test gives each version to the library first, and the checks
;;; after that use the answer. A backend without TLS 1.3 skips the 1.3 checks
;;; and does not fail them, and the same rule applies to 1.2. Two statements
;;; are true on each backend: a session reports a version, and that version is
;;; one of the two versions that this library accepts.
;;;
;;; The code can set only the minimum. As a result, a minimum of 1.3 gives
;;; exactly one version, and a minimum of 1.2 permits the two versions.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(load* "tests/lib/tls.clp")
(test-suite "tls-versions")
(test-plan 8)

(defglobal ?*port* = 18961)
(defglobal ?*ca*   = "tests/fixtures/ca.pem")
(defglobal ?*cert* = "tests/fixtures/server.pem")
(defglobal ?*key*  = "tests/fixtures/server-key.pem")

;;; Runs one exchange with ?version as the minimum on the two ends. Gives the
;;; protocol that the session agreed on, or FALSE if the session never
;;; started.
(deffunction negotiate (?version ?port)
   (bind ?sctx (tls-create-context TLS_SERVER))
   (tls-context-use-certificate-file ?sctx ?*cert*)
   (tls-context-use-private-key-file ?sctx ?*key*)
   (tls-context-set-min-proto-version ?sctx ?version)

   (bind ?cctx (tls-create-context TLS_CLIENT))
   (tls-context-load-verify-locations ?cctx ?*ca*)
   (tls-context-set-min-proto-version ?cctx ?version)

   (bind ?pair (tcp-connected-pair ?port))
   (bind ?srv (nth$ 1 ?pair))
   (bind ?cli (nth$ 2 ?pair))
   (bind ?sfd (nth$ 3 ?pair))

   (fcntl-add-status-flags ?cli O_NONBLOCK)
   (fcntl-add-status-flags ?sfd O_NONBLOCK)
   (tls-connect ?cctx ?cli localhost)
   (tls-accept ?sctx ?sfd)

   (bind ?established (tls-drive-handshake ?cli ?sfd 200))
   (bind ?version-in-use FALSE)
   (if ?established then (bind ?version-in-use (tls-version ?cli)))

   (close-connection ?cli)
   (close-connection ?sfd)
   (close-connection ?srv)
   (tls-free-context ?cctx)
   (tls-free-context ?sctx)

   (return ?version-in-use))

;;; There are two checks for each version, and the test runs them or skips
;;; them. As a result, the plan is the same number on each backend.
(deffunction check-version (?version ?label ?expected ?port)
   (if (not (backend-supports-version ?version))
      then
         (bind ?why (str-cat (tls-backend) " " (tls-backend-version)
                             " does not implement " ?label))
         (test-skip (str-cat ?label " establishes a session") ?why)
         (test-skip (str-cat ?label " is what gets negotiated") ?why)
         (return TRUE))

   (bind ?in-use (negotiate ?version ?port))
   (expect-true (str-cat ?label " establishes a session") ?in-use)
   (expect-contains (str-cat ?label " is what gets negotiated") ?in-use ?expected))

(deffunction run-tests ()
   ;; With a minimum of 1.3, there is only one version to agree on.
   (check-version TLS1_3_VERSION "TLS 1.3" (create$ TLSv1.3) ?*port*)

   ;; A minimum of 1.2 permits 1.2 and each version above it. As a result,
   ;; the two answers are correct.
   (check-version TLS1_2_VERSION "TLS 1.2" (create$ TLSv1.2 TLSv1.3) (+ ?*port* 1))

   ;;=================================================================
   ;; True of every backend, whichever versions it was built with
   ;;=================================================================
   (expect-true "at least one of TLS 1.2 and TLS 1.3 is available"
                (or (backend-supports-version TLS1_2_VERSION)
                    (backend-supports-version TLS1_3_VERSION)))

   (expect-false "a version this library predates is refused"
                 (backend-supports-version SSL3_VERSION))

   ;; The default version must be one of the two versions that are still safe
   ;; to use. A default that changed to TLS 1.0 would pass each other test in
   ;; the suite.
   (bind ?default (negotiate TLS1_2_VERSION (+ ?*port* 2)))
   (expect-contains "the default session speaks a version still fit to use"
                    ?default (create$ TLSv1.2 TLSv1.3))

   (expect-true "the backend reports a version string"
                (stringp (tls-backend-version))))

(run-tests)
(test-summary)
