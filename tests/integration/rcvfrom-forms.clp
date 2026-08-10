;;; (rcvfrom) argument forms and the shape of what it returns for each
;;; address family.
;;;
;;;   AF_INET / AF_INET6 -> family address port bytes payload   (5 fields)
;;;   AF_UNIX            -> family path bytes payload           (4 fields)
;;;   anonymous peer     -> AF_UNSPEC bytes payload             (3 fields)

(load* "tests/lib/expect.clp")
(test-suite "rcvfrom-forms")
(test-plan 23)

(defglobal ?*port*  = 18924)
(defglobal ?*port6* = 18925)
(defglobal ?*path*        = "/tmp/clipsockets-test-rcvfrom.sock")
(defglobal ?*client-path* = "/tmp/clipsockets-test-rcvfrom-client.sock")

(deffunction run-flag-tests ()
   (bind ?srv (create-socket AF_INET SOCK_DGRAM))
   (setsockopt ?srv SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?srv 127.0.0.1 ?*port*)
   (bind ?cli (create-socket AF_INET SOCK_DGRAM))

   ;; MSG_PEEK leaves the datagram in the queue, so the same one can be read
   ;; twice -- which is what makes the flag observable.
   (sendto ?cli AF_INET "127.0.0.1" ?*port* "peekable")
   (poll ?srv 5000 POLLIN)
   (bind ?peeked (rcvfrom ?srv MSG_PEEK))
   (expect-eq "peek reads the payload" "peekable" (nth$ 5 ?peeked))
   (bind ?again (rcvfrom ?srv))
   (expect-eq "and leaves it to be read again" "peekable" (nth$ 5 ?again))

   ;; Flags as a multifield and as a raw integer.
   (sendto ?cli AF_INET "127.0.0.1" ?*port* "multi")
   (poll ?srv 5000 POLLIN)
   (expect-eq "flags as a multifield" "multi"
              (nth$ 5 (rcvfrom ?srv (create$ MSG_PEEK MSG_WAITALL))))
   (expect-eq "flags as an integer" "multi" (nth$ 5 (rcvfrom ?srv 0)))

   ;; The third argument caps how much is read.
   (sendto ?cli AF_INET "127.0.0.1" ?*port* "truncate me")
   (poll ?srv 5000 POLLIN)
   (bind ?short (rcvfrom ?srv 0 4))
   (expect-eq "a maximum length truncates the payload" "trun" (nth$ 5 ?short))
   (expect-eq "and is reflected in the byte count" 4 (nth$ 4 ?short))

   ;; A single flag symbol rather than a multifield. MSG_WAITALL is the last
   ;; branch of that chain, so reaching it exercises the ones before it too.
   (sendto ?cli AF_INET "127.0.0.1" ?*port* "waited for")
   (poll ?srv 5000 POLLIN)
   (expect-eq "flags as a single symbol" "waited for"
              (nth$ 5 (rcvfrom ?srv MSG_WAITALL)))

   ;; Nothing left to read on a non-blocking socket is a failure, not a hang.
   (fcntl-add-status-flags ?srv O_NONBLOCK)
   (expect-false "reading an empty non-blocking socket fails" (rcvfrom ?srv))
   (expect-eq "which the kernel reports as EAGAIN" EAGAIN (errno-sym))

   (close-connection ?cli)
   (close-connection ?srv))

(deffunction run-inet6-tests ()
   (bind ?srv (create-socket AF_INET6 SOCK_DGRAM))
   (setsockopt ?srv SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?srv ::1 ?*port6*)
   (bind ?cli (create-socket AF_INET6 SOCK_DGRAM))

   (sendto ?cli AF_INET6 "::1" ?*port6* "over v6")
   (poll ?srv 5000 POLLIN)
   (bind ?mf (rcvfrom ?srv))
   (expect-length "an IPv6 peer gives five fields" 5 ?mf)
   (expect-eq "family is AF_INET6" AF_INET6 (nth$ 1 ?mf))
   (expect-eq "address is the v6 loopback" ::1 (nth$ 2 ?mf))
   (expect-gte "port is the sender's" 1 (nth$ 3 ?mf))
   (expect-eq "payload survives" "over v6" (nth$ 5 ?mf))

   (close-connection ?cli)
   (close-connection ?srv))

(deffunction run-unix-anonymous-tests ()
   ;; A datagram socket that was never bound has no address of its own, so the
   ;; kernel reports no peer family at all and the address fields are omitted.
   (remove ?*path*)
   (bind ?srv (create-socket AF_UNIX SOCK_DGRAM))
   (bind-socket ?srv /tmp/clipsockets-test-rcvfrom.sock)
   (bind ?cli (create-socket AF_UNIX SOCK_DGRAM))

   (sendto ?cli AF_UNIX "/tmp/clipsockets-test-rcvfrom.sock" "over unix")
   (poll ?srv 5000 POLLIN)
   (bind ?mf (rcvfrom ?srv))
   (expect-length "an anonymous peer gives three fields" 3 ?mf)
   (expect-eq "family is reported as AF_UNSPEC" AF_UNSPEC (nth$ 1 ?mf))
   (expect-eq "byte count" 9 (nth$ 2 ?mf))
   (expect-eq "payload survives" "over unix" (nth$ 3 ?mf))

   (close-connection ?cli)
   (close-connection ?srv)
   (remove ?*path*))

(deffunction run-unix-named-tests ()
   ;; Bind the sender too, and it now has a path for the receiver to see.
   (remove ?*path*)
   (remove ?*client-path*)
   (bind ?srv (create-socket AF_UNIX SOCK_DGRAM))
   (bind-socket ?srv /tmp/clipsockets-test-rcvfrom.sock)
   (bind ?cli (create-socket AF_UNIX SOCK_DGRAM))
   (bind-socket ?cli /tmp/clipsockets-test-rcvfrom-client.sock)

   (sendto ?cli AF_UNIX "/tmp/clipsockets-test-rcvfrom.sock" "from a named socket")
   (poll ?srv 5000 POLLIN)
   (bind ?mf (rcvfrom ?srv))
   (expect-length "a named UNIX peer gives four fields" 4 ?mf)
   (expect-eq "family is AF_UNIX" AF_UNIX (nth$ 1 ?mf))
   (expect-eq "the sender's path comes back"
              /tmp/clipsockets-test-rcvfrom-client.sock (nth$ 2 ?mf))
   (expect-eq "byte count" 19 (nth$ 3 ?mf))
   (expect-eq "payload survives" "from a named socket" (nth$ 4 ?mf))

   (close-connection ?cli)
   (close-connection ?srv)
   (remove ?*path*)
   (remove ?*client-path*))

(run-flag-tests)
(run-inet6-tests)
(run-unix-anonymous-tests)
(run-unix-named-tests)
(test-summary)
