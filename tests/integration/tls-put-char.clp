;;; put-char to a socket that has a TLS session.
;;;
;;; requires: tls
;;;
;;; The put-char function writes one byte to a socket, and the TLS session of
;;; the socket encrypts the byte. These tests write the bytes that a string
;;; cannot contain, the value 0 among them, through a session. They make sure
;;; that the session holds the byte until a flush, that a byte keeps its
;;; position between two strings, and that the function writes the low eight
;;; bits of a wide value.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(load* "tests/lib/tls.clp")
(test-suite "tls-put-char")
(test-plan 12)

(defglobal ?*port* = 19702)

;;; Reads ?n characters and gives them as a multifield of codes.
(deffunction chars (?name ?n)
   (bind ?out (create$))
   (bind ?i 0)
   (while (< ?i ?n) do
      (bind ?out (create$ ?out (get-char ?name)))
      (bind ?i (+ ?i 1)))
   (return ?out))

(deffunction run-tests ()
   (bind ?ctxs (tls-fixture-contexts))
   (bind ?cctx (nth$ 1 ?ctxs))
   (bind ?sctx (nth$ 2 ?ctxs))
   (bind ?pair (tls-connected-pair ?cctx ?sctx ?*port*))
   (expect-true "session established" ?pair)
   (bind ?cli (nth$ 1 ?pair))
   (bind ?acc (nth$ 2 ?pair))
   (bind ?srv (nth$ 3 ?pair))
   (fcntl-remove-status-flags ?cli O_NONBLOCK)
   (fcntl-remove-status-flags ?acc O_NONBLOCK)
   (bind ?cname (get-socket-logical-name ?cli))
   (bind ?aname (get-socket-logical-name ?acc))

   ;; Bytes of each kind, with the value 0, from the client to the server.
   ;; The session holds each byte until the flush.
   (foreach ?byte (create$ 0 1 126 127 128 200 255)
      (put-char ?cname ?byte))
   (expect-eq "the session holds the seven bytes" 7 (get-retained-bytes ?cli))
   (expect-true "client flush" (flush-connection ?cli))
   (expect-eq "the flush released them" 0 (get-retained-bytes ?cli))
   (expect-true "server sees the bytes" (poll ?acc 5000 POLLIN))
   (expect-eq "the seven bytes arrive as written"
              (create$ 0 1 126 127 128 200 255) (chars ?aname 7))

   ;; A byte keeps its position between two strings.
   (printout ?cname "a")
   (put-char ?cname 0)
   (printout ?cname "b")
   (expect-true "client flush of the sequence" (flush-connection ?cli))
   (expect-true "server sees the sequence" (poll ?acc 5000 POLLIN))
   (expect-eq "a, 0 and b, in that sequence" (create$ 97 0 98) (chars ?aname 3))

   ;; The function writes the low eight bits of a wide value.
   (put-char ?cname 256)
   (put-char ?cname 321)
   (expect-true "client flush of the wide values" (flush-connection ?cli))
   (expect-true "server sees the two bytes" (poll ?acc 5000 POLLIN))
   (expect-eq "256 gives 0 and 321 gives 65" (create$ 0 65) (chars ?aname 2))

   (tls-shutdown ?cli)
   (close-connection ?acc)
   (close-connection ?cli)
   (close-connection ?srv)
   (tls-free-context ?cctx)
   (tls-free-context ?sctx))

(run-tests)
(test-summary)
