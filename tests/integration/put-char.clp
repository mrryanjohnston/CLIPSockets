;;; put-char to a socket.
;;;
;;; The put-char function writes one byte to a socket. These tests write the
;;; bytes that a string cannot contain, the value 0 among them. They make sure
;;; that a byte keeps its position between two strings, that the function
;;; writes the low eight bits of a wide value, and that a file gets its bytes.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(test-suite "put-char")
(test-plan 11)

(defglobal ?*port* = 19701)
(defglobal ?*file* = "/tmp/clipsockets-put-char.bin")

;;; Reads ?n characters and gives them as a multifield of codes.
(deffunction chars (?name ?n)
   (bind ?out (create$))
   (bind ?i 0)
   (while (< ?i ?n) do
      (bind ?out (create$ ?out (get-char ?name)))
      (bind ?i (+ ?i 1)))
   (return ?out))

(deffunction run-tests ()
   (bind ?pair (tcp-connected-pair ?*port*))
   (bind ?srv (nth$ 1 ?pair))
   (bind ?cli (nth$ 2 ?pair))
   (bind ?acc (nth$ 3 ?pair))
   (bind ?cname (nth$ 4 ?pair))
   (bind ?aname (nth$ 5 ?pair))
   (expect-true "the accepted end reads with no buffer" (set-not-buffered ?acc))

   ;; Bytes of each kind, with the value 0, from the client to the server.
   (foreach ?byte (create$ 0 1 126 127 128 200 255)
      (put-char ?cname ?byte))
   (expect-true "client flush" (flush-connection ?cli))
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

   ;; A file gets its bytes.
   (open ?*file* put-char-out "w")
   (put-char put-char-out 0)
   (put-char put-char-out 255)
   (put-char put-char-out 10)
   (close put-char-out)
   (open ?*file* put-char-in "r")
   (expect-eq "a file gets its bytes" (create$ 0 255 10) (chars put-char-in 3))
   (close put-char-in)
   (remove ?*file*)

   (close-connection ?acc)
   (close-connection ?cli)
   (close-connection ?srv))

(run-tests)
(test-summary)
