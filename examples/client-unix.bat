(defrule a =>
	(bind ?socket (create-socket AF_UNIX SOCK_STREAM))
	(bind ?connection (connect ?socket /tmp/CLIPSockets-tmp-socket))
	(println "uh... " ?connection)
	(bind ?name (get-socket-logical-name ?connection))
	(println "[CLIENT] Connected to " ?name)
	(println "[CLIENT] Type a message to send to server")
	(printout t "[CLIENT] followed by the ENTER key: ")
	(bind ?msg (readline))
	(printout ?name ?msg crlf)
	(println "[CLIENT] Wrote message. Waiting for response...")
	(flush-connection ?connection)
	(println "[CLIENT] The server said: " (readline ?name))
)
(reset)
(run)
(exit)