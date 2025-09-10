(defrule go =>
  (bind ?sock (create-socket AF_INET SOCK_DGRAM))
  (bind ?conn (connect ?sock 127.0.0.1 9999))
  (println "[CLIENT] connected to " ?conn)

  (printout t "[CLIENT] enter a message: ")
  (bind ?msg (readline))

  ; send one datagram (newline included)
  (set-not-buffered ?sock)
  (printout ?conn ?msg crlf)
  (flush-connection ?conn)
  (println "[CLIENT] sent.")

  ; wait for reply from server
  (while (not (poll ?sock 5000 POLLIN)) do
    (printout t "[CLIENT] waiting for reply..." crlf))

  (bind ?mf (rcvfrom ?sock))
  (if (eq ?mf FALSE) then
    (println "[CLIENT] rcvfrom failed (errno " (errno) ", " (errno-sym) ")")
  else
    (println "[CLIENT] reply from " (nth$ 2 ?mf) ":" (nth$ 3 ?mf) ": " (nth$ 5 ?mf)))

  (close-connection ?conn)
  (println "[CLIENT] done."))

(reset)
(run)
(exit)

