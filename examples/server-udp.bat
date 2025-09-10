(deffunction server-echo-once (?fd)
  ; wait until something is readable
  (while (not (poll ?fd 5000 POLLIN)) do
    (printout t "[SERVER] waiting for datagram..." crlf))

  ; receive: (endpoint family bytes buf peerlen)
  (bind ?mf (rcvfrom ?fd))
  (if (eq ?mf FALSE) then
    (printout t "[SERVER] rcvfrom failed (errno " (errno) ", " (errno-sym) ")" crlf)
    (return FALSE))

  (bind ?endpoint (nth$ 2 ?mf))
  (bind ?payload  (nth$ 5 ?mf))
  (printout t "[SERVER] from " ?endpoint " -> " ?payload crlf)

  ; connect to sender so we can printout back via FILE* stream
  (bind ?ip    (nth$ 2 ?mf))
  (bind ?port  (nth$ 3 ?mf))
  (if (or (eq ?ip FALSE) (eq ?port 0)) then
    (printout t "[SERVER] bad endpoint, skipping reply" crlf)
    (return FALSE))

  (connect ?fd ?ip ?port) ; sets logical name on this socket

  (bind ?name (get-socket-logical-name ?fd))
  (set-not-buffered ?fd)
  (printout ?name (str-cat "ECHO: " ?payload))
  (flush-connection ?name)
  TRUE
)

(defrule start =>
  (bind ?sock (create-socket AF_INET SOCK_DGRAM))
  (setsockopt ?sock SOL_SOCKET SO_REUSEADDR 1)
  (bind ?name (bind-socket ?sock 127.0.0.1 9999))
  (println "[SERVER] bound to " ?name)
  (server-echo-once ?sock) ; handle exactly one datagram; duplicate this line to handle more
  (close-connection ?name)
  (println "[SERVER] done."))

(reset)
(run)
(exit)

