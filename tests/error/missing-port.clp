;;; bind-socket and connect with no port argument.
;;;
;;; The two functions take the port as an optional third argument. It is
;;; optional because AF_UNIX has no port. An AF_UNIX address is a file path and
;;; nothing more. It is not optional because AF_INET and AF_INET6 have a
;;; default. For those two families there is no value to use, and the code
;;; refuses the call.
;;;
;;; The code must not read the argument. No code wrote to an argument that the
;;; caller did not give, and the memory there holds the last value of the
;;; stack. As a port that value is a number that no one selected. As the
;;; pointer that it is, it ends the process.
;;;
;;; This is a file of its own because of that failure. A stop of the process
;;; also loses the output in the buffer. As a result, the runner would report
;;; each check in the same file as a check that never ran, and not as a check
;;; that failed.

(load* "tests/lib/expect.clp")
(test-suite "missing-port")
(test-plan 5)

(defglobal ?*unix* = "/tmp/clipsockets-test-missing-port.sock")

(deffunction run-tests ()
   (bind ?s (create-socket AF_INET SOCK_STREAM))
   (setsockopt ?s SOL_SOCKET SO_REUSEADDR 1)
   (expect-false "bind-socket refuses an AF_INET address with no port"
                 (bind-socket ?s "127.0.0.1"))
   (close-connection ?s)

   (bind ?c (create-socket AF_INET SOCK_STREAM))
   (expect-false "connect refuses an AF_INET address with no port"
                 (connect ?c "127.0.0.1"))
   (close-connection ?c)

   (bind ?s6 (create-socket AF_INET6 SOCK_STREAM))
   (setsockopt ?s6 SOL_SOCKET SO_REUSEADDR 1)
   (expect-false "bind-socket refuses an AF_INET6 address with no port"
                 (bind-socket ?s6 "::1"))
   (close-connection ?s6)

   ;; AF_UNIX is the cause of the optional argument. As a result, a call with
   ;; no port on an AF_UNIX socket must continue to operate.
   (remove ?*unix*)
   (bind ?u (create-socket AF_UNIX SOCK_STREAM))
   (expect-eq   "AF_UNIX still binds with no port at all"
                (sym-cat ?*unix*) (bind-socket ?u ?*unix*))
   (expect-true "the AF_UNIX socket really is listening" (listen ?u))
   (close-connection ?u)
   (remove ?*unix*))

(run-tests)
(test-summary)
