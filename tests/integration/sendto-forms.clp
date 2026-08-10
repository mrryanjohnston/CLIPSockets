;;; Every argument form (sendto) accepts.
;;;
;;;   (sendto <socket> AF_UNIX  <path>          <data> [flags])
;;;   (sendto <socket> AF_INET  <ip> <port-int> <data> [flags])
;;;   (sendto <socket> AF_INET6 <ip> <port-int> <data> [flags])
;;;
;;; The AF_UNIX form and the flags argument were both unreachable until the
;;; registration was widened: argument 4 only accepted integers, and flags
;;; would have been argument 6 against a declared maximum of 5.

(load* "tests/lib/expect.clp")
(test-suite "sendto-forms")
(test-plan 19)

(defglobal ?*port*  = 18922)
(defglobal ?*port6* = 18923)
(defglobal ?*path*  = "/tmp/clipsockets-test-sendto.sock")

(deffunction run-inet-tests ()
   (bind ?srv (create-socket AF_INET SOCK_DGRAM))
   (setsockopt ?srv SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?srv 127.0.0.1 ?*port*)
   (bind ?cli (create-socket AF_INET SOCK_DGRAM))

   (expect-eq "plain AF_INET send" 5 (sendto ?cli AF_INET "127.0.0.1" ?*port* "abcde"))

   ;; Flags may be given as a single symbol, as a multifield of symbols, or as
   ;; a raw integer.
   (expect-eq "flags as one symbol" 2
              (sendto ?cli AF_INET "127.0.0.1" ?*port* "hi" MSG_DONTWAIT))
   (expect-eq "flags as a multifield" 2
              (sendto ?cli AF_INET "127.0.0.1" ?*port* "hi"
                      (create$ MSG_DONTWAIT MSG_NOSIGNAL)))
   (expect-eq "flags as an integer" 2
              (sendto ?cli AF_INET "127.0.0.1" ?*port* "hi" 0))
   (expect-eq "an unknown flag symbol is ignored" 2
              (sendto ?cli AF_INET "127.0.0.1" ?*port* "hi" NOT_A_FLAG))
   (expect-eq "the rest of the recognised flags" 2
              (sendto ?cli AF_INET "127.0.0.1" ?*port* "hi"
                      (create$ MSG_CONFIRM MSG_DONTROUTE MSG_EOR MSG_MORE)))

   ;; MSG_OOB is parsed like the others, but a datagram socket has no notion of
   ;; out-of-band data, so the send itself is refused.
   (expect-false "MSG_OOB on a datagram socket is refused"
                 (sendto ?cli AF_INET "127.0.0.1" ?*port* "hi"
                         (create$ MSG_DONTWAIT MSG_OOB)))

   (expect-false "a malformed IPv4 address is rejected"
                 (sendto ?cli AF_INET "not.an.ip" ?*port* "hi"))
   (expect-false "AF_INET without a payload" (sendto ?cli AF_INET "127.0.0.1" ?*port*))
   (expect-false "AF_INET without a port" (sendto ?cli AF_INET "127.0.0.1"))

   (close-connection ?cli)
   (close-connection ?srv))

(deffunction run-inet6-tests ()
   (bind ?srv (create-socket AF_INET6 SOCK_DGRAM))
   (setsockopt ?srv SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?srv ::1 ?*port6*)
   (bind ?cli (create-socket AF_INET6 SOCK_DGRAM))

   (expect-eq "plain AF_INET6 send" 3 (sendto ?cli AF_INET6 "::1" ?*port6* "abc"))
   (expect-eq "AF_INET6 with flags" 3
              (sendto ?cli AF_INET6 "::1" ?*port6* "abc" MSG_DONTWAIT))
   (expect-false "a malformed IPv6 address is rejected"
                 (sendto ?cli AF_INET6 "not::an::ip" ?*port6* "hi"))
   (expect-false "AF_INET6 without a payload" (sendto ?cli AF_INET6 "::1" ?*port6*))

   (close-connection ?cli)
   (close-connection ?srv))

(deffunction run-unix-tests ()
   (remove ?*path*)
   (bind ?srv (create-socket AF_UNIX SOCK_DGRAM))
   (bind-socket ?srv /tmp/clipsockets-test-sendto.sock)
   (bind ?cli (create-socket AF_UNIX SOCK_DGRAM))

   (expect-eq "AF_UNIX send to a path" 5
              (sendto ?cli AF_UNIX "/tmp/clipsockets-test-sendto.sock" "abcde"))
   (expect-eq "AF_UNIX with flags" 2
              (sendto ?cli AF_UNIX "/tmp/clipsockets-test-sendto.sock" "hi" MSG_DONTWAIT))
   (expect-false "sending to a path with no socket fails"
                 (sendto ?cli AF_UNIX "/tmp/clipsockets-no-such.sock" "hi"))
   (expect-false "AF_UNIX without a payload"
                 (sendto ?cli AF_UNIX "/tmp/clipsockets-test-sendto.sock"))

   (close-connection ?cli)
   (close-connection ?srv)
   (remove ?*path*))

(deffunction run-family-tests ()
   (bind ?fd (create-socket AF_INET SOCK_DGRAM))
   (expect-false "an unsupported family is rejected"
                 (sendto ?fd AF_PACKET "127.0.0.1" 18999 "hi"))
   (close-connection ?fd))

(run-inet-tests)
(run-inet6-tests)
(run-unix-tests)
(run-family-tests)
(test-summary)
