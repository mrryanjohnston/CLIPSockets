(deffunction server-echo-once (?fd)
  ; wait until something is readable
  (while (not (poll ?fd 5000 POLLIN)) do
    (printout t "[SERVER] waiting for datagram..." crlf))

  ; receive: (family ip port bytes buf)
  (bind ?mf (rcvfrom ?fd))
  (if (eq ?mf FALSE) then
    (printout t "[SERVER] rcvfrom failed (errno " (errno) ", " (errno-sym) ")" crlf)
    (return FALSE))

  (bind ?family  (nth$ 1 ?mf))
  (bind ?ip      (nth$ 2 ?mf))
  (bind ?port    (nth$ 3 ?mf))
  (bind ?payload (nth$ 5 ?mf))

  (printout t "[SERVER] from " ?ip ":" ?port " -> " ?payload crlf)

  (if (or (eq ?ip FALSE) (eq ?port 0)) then
    (printout t "[SERVER] bad endpoint, skipping reply" crlf)
    (return FALSE))

  ; echo back directly using sendto
  (bind ?bytes (sendto ?fd AF_INET ?ip ?port (str-cat "ECHO: " ?payload)))
  (if (eq ?bytes FALSE) then
    (printout t "[SERVER] sendto reply failed (errno " (errno) ", " (errno-sym) ")" crlf)
    (return FALSE))

  (println "[SERVER] sent reply of " ?bytes " bytes.")
  TRUE
)

(defrule start =>
  (bind ?sock (create-socket AF_INET SOCK_DGRAM))
  (setsockopt ?sock SOL_SOCKET SO_REUSEADDR 1)
  (bind ?name (bind-socket ?sock 127.0.0.1 9999))
  (println "[SERVER] bound to " ?name)

  ; handle exactly one datagram
  (server-echo-once ?sock)

  (close-connection ?sock)
  (println "[SERVER] done.")
)

(reset)
(run)
(exit)
