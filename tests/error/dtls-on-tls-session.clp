;;; A DTLS call on a socket that carries a TLS session.
;;;
;;; requires: tls
;;;
;;; tests/error/dtls-failures.clp checks the calls that name a context of the
;;; wrong kind, or a socket of the wrong type. This file checks the condition
;;; after a handshake completed: the socket is encrypted, the session is real,
;;; and the transport is a stream. A record call has nothing to operate on
;;; there, because a stream has no record boundaries to send or receive.
;;;
;;; The two ends of the pair are in this process, so the checks use the client
;;; end alone. Which end carries the session does not matter here. What matters
;;; is that the session is a TLS one and not a DTLS one.
;;;
;;; The last section needs a DTLS server context to have something to give
;;; dtls-accept, and a backend without a stateless cookie exchange has none.
;;; That section asks the library and skips its checks when the answer is no,
;;; in the manner of tests/error/dtls-failures.clp. The sections above it need
;;; no DTLS at all: they refuse the call before any library code runs, so they
;;; operate on every backend that has TLS.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(load* "tests/lib/tls.clp")
(test-suite "dtls-on-tls-session")
(test-plan 11)

(defglobal ?*port* = 19281)

(deffunction run-tests ()
   (bind ?ctx (tls-fixture-contexts))
   (bind ?cctx (nth$ 1 ?ctx))
   (bind ?sctx (nth$ 2 ?ctx))

   (bind ?pair (tls-connected-pair ?cctx ?sctx ?*port*))
   (bind ?cli (nth$ 1 ?pair))
   (bind ?acc (nth$ 2 ?pair))
   (bind ?lsn (nth$ 3 ?pair))

   (expect-true "the pair is handshaked" (tls-version ?cli))

   ;;=================================================================
   ;; The record calls
   ;;=================================================================
   ;; Each of these reaches the socket through the same check. They are
   ;; separate checks because they are separate entry points, and a new one
   ;; that forgets the check would pass if only its neighbour were tested.
   (expect-false "dtls-send refuses a stream session"
                 (dtls-send ?cli "payload"))
   (expect-false "dtls-recv refuses a stream session"
                 (dtls-recv ?cli))
   (expect-false "dtls-set-mtu refuses a stream session"
                 (dtls-set-mtu ?cli 1200))
   (expect-false "dtls-timeout refuses a stream session"
                 (dtls-timeout ?cli))
   (expect-false "dtls-handle-timeout refuses a stream session"
                 (dtls-handle-timeout ?cli))

   ;; The message is the part of the answer that names the cause. FALSE alone
   ;; does not say that the transport is the problem, and a caller that gave a
   ;; stream by mistake needs to read that.
   (capture-start)
   (dtls-send ?cli "payload")
   (bind ?said (capture-lines))
   (expect-contains "dtls-send names itself and the cause"
                    "dtls-send: socket carries a TLS session, not a DTLS one; a stream has no records to send or receive"
                    ?said)

   ;; The session is unchanged by a call that it refused. A refusal that broke
   ;; the session would turn a caller's mistake into a lost connection.
   (printout (get-socket-logical-name ?cli) "still here" crlf)
   (flush-connection ?cli)
   (expect-true "the session still carries data afterwards"
                (poll ?acc 5000 POLLIN))
   (expect-eq "and the data arrives unchanged"
              "still here" (readline (get-socket-logical-name ?acc)))

   ;;=================================================================
   ;; dtls-accept
   ;;=================================================================
   ;; dtls-accept makes this check itself, because it takes a socket that has
   ;; no session yet in the usual case. Reaching it needs a context that the
   ;; backend agrees to make.
   (if (not (tls-supports-dtls DTLS_SERVER))
      then
      (test-skip "dtls-accept refuses a stream session"
                 "this TLS backend has no DTLS server")
      (test-skip "dtls-accept names itself and the cause"
                 "this TLS backend has no DTLS server")
      else
      (bind ?dctx (tls-create-context DTLS_SERVER))
      (expect-false "dtls-accept refuses a stream session"
                    (dtls-accept ?dctx ?cli))

      (capture-start)
      (dtls-accept ?dctx ?cli)
      (bind ?heard (capture-lines))
      (expect-contains "dtls-accept names itself and the cause"
                       "dtls-accept: socket carries a TLS session, not a DTLS one"
                       ?heard)
      (tls-free-context ?dctx))

   (close-connection ?cli)
   (close-connection ?acc)
   (close-connection ?lsn)
   (tls-free-context ?cctx)
   (tls-free-context ?sctx))

(run-tests)
(test-summary)
