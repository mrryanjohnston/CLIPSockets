;;; Each request that a DTLS call cannot do.
;;;
;;; requires: tls
;;;
;;; The checks here use (tls-supports-dtls) and not a fixed answer. The answer
;;; differs between backends, and it changes when a backend adds DTLS. s2n-tls
;;; implements no DTLS. A backend without a stateless cookie exchange can be a
;;; client but cannot be a server. A question to the library is the only form
;;; of this test that stays correct.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(test-suite "dtls-failures")
(test-plan 28)

(defglobal ?*port-a* = 18961)
(defglobal ?*port-b* = 18962)
(defglobal ?*stream-port* = 18963)
(defglobal ?*used-port*   = 18964)

(deffunction run-tests ()
   ;;=================================================================
   ;; What the backend says about itself
   ;;=================================================================
   ;; The two directions must give an answer, and each answer is acceptable.
   ;; A backend with the server half always has the client half. The opposite
   ;; is not always true.
   (bind ?client-ok (tls-supports-dtls DTLS_CLIENT))
   (bind ?server-ok (tls-supports-dtls DTLS_SERVER))
   (expect-true "a backend with a DTLS server also has a DTLS client"
                (or (eq ?server-ok FALSE) (eq ?client-ok TRUE)))
   (expect-eq "the unqualified answer matches the two halves"
              (or ?client-ok ?server-ok) (tls-supports-dtls))
   (expect-false "an unknown role is refused" (tls-supports-dtls TLS_SIDEWAYS))

   ;;=================================================================
   ;; Contexts follow what the backend said
   ;;=================================================================
   (expect-false "an unknown role is refused" (tls-create-context DTLS_SIDEWAYS))

   (bind ?dctx (tls-create-context DTLS_CLIENT))
   (if ?client-ok
      then (expect-gte "a DTLS client context is created" 1 ?dctx)
      else (expect-false "a backend without DTLS refuses the context" ?dctx))

   ;;=================================================================
   ;; A TLS context is not a DTLS context
   ;;=================================================================
   (bind ?tctx (tls-create-context TLS_CLIENT))
   (expect-gte "a TLS client context is created" 1 ?tctx)

   (bind ?pair (udp-connected-pair ?*port-a* ?*port-b*))
   (bind ?dgram      (nth$ 1 ?pair))
   (bind ?other      (nth$ 2 ?pair))
   (bind ?dgram-name (nth$ 3 ?pair))

   (expect-false "dtls-connect refuses a context made for TLS"
                 (dtls-connect ?tctx ?dgram localhost))
   (expect-false "dtls-accept refuses a context made for TLS"
                 (dtls-accept ?tctx ?dgram))

   ;;=================================================================
   ;; Socket type is checked before anything reaches the wire
   ;;=================================================================
   (bind ?stream (create-socket AF_INET SOCK_STREAM))
   ;; This check needs a DTLS context to reach the socket check. As a result,
   ;; a backend without DTLS cannot answer this question.
   (if ?client-ok
      then (expect-false "dtls-connect refuses a stream socket"
                         (dtls-connect ?dctx ?stream localhost))
      else (test-skip "dtls-connect refuses a stream socket"
                      "this backend has no DTLS, so there is no context to ask with"))
   (expect-false "tls-connect refuses a datagram socket"
                 (tls-connect ?tctx ?dgram localhost))

   ;;=================================================================
   ;; The record functions need a DTLS session, not just a socket
   ;;=================================================================
   ;; The socket is real, connected and usable with printout. It has no
   ;; session. Each of these calls must report that condition, and none of
   ;; them must read a NULL pointer.
   (expect-false "dtls-send on a plaintext socket" (dtls-send ?dgram "hello"))
   (expect-false "dtls-recv on a plaintext socket" (dtls-recv ?dgram))
   (expect-false "dtls-timeout on a plaintext socket" (dtls-timeout ?dgram))
   (expect-false "dtls-handle-timeout on a plaintext socket"
                 (dtls-handle-timeout ?dgram))
   (expect-false "dtls-set-mtu on a plaintext socket" (dtls-set-mtu ?dgram 1200))

   ;; A value that was never a socket.
   (expect-false "dtls-send on something that is not a socket"
                 (dtls-send 9999 "hello"))
   ;; This check has the same guard as the others. A backend with no DTLS
   ;; gives no context handle. A FALSE in place of an integer stops the file
   ;; and does not fail one check.
   (if ?client-ok
      then (expect-false "dtls-accept on something that is not a socket"
                         (dtls-accept ?dctx 9999))
      else (test-skip "dtls-accept on something that is not a socket"
                      "this backend has no DTLS, so there is no context to ask with"))

   ;;=================================================================
   ;; Handles and hostnames
   ;;=================================================================
   ;; A context handle that the library never gave. The code checks the
   ;; handle before it reads the socket, and the message names the true
   ;; cause.
   (expect-false "dtls-connect on an unknown context handle"
                 (dtls-connect 9999 ?dgram localhost))
   (expect-false "dtls-accept on an unknown context handle"
                 (dtls-accept 9999 ?dgram))

   ;; The code compares the peer certificate with the hostname. As a result,
   ;; an empty hostname is a request with the important part absent.
   (if ?client-ok
      then (expect-false "an empty hostname is refused"
                         (dtls-connect ?dctx ?dgram ""))
      else (test-skip "an empty hostname is refused"
                      "this backend has no DTLS, so there is no context to ask with"))

   ;;=================================================================
   ;; A socket that has already carried traffic
   ;;=================================================================
   ;; This is the same rule as for streams, and for the same cause. A
   ;; handshake cannot come after bytes that already went out as plaintext.
   (bind ?used (create-socket AF_INET SOCK_DGRAM))
   (setsockopt ?used SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?used 127.0.0.1 ?*used-port*)
   (bind ?used-name (connect ?used 127.0.0.1 ?*port-b*))
   (printout ?used-name "already talking" crlf)
   (flush-connection ?used)

   (if ?client-ok
      then (expect-false "a datagram socket already written to is refused"
                         (dtls-connect ?dctx ?used localhost))
      else (test-skip "a datagram socket already written to is refused"
                      "this backend has no DTLS, so there is no context to ask with"))
   (close-connection ?used)

   ;;=================================================================
   ;; A TLS session is not a DTLS session
   ;;=================================================================
   ;; The record functions need datagrams. With a correct stream session they
   ;; must report that condition, and they must not give part of a stream. As
   ;; a result, this section gives a real TLS connection to each of them.
   ;;
   ;; The two ends are in this process, and the code runs the two halves of
   ;; the handshake in turn. tests/integration/tls-loopback.clp says why that
   ;; is necessary.
   (bind ?lsn (create-socket AF_INET SOCK_STREAM))
   (setsockopt ?lsn SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?lsn 127.0.0.1 ?*stream-port*)
   (listen ?lsn 1)

   (bind ?tctx2 (tls-create-context TLS_SERVER))
   (tls-context-use-certificate-file ?tctx2 "tests/fixtures/server.pem")
   (tls-context-use-private-key-file ?tctx2 "tests/fixtures/server-key.pem")

   (bind ?c (create-socket AF_INET SOCK_STREAM))
   (fcntl-add-status-flags ?c O_NONBLOCK)
   (connect ?c 127.0.0.1 ?*stream-port*)
   (bind ?a (accept ?lsn))
   (fcntl-add-status-flags ?a O_NONBLOCK)

   (tls-accept ?tctx2 ?a)
   (tls-connect ?tctx ?c localhost)
   (bind ?n 0)
   (while (< ?n 40) do
      (tls-handshake ?a)
      (tls-handshake ?c)
      (poll ?a 20 POLLIN)
      (poll ?c 20 POLLIN)
      (bind ?n (+ ?n 1)))

   (expect-false "dtls-send refuses a TLS session" (dtls-send ?c "hello"))
   (expect-false "dtls-recv refuses a TLS session" (dtls-recv ?c))
   (expect-false "dtls-timeout refuses a TLS session" (dtls-timeout ?c))
   (expect-false "dtls-set-mtu refuses a TLS session" (dtls-set-mtu ?c 1200))
   (expect-false "dtls-accept refuses a socket carrying a TLS session"
                 (dtls-accept ?tctx2 ?a))

   ;; A socket that already has a session cannot take a second one.
   (expect-false "tls-connect refuses an already encrypted socket"
                 (tls-connect ?tctx ?c localhost))

   ;; The version names also do not move between the two transports.
   (expect-false "a DTLS version is refused on a TLS context"
                 (tls-context-set-min-proto-version ?tctx DTLS1_2_VERSION))

   (close-connection ?c)
   (close-connection ?a)
   (close-connection ?lsn)
   (tls-free-context ?tctx2)

   (close-connection ?stream)
   (close-connection ?dgram)
   (close-connection ?other)
   (tls-free-context ?tctx)
   (if ?client-ok then (tls-free-context ?dctx)))

(run-tests)
(test-summary)
