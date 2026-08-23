;;; What the TLS library in this binary can do and cannot do.
;;;
;;; Only the test files with "requires: tls" load this file. expect.clp never
;;; loads it. CLIPS resolves function names when it parses a construct and not
;;; when the construct runs. As a result, a deffunction that names
;;; (tls-backend) does not load in a TLS=0 build, even if no code calls it.
;;; Such a failure would stop each test that does not use TLS.
;;;
;;; The same rule sets the load sequence. A test must load tests/lib/socket.clp
;;; before this file, because tls-connected-pair below uses tcp-connected-pair
;;; and does not parse without it. This file cannot load socket.clp itself,
;;; because load* reads constructs and a call at the top level of a file is not
;;; a construct. As a result, the test loads socket.clp, and each test that
;;; loads this file loads socket.clp on the line above.
;;;
;;; There are two methods to answer the question "can this backend do X". The
;;; first method is much better than the second one.

;;; Ask the library. The code gives a protocol version to a context, and the
;;; context accepts it or refuses it. As a result, no data here must agree with
;;; the current releases. mbedTLS 3.x gives a different answer from 2.x by
;;; itself, and a wolfSSL build without TLS 1.2 reports that condition. No one
;;; must know the build flag.
(deffunction backend-supports-version (?version)
   (bind ?ctx (tls-create-context TLS_CLIENT))
   (if (eq ?ctx FALSE) then (return FALSE))
   (bind ?ok (tls-context-set-min-proto-version ?ctx ?version))
   (tls-free-context ?ctx)
   (return ?ok))

;;; Declare it, for the items with no question to ask. Each entry is a
;;; statement about the library and not about CLIPSockets. A check behind one
;;; of these entries needs a capability that the backend does not have, and to
;;; run that check would report a defect that does not exist. A defect in
;;; CLIPSockets belongs in the code and not in this list.
;;;
;;; Keep this list as short as possible. A backend that reports less than the
;;; library knows looks like a library that cannot answer, and the entry then
;;; hides a defect in this code. A skip is a statement about a library, and it
;;; needs the same care as any other statement.
;;;
;;; address-verification
;;;   Tells if the library can compare a certificate with an address.
;;;
;;;   mbedTLS 2.x parses only the dNSName and OtherName entries from the
;;;   subjectAltName of a certificate. Its own x509_crt.h says this. As a
;;;   result, an iPAddress entry is not available and no address can match one.
;;;   Version 3.x lists each entry and compares addresses correctly.
;;;
;;;   This entry is about a library that cannot accept an address. It is never
;;;   about a library that accepts the incorrect address. Each backend must
;;;   refuse a wildcard for an address, and no test skips that check. On 2.x
;;;   the check below changes "cannot verify" into "refuses", and that is the
;;;   safe direction.
(deffunction backend-lacks (?feature)
   (switch ?feature
      (case address-verification then
         (and (eq (tls-backend) mbedtls)
              (eq 1 (str-index "2." (tls-backend-version)))))
      (default FALSE)))

;;; Runs the two halves of a TLS handshake that are in this process.
;;;
;;; Neither end can block, for the same cause as in the DTLS function below.
;;; tls-connect waits for a ServerHello that only tls-accept can send, and
;;; tls-accept is in the same thread. The two sockets must already be
;;; non-blocking, and the code must already start the two handshakes.
;;;
;;; The poll call makes this loop end instead of run without result. A
;;; handshake flight can go out in more than one write. LibreSSL sends
;;; change_cipher_spec and Finished separately, and OpenSSL sends them
;;; together. Without TCP_NODELAY the socket keeps the second write until the
;;; peer acknowledges the first one. A loop with no wait uses its full budget
;;; before that acknowledgement arrives.
;;;
;;; The optional fourth argument is the wait time in milliseconds. The default
;;; is 20. Use a smaller value when the tests refuse most of the handshakes. A
;;; handshake that fails never completes, and the loop then always runs to the
;;; end of its budget and uses the full wait in each cycle. The wait here is
;;; only idle time, because poll returns as soon as the peer answers. As a
;;; result, a shorter wait costs a handshake that completes nothing.
(deffunction tls-drive-handshake (?cfd ?sfd ?limit $?wait)
   (bind ?ms (if (> (length$ ?wait) 0) then (nth$ 1 ?wait) else 20))
   (bind ?client-done FALSE)
   (bind ?server-done FALSE)
   (bind ?i 0)
   (while (and (< ?i ?limit)
               (or (not ?client-done) (not ?server-done))) do
      (if (not ?client-done) then (bind ?client-done (tls-handshake ?cfd)))
      (if (not ?server-done) then (bind ?server-done (tls-handshake ?sfd)))
      (if (not ?client-done) then (poll ?cfd ?ms POLLIN))
      (if (not ?server-done) then (poll ?sfd ?ms POLLIN))
      (bind ?i (+ ?i 1)))
   (and ?client-done ?server-done))

