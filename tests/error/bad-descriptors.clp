;;; Descriptors and logical names that do not correspond to a live socket.
;;;
;;; Every check here guards a path that must report and return FALSE rather
;;; than dereference a lookup that found nothing.
;;;
;;; A crash here shows up as "did not reach (test-summary)" rather than as a
;;; failed assertion, because the process dies before it can report.

(load* "tests/lib/expect.clp")
(test-suite "bad-descriptors")
(test-plan 18)

(deffunction run-tests ()
   ;; Unknown descriptors: report and return FALSE, never crash.
   (expect-false "get-socket-logical-name on an unknown descriptor"
                 (get-socket-logical-name 9999))
   (expect-false "bind-socket on an unknown descriptor"
                 (bind-socket 9999 127.0.0.1 19999))
   (expect-false "close-connection on an unknown descriptor"
                 (close-connection 9999))
   (expect-false "flush-connection on an unknown descriptor"
                 (flush-connection 9999))
   (expect-false "get-timeout on an unknown descriptor"
                 (get-timeout 9999))
   (expect-false "close-connection on an unknown logical name"
                 (close-connection no-such-socket))

   ;; shutdown() reports success as 0, and 0 handed straight to CreateBoolean
   ;; reads as FALSE. The answer here must come from the lookup and not from
   ;; that value.
   (expect-false "shutdown-connection on an unknown descriptor"
                 (shutdown-connection 9999))

   ;; The fcntl functions give the descriptor to the system call and do not
   ;; look it up in the socket list. As a result, a number that is not a
   ;; descriptor reaches fcntl(2), and the call must give FALSE and must not
   ;; give a success with no message.
   (expect-false "fcntl-add-status-flags on an unknown descriptor"
                 (fcntl-add-status-flags 9999 O_NONBLOCK))
   (expect-false "fcntl-remove-status-flags on an unknown descriptor"
                 (fcntl-remove-status-flags 9999 O_NONBLOCK)))

(deffunction run-unnamed-socket-tests ()
   ;; A socket that exists but has never been bound, connected or accepted.
   (bind ?fd (create-socket AF_INET SOCK_STREAM))
   (expect-gte "socket created" 0 ?fd)

   ;; It has no logical name yet, and asking must not hand back garbage.
   (expect-false "an unbound socket has no logical name"
                 (get-socket-logical-name ?fd))

   ;; Every router query walks the socket list, so plain output to t is enough
   ;; to trip the name comparison against an unnamed socket.
   (printout t "")
   (test-pass)

   ;; Closing an unnamed socket must not free a name it never allocated.
   (expect-true "an unbound socket closes cleanly" (close-connection ?fd)))

(deffunction run-list-removal-tests ()
   ;; The code puts a new socket at the front of the router list. As a
   ;; result, the first socket is never at the head of the list. A close of
   ;; that socket removes an entry from the middle, and that is a different
   ;; path from a close of the head.
   (bind ?first (create-socket AF_INET SOCK_STREAM))
   (bind ?second (create-socket AF_INET SOCK_STREAM))
   (expect-true "a socket behind the head of the list closes"
                (close-connection ?first))
   (expect-true "and so does the one that was at the head"
                (close-connection ?second))

   ;; A close by logical name reads the same list with a different key. As a
   ;; result, it has an unlink step of its own, and that step must be
   ;; correct.
   (bind ?named (create-socket AF_INET SOCK_STREAM))
   (setsockopt ?named SOL_SOCKET SO_REUSEADDR 1)
   (bind ?nname (bind-socket ?named 127.0.0.1 18932))
   (bind ?head (create-socket AF_INET SOCK_STREAM))
   (expect-true "a named socket behind the head closes by name"
                (close-connection ?nname))
   (close-connection ?head))

(deffunction run-leaked-socket-test ()
   ;; Deliberately left open: CloseAllSockets runs at exit and must cope with
   ;; a socket that has no logical name.
   (bind ?fd (create-socket AF_INET SOCK_STREAM))
   (expect-gte "socket created and left open for exit cleanup" 0 ?fd)

   ;; The same test with a socket that has a name. The code must release the
   ;; memory of that name.
   (bind ?named (create-socket AF_INET SOCK_STREAM))
   (setsockopt ?named SOL_SOCKET SO_REUSEADDR 1)
   (expect-true "a named socket left open for exit cleanup"
                (bind-socket ?named 127.0.0.1 18931)))

(run-tests)
(run-unnamed-socket-tests)
(run-list-removal-tests)
(run-leaked-socket-test)
(test-summary)
