;;; TLS contexts and sessions give their descriptors back.
;;;
;;; requires: tls
;;;
;;; descriptor-leaks.clp has the plaintext half of this test, and the
;;; explanation there is also correct here. The check is the count in
;;; /proc/self/fd before and after, and exact equality is the only useful
;;; form.
;;;
;;; TLS adds descriptors that a socket test cannot reach. A certificate load
;;; opens a file, a CA bundle load opens another file, and the code makes and
;;; frees a context for each configuration that a program tries. None of these
;;; are sockets. As a result, no other test in this suite would see them
;;; collect. A server that reloads its certificate on a timer does this in a
;;; loop for as long as it operates.
;;;
;;; The failure paths are more important here than anywhere else. A handshake
;;; that the code refuses already allocated a session, opened files and read a
;;; certificate before the refusal. That path is the least likely one to free
;;; each item correctly.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(load* "tests/lib/tls.clp")
(test-suite "tls-descriptor-leaks")
(test-plan 9)

(defglobal ?*port*   = 19351)
(defglobal ?*ca*     = "tests/fixtures/ca.pem")
(defglobal ?*cert*   = "tests/fixtures/server.pem")
(defglobal ?*key*    = "tests/fixtures/server-key.pem")
(defglobal ?*wild*   = "tests/fixtures/wildcard.pem")
(defglobal ?*wkey*   = "tests/fixtures/wildcard-key.pem")
(defglobal ?*absent* = "tests/fixtures/there-is-no-such-file.pem")

;;; A pair of contexts with their settings. The caller makes them and frees
;;; them.
(deffunction server-context ()
   (bind ?ctx (tls-create-context TLS_SERVER))
   (tls-context-use-certificate-file ?ctx ?*cert*)
   (tls-context-use-private-key-file ?ctx ?*key*)
   (return ?ctx))

(deffunction client-context ()
   (bind ?ctx (tls-create-context TLS_CLIENT))
   (tls-context-load-verify-locations ?ctx ?*ca*)
   (tls-context-set-verify ?ctx SSL_VERIFY_PEER)
   (return ?ctx))

;;; Runs two handshakes. The caller gives the wait between the cycles, and
;;; that argument is the difference between the two halves of this file.
;;;
;;; tls-handshake gives FALSE for "not complete yet" and FALSE for "this never
;;; completes". There is no third value to tell the two conditions apart. As a
;;; result, this function cannot stop early on a refusal. It can only use its
;;; full budget. With a wait of 20ms on each socket, one cycle costs 40ms for
;;; each cycle in the budget. Forty refused handshakes at that rate take more
;;; time than the full suite has.
;;;
;;; As a result, the code runs a refusal with no wait. The two ends are on
;;; loopback, and each byte already arrived. A cycle that finds no data has
;;; nothing to wait for.
(deffunction drive (?cfd ?sfd ?limit ?wait)
   (bind ?client-done FALSE)
   (bind ?server-done FALSE)
   (bind ?i 0)
   (while (and (< ?i ?limit)
               (or (not ?client-done) (not ?server-done))) do
      (if (not ?client-done) then (bind ?client-done (tls-handshake ?cfd)))
      (if (not ?server-done) then (bind ?server-done (tls-handshake ?sfd)))
      (if (not ?client-done) then (poll ?cfd ?wait POLLIN))
      (if (not ?server-done) then (poll ?sfd ?wait POLLIN))
      (bind ?i (+ ?i 1)))
   (and ?client-done ?server-done))

;;; One complete session: the handshake, one line in each direction, the
;;; shutdown and the close.
(deffunction session-cycle (?cctx ?sctx ?limit ?wait)
   (bind ?pair (tcp-connected-pair ?*port*))
   (bind ?srv (nth$ 1 ?pair))
   (bind ?cli (nth$ 2 ?pair))
   (bind ?acc (nth$ 3 ?pair))

   ;; This function does not use tls-connected-pair. That function runs the
   ;; handshake with the usual budget, and this file needs the limit and the
   ;; wait as arguments. See the note on drive above.
   (fcntl-add-status-flags ?cli O_NONBLOCK)
   (fcntl-add-status-flags ?acc O_NONBLOCK)
   (tls-connect ?cctx ?cli localhost)
   (tls-accept ?sctx ?acc)
   (bind ?ok (drive ?cli ?acc ?limit ?wait))
   (fcntl-remove-status-flags ?cli O_NONBLOCK)
   (fcntl-remove-status-flags ?acc O_NONBLOCK)

   (if ?ok then
      (printout (get-socket-logical-name ?cli) "traffic" crlf)
      (flush-connection ?cli)
      (readline (get-socket-logical-name ?acc))
      (tls-shutdown ?cli))

   (close-connection ?cli)
   (close-connection ?acc)
   (close-connection ?srv)
   (return ?ok))

