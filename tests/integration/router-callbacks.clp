;;; The router callbacks a socket logical name is wired to.
;;;
;;; A socket is registered with CLIPS as an I/O router, so the ordinary reading
;;; and writing commands work on one. printout reaches the write callback and
;;; readline the read callback, both covered wherever this suite moves data.
;;; Tokenised reading is what reaches the unget callback: the scanner has to
;;; push back the delimiter once it has read past the end of a token.
;;;
;;; Closing by logical name takes a different route through the router list
;;; than closing by descriptor, so both are worth exercising.

(load* "tests/lib/expect.clp")
(test-suite "router-callbacks")
(test-plan 11)

(defglobal ?*port* = 18991)

(deffunction run-tests ()
   (bind ?srv (create-socket AF_INET SOCK_STREAM))
   (setsockopt ?srv SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?srv 127.0.0.1 ?*port*)
   (listen ?srv)
   (bind ?cli (create-socket AF_INET SOCK_STREAM))
   (bind ?conn (connect ?cli 127.0.0.1 ?*port*))
   (bind ?acc (accept ?srv))
   (bind ?aname (get-socket-logical-name ?acc))

   ;; A line of separate tokens: reading them one at a time makes the scanner
   ;; push back each delimiter it reads past.
   (printout ?conn "hello 42 3.5 \"a string\"" crlf)
   (flush-connection ?cli)
   (expect-eq "a symbol token" hello      (read ?aname))
   (expect-eq "an integer token" 42       (read ?aname))
   (expect-eq "a float token" 3.5         (read ?aname))
   (expect-eq "a string token" "a string" (read ?aname))

   ;; Reading tokens stops at the last one, so the newline that ended that
   ;; line is still waiting and the next readline returns what is left of it.
   (expect-eq "the remainder of the tokenised line" "" (readline ?aname))

   ;; Reading whole lines still works on the same socket afterwards.
   (printout ?conn "a whole line" crlf)
   (flush-connection ?cli)
   (expect-eq "readline after tokenised reads" "a whole line" (readline ?aname))

   ;; Writing by name reaches the peer.
   (printout ?aname "back to the client" crlf)
   (flush-connection ?acc)
   (expect-eq "the client reads it" "back to the client" (readline ?conn))

   ;; Closing by logical name, rather than by descriptor.
   (expect-true "close the accepted socket by name" (close-connection ?aname))
   (expect-false "and it is gone afterwards" (close-connection ?aname))
   (expect-true "close the client by name" (close-connection ?conn))
   (expect-true "close the listening socket by name"
                (close-connection (get-socket-logical-name ?srv))))

(run-tests)
(test-summary)