;;; The contexts for the fixture certificate: a server with that certificate
;;; and its key, and a client that trusts the CA that made it.
;;;
;;; Gives the client context and then the server context. A test that needs a
;;; different configuration makes its own contexts. Examples are a client that
;;; trusts no authority, a server that sends the incorrect certificate, and a
;;; minimum version. Such a test checks the configuration itself.
(deffunction tls-fixture-contexts ()
   (bind ?sctx (tls-create-context TLS_SERVER))
   (tls-context-use-certificate-file ?sctx "tests/fixtures/server.pem")
   (tls-context-use-private-key-file ?sctx "tests/fixtures/server-key.pem")

   (bind ?cctx (tls-create-context TLS_CLIENT))
   (tls-context-load-verify-locations ?cctx "tests/fixtures/ca.pem")

   (return (create$ ?cctx ?sctx)))

;;; A TLS pair on loopback after the handshake. The two ends are non-blocking.
;;;
;;; The contexts are arguments and this function does not make them. A test
;;; that opens several sessions usually checks that one context can serve all
;;; of them, and it cannot check that if this function makes a new pair each
;;; time. tls-fixture-contexts above makes the usual contexts to give here.
;;;
;;; Gives the client, the accepted socket and the listen socket. It gives FALSE
;;; if the handshake did not complete inside the budget. It gives the listen
;;; socket because the caller must close it. No other code here needs it.
;;;
;;; The two ends stay non-blocking. A caller that reads with readline, which
;;; waits, removes that mode itself. tcp-connected-pair makes the same choice,
;;; and for the same cause.
(deffunction tls-connected-pair (?cctx ?sctx ?port)
   (bind ?pair (tcp-connected-pair ?port))
   (bind ?srv (nth$ 1 ?pair))
   (bind ?cli (nth$ 2 ?pair))
   (bind ?acc (nth$ 3 ?pair))

   ;; On Linux, accept() does not copy O_NONBLOCK from the listen socket. As
   ;; a result, the code must set the accepted end to non-blocking itself.
   (fcntl-add-status-flags ?cli O_NONBLOCK)
   (fcntl-add-status-flags ?acc O_NONBLOCK)

   (tls-connect ?cctx ?cli localhost)
   (tls-accept ?sctx ?acc)

   (if (not (tls-drive-handshake ?cli ?acc 80)) then (return FALSE))

   (return (create$ ?cli ?acc ?srv)))

;;; Runs the two halves of a DTLS handshake that are in this process.
;;;
;;; Neither end can block. The client would wait for a ServerHello that cannot
;;; arrive until the server runs, and the server is in the same thread. The two
;;; sockets must already be non-blocking.
;;;
;;; The two sides are not the same. A client has a session from the dtls-connect
;;; call, and it continues with tls-handshake. A server has no session until a
;;; ClientHello with a correct cookie arrives. As a result, dtls-accept
;;; continues the work of the server. That one call does the exchange and the
;;; handshake after it, because no code here can tell which of the two stages
;;; the server is in.
;;;
;;; This function controls the retransmission, and that is also why the
;;; interface has the retransmission calls. A handshake that never sends a
;;; flight again operates correctly on loopback, where no packet is lost. It
;;; stops on the first link that discards a datagram.
(deffunction dtls-drive-handshake (?cctx ?sctx ?cfd ?sfd ?limit)
   (bind ?client-done FALSE)
   (bind ?server-done FALSE)
   (bind ?started FALSE)
   (bind ?i 0)
   (while (and (< ?i ?limit)
               (or (not ?client-done) (not ?server-done))) do
      (if (not ?started) then
         (bind ?started TRUE)
         (bind ?client-done (dtls-connect ?cctx ?cfd localhost))
       else
         (if (not ?client-done) then
            ;; The code calls this in each cycle and not only when the
            ;; clock gives zero. The value from (dtls-timeout) differs
            ;; between the libraries. Some of them count down to the next
            ;; transmission, and one gives only the length of its interval.
            ;; As a result, a caller cannot use that value to know when to
            ;; send a flight. But each backend knows, and each of them sends
            ;; nothing when there is no flight to send.
            (dtls-handle-timeout ?cfd)
            (bind ?client-done (tls-handshake ?cfd))))

      (if (not ?server-done) then
         (bind ?server-done (dtls-accept ?sctx ?sfd)))

      (poll ?sfd 20 POLLIN)
      (poll ?cfd 20 POLLIN)
      (bind ?i (+ ?i 1)))
   (and ?client-done ?server-done))
