;;; (errno-sym) translation for the failures this library can actually cause.
;;;
;;; The underlying switch covers every errno the platform defines, so most of
;;; it is unreachable from CLIPS by construction. These are the ones a socket
;;; program realistically hits, each triggered through a real failed syscall
;;; rather than by setting errno directly.
;;;
;;; (errno-sym) reads the current errno, so each check has to follow its
;;; failing call immediately.

(load* "tests/lib/expect.clp")
(test-suite "errno-symbols")
(test-plan 14)

(deffunction run-tests ()
   (expect-false "scandir of a missing path fails" (scandir "/no/such/dir"))
   (expect-eq    "missing path is ENOENT" ENOENT (errno-sym))

   (expect-false "scandir of a regular file fails" (scandir "makefile"))
   (expect-eq    "regular file is ENOTDIR" ENOTDIR (errno-sym))

   ;; Nothing is listening on this port.
   (bind ?a (create-socket AF_INET SOCK_STREAM))
   (expect-false "connect to a closed port fails" (connect ?a 127.0.0.1 18999))
   (expect-eq    "closed port is ECONNREFUSED" ECONNREFUSED (errno-sym))

   ;; An address that is not on this machine.
   (bind ?b (create-socket AF_INET SOCK_STREAM))
   (expect-false "bind to a foreign address fails" (bind-socket ?b 1.2.3.4 18905))
   (expect-eq    "foreign address is EADDRNOTAVAIL" EADDRNOTAVAIL (errno-sym))

   ;; Privileged port, and this suite does not run as root.
   (bind ?c (create-socket AF_INET SOCK_STREAM))
   (expect-false "bind to a privileged port fails" (bind-socket ?c 127.0.0.1 1))
   (expect-eq    "privileged port is EACCES" EACCES (errno-sym))

   ;; Second bind to the same port, without SO_REUSEADDR on either socket.
   (bind ?d (create-socket AF_INET SOCK_STREAM))
   (bind-socket ?d 127.0.0.1 18906)
   (bind ?e (create-socket AF_INET SOCK_STREAM))
   (expect-false "double bind fails" (bind-socket ?e 127.0.0.1 18906))
   (expect-eq    "double bind is EADDRINUSE" EADDRINUSE (errno-sym))

   (expect-false "getsockopt on a non-socket fails"
                 (getsockopt 9999 SOL_SOCKET SO_REUSEADDR))
   (expect-eq    "non-socket is EBADF" EBADF (errno-sym))

   (close-connection ?a)
   (close-connection ?b)
   (close-connection ?c)
   (close-connection ?d)
   (close-connection ?e))

(run-tests)
(test-summary)
