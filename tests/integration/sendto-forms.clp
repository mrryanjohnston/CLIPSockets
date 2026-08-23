;;; Every argument form (sendto) accepts.
;;;
;;;   (sendto <socket> AF_UNIX  <path>          <data> [flags])
;;;   (sendto <socket> AF_INET  <ip> <port-int> <data> [flags])
;;;   (sendto <socket> AF_INET6 <ip> <port-int> <data> [flags])
;;;
;;; The AF_UNIX form reaches argument 4 with a lexeme, and the flags argument
;;; is argument 6. The registration must accept both.

(load* "tests/lib/expect.clp")
(test-suite "sendto-forms")
(test-plan 23)

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

;;; The family argument gives the arguments after it, and the caller must
;;; supply it. But the family must also agree with the socket. The kernel
;;; refuses a sockaddr of one family on a socket of another family, and it
;;; gives EINVAL. That message does not say which of the two values is
;;; incorrect. A refusal here can name both of them.
(deffunction run-family-tests ()
   (bind ?fd (create-socket AF_INET SOCK_DGRAM))
   (expect-false "an unsupported family is rejected"
                 (sendto ?fd AF_PACKET "127.0.0.1" 18999 "hi"))
   (expect-false "an AF_INET6 destination on an AF_INET socket is rejected"
                 (sendto ?fd AF_INET6 "::1" 18999 "hi"))
   (expect-false "an AF_UNIX destination on an AF_INET socket is rejected"
                 (sendto ?fd AF_UNIX "/tmp/clipsockets-no-such.sock" "hi"))
   (close-connection ?fd)

   (bind ?fd6 (create-socket AF_INET6 SOCK_DGRAM))
   (expect-false "an AF_INET destination on an AF_INET6 socket is rejected"
                 (sendto ?fd6 AF_INET "127.0.0.1" 18999 "hi"))
   (close-connection ?fd6))

;;; A unix path that is longer than sun_path. A shorter form of that path names
;;; a different socket, or no socket, and the call would report that it sent the
;;; datagram. No later check can find that error. sun_path is 108 bytes on
;;; Linux, and 200 characters is above the limit on each platform that
;;; builds this library.
(deffunction run-long-path-tests ()
   (bind ?long "/tmp/")
   (while (< (str-length ?long) 200) do
      (bind ?long (str-cat ?long "0123456789")))

   (bind ?fd (create-socket AF_UNIX SOCK_DGRAM))
   (expect-false "a unix path too long to hold is refused rather than truncated"
                 (sendto ?fd AF_UNIX ?long "hi"))
   (close-connection ?fd))

(run-inet-tests)
(run-inet6-tests)
(run-unix-tests)
(run-family-tests)
(run-long-path-tests)
(test-summary)
