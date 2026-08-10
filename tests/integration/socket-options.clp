;;; Per-socket options on a live connection: descriptor status flags,
;;; the three stdio buffering modes, and draining a connection.

(load* "tests/lib/expect.clp")
(test-suite "socket-options")
(test-plan 18)

(defglobal ?*port* = 18904)

(deffunction run-tests ()
   (bind ?srv (create-socket AF_INET SOCK_STREAM))
   (setsockopt ?srv SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?srv 127.0.0.1 ?*port*)
   (listen ?srv)
   (bind ?cli (create-socket AF_INET SOCK_STREAM))
   (bind ?conn (connect ?cli 127.0.0.1 ?*port*))
   (bind ?cfd (accept ?srv))
   (bind ?cname (get-socket-logical-name ?cfd))
   (expect-true "connection established" ?cname)

   ;; Descriptor status flags.
   (expect-true "O_NONBLOCK can be added" (fcntl-add-status-flags ?cfd O_NONBLOCK))
   (expect-true "O_NONBLOCK can be removed" (fcntl-remove-status-flags ?cfd O_NONBLOCK))
   (expect-true "O_APPEND can be added" (fcntl-add-status-flags ?cfd O_APPEND))
   (expect-true "O_APPEND can be removed" (fcntl-remove-status-flags ?cfd O_APPEND))
   (expect-true "several flags can be added at once"
                (fcntl-add-status-flags ?cfd O_NONBLOCK O_APPEND))
   (expect-true "several flags can be removed at once"
                (fcntl-remove-status-flags ?cfd O_NONBLOCK O_APPEND))
   (expect-true "O_ASYNC can be added" (fcntl-add-status-flags ?cfd O_ASYNC))
   (expect-true "O_ASYNC can be removed" (fcntl-remove-status-flags ?cfd O_ASYNC))
   (expect-false "an unknown flag is rejected by add"
                 (fcntl-add-status-flags ?cfd NOT_A_REAL_FLAG))
   (expect-false "an unknown flag is rejected by remove"
                 (fcntl-remove-status-flags ?cfd NOT_A_REAL_FLAG))

   ;; All three stdio buffering modes. These are registered as returning a
   ;; boolean; they used to fall off the end returning void, which made them
   ;; illegal as arguments to anything.
   (expect-true "line buffering" (set-line-buffered ?cfd))
   (expect-true "full buffering" (set-fully-buffered ?cfd))
   (expect-true "no buffering" (set-not-buffered ?cfd))
   (expect-false "buffering mode on an unknown descriptor"
                 (set-line-buffered 9999))

   ;; empty-connection drains whatever is waiting to be read. On a blocking
   ;; socket with nothing left it would wait forever, so O_NONBLOCK first --
   ;; the same order examples/server-simple.bat uses.
   (printout ?conn "data the server never reads" crlf)
   (flush-connection ?cli)
   (fcntl-add-status-flags ?cfd O_NONBLOCK)
   (expect-true "drain by logical name" (empty-connection ?cname))
   (expect-true "drain by descriptor" (empty-connection ?cfd))
   (expect-false "drain an unknown descriptor" (empty-connection 9999))

   (close-connection ?cfd)
   (close-connection ?cli)
   (close-connection ?srv))

(run-tests)
(test-summary)
