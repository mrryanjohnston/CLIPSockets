;;; Writing to a peer that has hung up.
;;;
;;; Socket writes go through the CLIPS router to fprintf, so a peer that has
;;; closed raises SIGPIPE, whose default action terminates the process --
;;; meaning any client could kill a CLIPSockets server by disconnecting
;;; mid-response. Ignoring the signal turns that into an ordinary EPIPE.
;;;
;;; Without the (signal SIGPIPE SIG_IGN) below this file is killed partway and
;;; run.sh reports it as "did not reach (test-summary)" rather than as a failed
;;; assertion, because the process dies before it can report anything.

(load* "tests/lib/expect.clp")
(test-suite "closed-peer")
(test-plan 3)

(defglobal ?*port* = 18961)

(deffunction run-tests ()
   (expect-true "SIGPIPE can be ignored" (signal SIGPIPE SIG_IGN))

   (bind ?srv (create-socket AF_INET SOCK_STREAM))
   (setsockopt ?srv SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?srv 127.0.0.1 ?*port*)
   (listen ?srv)
   (bind ?cli (create-socket AF_INET SOCK_STREAM))
   (connect ?cli 127.0.0.1 ?*port*)
   (bind ?acc (accept ?srv))
   (bind ?name (get-socket-logical-name ?acc))

   ;; The peer goes away without reading anything.
   (close-connection ?cli)

   ;; The first write usually lands in the socket buffer and only draws a reset
   ;; afterwards, so this has to write more than once to reach EPIPE.
   (loop-for-count 5 do
      (printout ?name "writing to a peer that is gone" crlf)
      (flush-connection ?acc))

   (expect-eq "the write fails with EPIPE" EPIPE (errno-sym))

   ;; Reaching here at all is the point of the test; opening another socket
   ;; shows the environment came through intact rather than merely surviving.
   (bind ?after (create-socket AF_INET SOCK_STREAM))
   (expect-gte "sockets still work afterwards" 0 ?after)

   (close-connection ?after)
   (close-connection ?acc)
   (close-connection ?srv))

(run-tests)
(test-summary)
