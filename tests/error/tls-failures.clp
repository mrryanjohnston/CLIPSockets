;;; Each request that a TLS call cannot do.
;;;
;;; requires: tls
;;;
;;; None of these requests must reach the network. These checks change an error
;;; into a FALSE and a message on stderr. Without them, the error becomes a
;;; handshake that fails at a less clear point, or a session on a socket that
;;; cannot carry one.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(load* "tests/lib/tls.clp")
(test-suite "tls-failures")
(test-plan 40)

(defglobal ?*port* = 18923)
(defglobal ?*ca*   = "tests/fixtures/ca.pem")
(defglobal ?*cert* = "tests/fixtures/server.pem")
(defglobal ?*key*  = "tests/fixtures/server-key.pem")

(deffunction run-tests ()
   ;; Some checks below write to a peer that went away. Those checks are the
   ;; handshake against a peer that closed, and the write to a closed
   ;; connection at the end. Without this call, the first of them ends the
   ;; process and gives no error. The library decides which check comes
   ;; first.
   (signal SIGPIPE SIG_IGN)

   ;;=================================================================
   ;; Contexts
   ;;=================================================================
   (expect-false "unknown role is refused" (tls-create-context TLS_SIDEWAYS))
   (expect-false "freeing an unknown handle fails" (tls-free-context 9999))

   ;; Each context function must refuse a handle that the library never
   ;; gave.
   (expect-false "load-verify-locations on an unknown handle"
                 (tls-context-load-verify-locations 9999 ?*ca*))
   (expect-false "set-default-verify-paths on an unknown handle"
                 (tls-context-set-default-verify-paths 9999))
   (expect-false "use-certificate-file on an unknown handle"
                 (tls-context-use-certificate-file 9999 ?*cert*))
   (expect-false "use-private-key-file on an unknown handle"
                 (tls-context-use-private-key-file 9999 ?*key*))
   (expect-false "set-verify on an unknown handle"
                 (tls-context-set-verify 9999 SSL_VERIFY_PEER))
   (expect-false "set-min-proto-version on an unknown handle"
                 (tls-context-set-min-proto-version 9999 TLS1_2_VERSION))
   (expect-false "tls-accept on an unknown handle" (tls-accept 9999 0))

   (bind ?ctx (tls-create-context TLS_CLIENT))
   (expect-gte "a client context was created" 1 ?ctx)

   (expect-false "unknown verify mode is refused"
                 (tls-context-set-verify ?ctx SSL_VERIFY_MAYBE))
   (expect-false "unknown protocol version is refused"
                 (tls-context-set-min-proto-version ?ctx SSL3_VERSION))
   ;; Some libraries do not implement both versions. Such a library reports
   ;; that condition. It does not accept the request and then ignore it. This
   ;; test depends on that refusal. As a result, the test asks the question
   ;; first, and the check uses the answer.
   (if (backend-supports-version TLS1_3_VERSION)
      then (expect-true "TLS 1.3 can be asked for"
                        (tls-context-set-min-proto-version ?ctx TLS1_3_VERSION))
      else (test-skip "TLS 1.3 can be asked for"
                      (str-cat (tls-backend) " " (tls-backend-version)
                               " does not implement TLS 1.3")))

   (if (backend-supports-version TLS1_2_VERSION)
      then (expect-true "TLS 1.2 can be asked for"
                        (tls-context-set-min-proto-version ?ctx TLS1_2_VERSION))
      else (test-skip "TLS 1.2 can be asked for"
                      (str-cat (tls-backend) " " (tls-backend-version)
                               " does not implement TLS 1.2")))
   (expect-true  "verification can be turned off"
                 (tls-context-set-verify ?ctx SSL_VERIFY_NONE))
   (expect-true  "verification can be turned back on"
                 (tls-context-set-verify ?ctx SSL_VERIFY_PEER))
   (expect-true  "the system trust store can be asked for"
                 (tls-context-set-default-verify-paths ?ctx))

   ;; Files that do not exist, and files of the incorrect type.
   (expect-false "a missing CA file fails"
                 (tls-context-load-verify-locations ?ctx "/nonexistent/ca.pem"))
   (expect-false "an empty CA file and path fails"
                 (tls-context-load-verify-locations ?ctx ""))
   (expect-false "a missing certificate fails"
                 (tls-context-use-certificate-file ?ctx "/nonexistent/cert.pem"))
   (expect-false "a missing private key fails"
                 (tls-context-use-private-key-file ?ctx "/nonexistent/key.pem"))
   (expect-false "a certificate offered as a private key fails"
                 (tls-context-use-private-key-file ?ctx ?*cert*))

   ;;=================================================================
   ;; Sockets that cannot be upgraded
   ;;=================================================================
   (bind ?fresh (create-socket AF_INET SOCK_STREAM))
   (expect-false "a socket with no logical name cannot be upgraded"
                 (tls-connect ?ctx ?fresh localhost))

   (bind ?dgram (create-socket AF_INET SOCK_DGRAM))
   (bind-socket ?dgram 127.0.0.1 ?*port*)
   (expect-false "a datagram socket cannot be upgraded"
                 (tls-connect ?ctx ?dgram localhost))

   (expect-false "an unknown descriptor cannot be upgraded"
                 (tls-connect ?ctx 9999 localhost))

   ;;=================================================================
   ;; Session functions against a socket carrying no session
   ;;=================================================================
   (expect-false "tls-handshake needs a session" (tls-handshake ?fresh))
   (expect-false "tls-shutdown needs a session" (tls-shutdown ?fresh))
   (expect-false "tls-pending needs a session" (tls-pending ?fresh))
   (expect-false "tls-cipher needs a session" (tls-cipher ?fresh))
   (expect-false "tls-version needs a session" (tls-version ?fresh))
   (expect-false "tls-verify-result needs a session" (tls-verify-result ?fresh))
   (expect-false "tls-peer-subject needs a session" (tls-peer-subject ?fresh))

   ;;=================================================================
   ;; A handshake cannot be layered over a socket already in use
   ;;=================================================================
   (bind ?pair (tcp-connected-pair (+ ?*port* 1)))
   (bind ?srv (nth$ 1 ?pair))
   (bind ?cli (nth$ 2 ?pair))
   (bind ?sfd (nth$ 3 ?pair))
   (bind ?conn (nth$ 4 ?pair))

   ;; This check prevents a write of plaintext before the handshake. Those
   ;; bytes would go out before the ClientHello.
   (printout ?conn "too early" crlf)
   (expect-false "a socket that has already written cannot be upgraded"
                 (tls-connect ?ctx ?cli localhost))

   ;;=================================================================
   ;; A peer that is not speaking TLS
   ;;=================================================================
   ;; The server replies in plaintext before the client starts its handshake.
   ;; As a result, the answer to the ClientHello is not a TLS record. This is
   ;; the one failure that comes at the first SSL_do_handshake call and not
   ;; after a message in each direction. For this cause the socket can stay in
   ;; blocking mode here.
   (bind ?psrv (create-socket AF_INET SOCK_STREAM))
   (setsockopt ?psrv SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?psrv 127.0.0.1 (+ ?*port* 2))
   (listen ?psrv)

   (bind ?pcli (create-socket AF_INET SOCK_STREAM))
   (connect ?pcli 127.0.0.1 (+ ?*port* 2))
   (bind ?pfd (accept ?psrv))

   (printout (get-socket-logical-name ?pfd) "HTTP/1.1 200 OK" crlf)
   (flush-connection ?pfd)
   ;; The server then closes. Without that close, the library decides if this
   ;; check fails. As a TLS record, "HTTP/1.1 " gives a length of twenty
   ;; thousand bytes. A library that uses the length before it refuses the
   ;; type waits for the remainder on a blocking socket. Each library agrees
   ;; on a peer that sends data that is not TLS and then goes away.
   (close-connection ?pfd)

   ;;=================================================================
   ;; Refusals that come before anything reaches the wire
   ;;=================================================================
   ;; One connected pair, for the two refusals below. The socket must operate
   ;; correctly. The code makes these checks in sequence. As a result, a
   ;; socket that an earlier check refuses never reaches the check under test.
   ;; For example, the code refuses a socket with no connection because it has
   ;; no logical name, and that check comes long before the check of the
   ;; hostname.
   (bind ?iosrv (create-socket AF_INET SOCK_STREAM))
   (setsockopt ?iosrv SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?iosrv 127.0.0.1 (+ ?*port* 6))
   (listen ?iosrv)
   (bind ?iocli (create-socket AF_INET SOCK_STREAM))
   (bind ?ioname (connect ?iocli 127.0.0.1 (+ ?*port* 6)))
   (bind ?ioacc (accept ?iosrv))
   (bind ?ioctx (tls-create-context TLS_CLIENT))

   ;; The code compares the peer certificate with the hostname. As a result,
   ;; an empty hostname is not a request to skip the check. It is a request
   ;; with the important part absent, and the code refuses it.
   (expect-false "an empty hostname is refused"
                 (tls-connect ?ioctx ?iocli ""))

   ;; A handshake cannot operate on a socket that already carried data. The
   ;; data in the stdio buffer would go out as plaintext before the
   ;; ClientHello. This check is second, because it makes the socket unusable
   ;; for the check above.
   (printout ?ioname "already talking" crlf)
   (flush-connection ?iocli)
   (expect-false "a socket that has already been written to is refused"
                 (tls-connect ?ioctx ?iocli localhost))

   (close-connection ?iocli)
   (close-connection ?ioacc)
   (close-connection ?iosrv)
   (tls-free-context ?ioctx)

   (bind ?ctx2 (tls-create-context TLS_CLIENT))
   (tls-context-set-verify ?ctx2 SSL_VERIFY_NONE)
   (expect-false "a handshake with a plaintext peer fails"
                 (tls-connect ?ctx2 ?pcli localhost))
   ;; The attempt that failed also freed its session, and the socket is now a
   ;; plain socket.
   (expect-false "the socket is left unencrypted" (tls-pending ?pcli))

   (close-connection ?pcli)
   (close-connection ?psrv)
   (tls-free-context ?ctx2)

   ;;=================================================================
   ;; Writing to a peer that has gone
   ;;=================================================================
   (bind ?dsrv (create-socket AF_INET SOCK_STREAM))
   (setsockopt ?dsrv SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?dsrv 127.0.0.1 (+ ?*port* 3))
   (listen ?dsrv)
   (bind ?dcli (create-socket AF_INET SOCK_STREAM))
   (connect ?dcli 127.0.0.1 (+ ?*port* 3))
   (bind ?dfd (accept ?dsrv))
   (close-connection ?dfd)

   (bind ?dname (get-socket-logical-name ?dcli))
   (printout ?dname "into the void" crlf)
   ;; This socket is plaintext. As a result, this check covers the same shape
   ;; of failure that the TLS write path reports: the code could not send the
   ;; buffer.
   (flush-connection ?dcli)
   (expect-true "the process survived writing to a closed peer" TRUE)

   (close-connection ?dcli)
   (close-connection ?dsrv)
   (signal SIGPIPE SIG_DFL)

   (expect-true "the backend reports a name" (tls-backend))

   (expect-true "context frees" (tls-free-context ?ctx))

   (close-connection ?cli)
   (close-connection ?sfd)
   (close-connection ?srv)
   (close-connection ?fresh)
   (close-connection ?dgram))

(run-tests)
(test-summary)
