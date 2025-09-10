(defrule go =>
  (bind ?sock (create-socket AF_INET SOCK_DGRAM))
  (println "[CLIENT] socket created: " ?sock)

  (printout t "[CLIENT] enter a message: ")
  (bind ?msg (readline))

  ; send one datagram directly with sendto
  (bind ?bytes (sendto ?sock AF_INET "127.0.0.1" 9999 ?msg))
  (if (eq ?bytes FALSE) then
    (println "[CLIENT] sendto failed (errno " (errno) ", " (errno-sym) ")")
    (close-connection ?sock)
    (return)
  else
    (println "[CLIENT] sent " ?bytes " bytes."))

  ; wait for reply
  (while (not (poll ?sock 5000 POLLIN)) do
    (printout t "[CLIENT] waiting for reply..." crlf))

  (bind ?mf (rcvfrom ?sock))
  (if (eq ?mf FALSE) then
    (println "[CLIENT] rcvfrom failed (errno " (errno) ", " (errno-sym) ")")
  else
    (println "[CLIENT] reply from " (nth$ 2 ?mf) ":" (nth$ 3 ?mf) ": " (nth$ 5 ?mf)))

  (close-connection ?sock)
  (println "[CLIENT] done.")
)

(reset)
(run)
(exit)