(deffunction run-tests ()
   (if (= (count-open-descriptors) 0) then
      ;; There is one skip for the file, and the plan must change to one. See
      ;; the same code in descriptor-leaks.clp.
      (test-plan 1)
      (test-skip "descriptor accounting" "/proc/self/fd is not readable here")
      (return))

   (bind ?base (count-open-descriptors))

   ;;=================================================================
   ;; Contexts on their own
   ;;=================================================================
   ;; An empty context that the code makes and frees. It must give back each
   ;; descriptor that it opens.
   (bind ?i 0)
   (while (< ?i 100) do
      (tls-free-context (tls-create-context TLS_CLIENT))
      (tls-free-context (tls-create-context TLS_SERVER))
      (bind ?i (+ ?i 1)))
   (expect-eq "200 bare contexts leak nothing" ?base (count-open-descriptors))

   ;;=================================================================
   ;; Contexts that read files
   ;;=================================================================
   ;; This is the condition that a socket test cannot reach. Each cycle opens
   ;; a certificate, a key and a CA bundle. A descriptor that any of the three
   ;; holds shows here and in no other test of this suite.
   (bind ?j 0)
   (while (< ?j 100) do
      (bind ?s (server-context))
      (bind ?c (client-context))
      (tls-free-context ?s)
      (tls-free-context ?c)
      (bind ?j (+ ?j 1)))
   (expect-eq "100 contexts that load certificates leak nothing"
              ?base (count-open-descriptors))

   ;; The code loads the same context again and again. A reload does the
   ;; same.
   (bind ?ctx (tls-create-context TLS_SERVER))
   (bind ?k 0)
   (while (< ?k 100) do
      (tls-context-use-certificate-file ?ctx ?*cert*)
      (tls-context-use-private-key-file ?ctx ?*key*)
      (tls-context-use-certificate-file ?ctx ?*wild*)
      (tls-context-use-private-key-file ?ctx ?*wkey*)
      (bind ?k (+ ?k 1)))
   (expect-eq "400 certificate reloads on one context leak nothing"
              ?base (count-open-descriptors))
   (tls-free-context ?ctx)
   (expect-eq "and freeing it gives everything back" ?base (count-open-descriptors))

   ;;=================================================================
   ;; Files that are not there
   ;;=================================================================
   ;; A load that fails still tried to open a file.
   (bind ?m 0)
   (while (< ?m 100) do
      (bind ?c2 (tls-create-context TLS_CLIENT))
      (tls-context-use-certificate-file ?c2 ?*absent*)
      (tls-context-load-verify-locations ?c2 ?*absent*)
      (tls-free-context ?c2)
      (bind ?m (+ ?m 1)))
   (expect-eq "100 failed certificate loads leak nothing" ?base (count-open-descriptors))

   ;;=================================================================
   ;; Complete sessions
   ;;=================================================================
   (bind ?sctx (server-context))
   (bind ?cctx (client-context))

   (bind ?good 0)
   (bind ?n 0)
   (while (< ?n 40) do
      (if (session-cycle ?cctx ?sctx 200 20) then (bind ?good (+ ?good 1)))
      (bind ?n (+ ?n 1)))

   (expect-eq "all 40 sessions handshook" 40 ?good)
   (expect-eq "40 complete TLS sessions leak nothing" ?base (count-open-descriptors))

   (tls-free-context ?sctx)
   (tls-free-context ?cctx)

   ;;=================================================================
   ;; Handshakes that are refused
   ;;=================================================================
   ;; A client that trusts no authority refuses the certificate of the server
   ;; in the middle of the handshake. As a result, each of these handshakes
   ;; allocates a session and then stops.
   (bind ?sctx2 (server-context))
   (bind ?cctx2 (tls-create-context TLS_CLIENT))
   (tls-context-set-verify ?cctx2 SSL_VERIFY_PEER)

   (bind ?refused 0)
   (bind ?p 0)
   (while (< ?p 40) do
      (if (not (session-cycle ?cctx2 ?sctx2 50 0)) then
         (bind ?refused (+ ?refused 1)))
      (bind ?p (+ ?p 1)))

   (expect-eq "all 40 handshakes were refused" 40 ?refused)
   (expect-eq "40 refused handshakes leak nothing" ?base (count-open-descriptors))

   (tls-free-context ?sctx2)
   (tls-free-context ?cctx2))

(run-tests)
(test-summary)
