;;; Descriptors and logical names that do not correspond to a live socket.
;;;
;;; Every check here is a regression test for a crash. Before these fixes
;;; each case either segfaulted or returned uninitialized memory:
;;;
;;;   get-socket-logical-name  dereferenced a NULL router lookup
;;;   bind-socket              did the same
;;;   create-socket            never initialised logicalName, so the close
;;;                            paths and the router-name lookup ran strlen()
;;;                            and strcmp() over a garbage pointer
;;;
;;; A crash here shows up as "did not reach (test-summary)" rather than as a
;;; failed assertion, because the process dies before it can report.

(load* "tests/lib/expect.clp")
(test-suite "bad-descriptors")
(test-plan 12)

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

   ;; shutdown() reports success as 0, which read as FALSE when handed
   ;; straight to CreateBoolean -- so this used to answer TRUE for a
   ;; descriptor that was never a socket.
   (expect-false "shutdown-connection on an unknown descriptor"
                 (shutdown-connection 9999)))

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

(deffunction run-leaked-socket-test ()
   ;; Deliberately left open: CloseAllSockets runs at exit and must cope with
   ;; a socket that has no logical name.
   (bind ?fd (create-socket AF_INET SOCK_STREAM))
   (expect-gte "socket created and left open for exit cleanup" 0 ?fd))

(run-tests)
(run-unnamed-socket-tests)
(run-leaked-socket-test)
(test-summary)
