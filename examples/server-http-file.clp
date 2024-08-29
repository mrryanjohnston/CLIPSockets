(deffacts methods (method GET 71 69 84))
(deffacts protocols
	(protocol HTTP/1.0 72 84 84 80 47 49 46 48)
	(protocol HTTP/1.1 72 84 84 80 47 49 46 49)
)
(deffacts routes
	(route /styles.css 47 115 116 121 108 101 115 46 99 115 115))

(defrule return-styles
	(method GET $?GET)
	(protocol HTTP/1.0 $?HTTP10)
	(protocol HTTP/1.1 $?HTTP11)
	(route /styles.css $?styles)
	?f <- (socket
		(fd ?sfd)
		(clients-connected ?clientsConnected))
	?c <- (client
		(socketfd ?sfd)
		(name ?name)
		(raw-ascii-codes
			$?GET
			32
			$?styles
			32
			$?HTTP10|$?HTTP11
			$?rest))
	=>
	(printout ?name
		"HTTP/1.1 200 OK" crlf "Content-Type: text/css" crlf crlf
	)
	(open examples/server-http-file.css server-http-file.css)
	(bind ?styles (readline server-http-file.css))
	(while (neq EOF ?styles)
		(printout ?name ?styles crlf)
		(bind ?styles (readline server-http-file.css)))
	(close server-http-file.css)
	(flush-connection ?name)
	(empty-connection ?name)
	(shutdown-connection ?name)
	(close-connection ?name)
	(retract ?c)
	(modify ?f
		(current-time (time))
		(client-waiting nil)
		(clients-connected (- ?clientsConnected 1))))

(defrule end-of-message-received-respond-to-client
"We respond to the client once we get a new line character as signified by ascii 10"
	(method GET $?GET)
	(protocol HTTP/1.0 $?HTTP10)
	(protocol HTTP/1.1 $?HTTP11)
	(route /styles.css $?styles)
	?f <- (socket
		(fd ?sfd)
		(clients-connected ?clientsConnected))
	?c <- (client
		(socketfd ?sfd)
		(name ?name)
		(raw-ascii-codes
			$?GET
			32
			$?path
				&~$?styles
				&:(not (member$ (create$ 46 46) ?path))
				&:(not (member$ 32 ?path))
			32
			$?HTTP10|$?HTTP11
			$?rest))
	(not (begin-client-directory-request ?c ?))
	(not (served ?c))
	=>
	(bind ?filepath ".")
	(loop-for-count (?cnt 1 (length$ ?path))
		(bind ?filepath (format nil "%s%c" ?filepath (nth$ ?cnt ?path))))
	(assert (begin-client-directory-request ?c ?filepath)))

(defrule ensure-directory-exists
	(begin-client-directory-request ? ?filepath)
	(not (directory-exists $? ?filepath))
	=>
	(assert
		(directory-exists (scandir ?filepath) ?filepath)))

(defrule ensure-file-exists
	(begin-client-directory-request ? ?filepath)
	(not (file-exists ? ?filepath))
	=>
	(assert
		(file-exists (open ?filepath ?filepath) ?filepath)))

(defrule redirect-display-directory
	(method GET $?GET)
	(protocol HTTP/1.0 $?HTTP10)
	(protocol HTTP/1.1 $?HTTP11)
	(route /styles.css $?styles)
	?c <- (client
		(socketfd ?sfd)
		(name ?name)
		(raw-ascii-codes
			$?GET
			32
			$?path
				&~$?styles
				&:(> (length$ ?path) 0)
				&:(not (member$ 32 ?path))
				&:(not (member$ (create$ 46 46) ?path))
				&:(not (= 47 (nth$ (length$ ?path) ?path)))
			32
			$?HTTP10|$?HTTP11
			$?rest))
	?b <- (begin-client-directory-request ?c ?directory)
	(directory-exists $?entries&:(neq (create$ FALSE) ?entries) ?directory)
	(not (served ?c))
	=>
	(printout ?name
		"HTTP/1.1 301 Moved Permanently" crlf "Content-Length: 0" crlf "Location: "
	)
	(loop-for-count (?cnt 1 (length$ ?path))
		(format ?name "%c" (nth$ ?cnt ?path)))
	(printout ?name "/" crlf crlf)
	(assert (served ?c)))

(defrule display-directory
	?c <- (client (name ?name))
	(begin-client-directory-request ?c ?directory)
	(directory-exists $?entries&:(neq (create$ FALSE) ?entries) ?directory)
	(not (served ?c))
	=>
	(printout ?name
		"HTTP/1.1 200 OK" crlf "Content-Type: text/html" crlf crlf
		"<!DOCTYPE html>"
		"<html><head><link rel=\"stylesheet\" type=\"text/css\" href=\"/styles.css\" /></head><body><ul>"
	)
	(loop-for-count (?cnt 1 (length$ ?entries))
		(bind ?entry (nth$ ?cnt ?entries))
		(if
			(and
				(neq . ?entry)
				(neq .. ?entry))
			then
			(printout ?name "<li><a href=\"./" ?entry "\">" ?entry "</a></li>" crlf))
	)
	(printout ?name "</ul></body></html>")
	(assert (served ?c)))

(defrule display-file
	?c <- (client (name ?name))
	(begin-client-directory-request ?c ?filepath)
	?d <- (directory-exists $?entries&:(eq (create$ FALSE) ?entries) ?filepath)
	?f <- (file-exists TRUE ?filepath)
	(not (served ?c))
	=>
	(printout ?name
		"HTTP/1.1 200 OK" crlf "Content-Type: " (mimetype ?filepath) crlf crlf
	)
	(bind ?file-contents (readline ?filepath))
	(while (neq EOF ?file-contents)
		(printout ?name ?file-contents crlf)
		(bind ?file-contents (readline ?filepath)))
	(assert (served ?c)))

(defrule not-found
	?c <- (client (name ?name))
	(begin-client-directory-request ?c ?path)
	?d <- (directory-exists $?entries&:(eq (create$ FALSE) ?entries) ?path)
	?f <- (file-exists FALSE ?path)
	(not (served ?c))
	=>
	(printout ?name "HTTP/1.1 404 Not Found" crlf crlf)
	(assert (served ?c)))

(defrule end-request
	(declare (salience 1))
	?f <- (socket
		(fd ?sfd)
		(clients-connected ?clientsConnected))
	?c <- (client
		(socketfd ?sfd)
		(name ?name))
	?b <- (begin-client-directory-request ?c ?filepath)
	?s <- (served ?c)
	=>
	(flush-connection ?name)
	(empty-connection ?name)
	(shutdown-connection ?name)
	(close-connection ?name)
	(retract ?b ?c ?s)
	(modify ?f
		(current-time (time))
		(client-waiting nil)
		(clients-connected (- ?clientsConnected 1))))
