;;; An accepted connection must inherit the listening socket's domain.
;;;
;;; accept() built its router without setting domain or type, so those fields
;;; held whatever was left in the recycled struct. Every switch on sptr->domain
;;; -- in bind-socket, connect and accept itself -- then read uninitialised
;;; memory. It happened to read as 0 (AF_UNSPEC) often enough to look like
;;; deliberate "domain not supported" handling, but the value was undefined and
;;; could equally have selected a real case and misread the address that
;;; followed.
;;;
;;; These calls are all nonsense to make on an established connection; the point
;;; is that they now fail for the honest reason reported by the kernel rather
;;; than on the contents of freed memory.

(load* "tests/lib/expect.clp")
(test-suite "accepted-socket-domain")
(test-plan 7)

(defglobal ?*port* = 18951)

(deffunction run-tests ()
   (bind ?srv (create-socket AF_INET SOCK_STREAM))
   (setsockopt ?srv SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?srv 127.0.0.1 ?*port*)
   (listen ?srv)
   (bind ?cli (create-socket AF_INET SOCK_STREAM))
   (connect ?cli 127.0.0.1 ?*port*)
   (bind ?acc (accept ?srv))
   (expect-gte "a connection was accepted" 0 ?acc)

   ;; Binding an already-connected socket is refused by the kernel, which is
   ;; only reachable if the domain was inherited: an AF_UNSPEC domain would
   ;; have been rejected before any syscall happened.
   (expect-false "bind-socket on an accepted socket fails"
                 (bind-socket ?acc 127.0.0.1 (+ ?*port* 1)))
   (expect-eq "and reports EINVAL rather than an unsupported domain"
              EINVAL (errno-sym))

   ;; Likewise connect: the socket is already connected.
   (expect-false "connect on an accepted socket fails"
                 (connect ?acc 127.0.0.1 ?*port*))
   (expect-eq "reporting EISCONN" EISCONN (errno-sym))

   ;; And accept, which needs a listening socket.
   (expect-false "accept on an accepted socket fails" (accept ?acc))
   (expect-eq "reporting EINVAL" EINVAL (errno-sym))

   (close-connection ?acc)
   (close-connection ?cli)
   (close-connection ?srv))

(run-tests)
(test-summary)
