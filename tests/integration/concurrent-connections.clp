;;; Several simultaneous connections to one server.
;;;
;;; printout and readline address a socket only by its logical name, so a name
;;; has to identify exactly one socket. A connection is named
;;; "<peer>#<descriptor>": the peer says what it reached, and the descriptor,
;;; unique within the process, keeps connections to the same host:port apart.

(load* "tests/lib/expect.clp")
(test-suite "concurrent-connections")
(test-plan 29)

(defglobal ?*port*  = 18980)
(defglobal ?*port6* = 18981)
(defglobal ?*path*  = "/tmp/clipsockets-test-concurrent.sock")

(deffunction run-distinct-name-tests ()
   (bind ?srv (create-socket AF_INET SOCK_STREAM))
   (setsockopt ?srv SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?srv 127.0.0.1 ?*port*)
   (listen ?srv)

   (bind ?c1 (create-socket AF_INET SOCK_STREAM))
   (bind ?n1 (connect ?c1 127.0.0.1 ?*port*))
   (bind ?c2 (create-socket AF_INET SOCK_STREAM))
   (bind ?n2 (connect ?c2 127.0.0.1 ?*port*))
   (bind ?c3 (create-socket AF_INET SOCK_STREAM))
   (bind ?n3 (connect ?c3 127.0.0.1 ?*port*))

   ;; Three connections to one server: three different names.
   (expect-neq "connection 1 and 2 are named differently" ?n1 ?n2)
   (expect-neq "connection 2 and 3 are named differently" ?n2 ?n3)
   (expect-neq "connection 1 and 3 are named differently" ?n1 ?n3)

   ;; Each still says which peer it reached.
   (expect-eq "connection 1 names its peer" 1 (str-index (sym-cat "127.0.0.1:" ?*port* "#") ?n1))
   (expect-eq "connection 2 names its peer" 1 (str-index (sym-cat "127.0.0.1:" ?*port* "#") ?n2))
   (expect-eq "connection 3 names its peer" 1 (str-index (sym-cat "127.0.0.1:" ?*port* "#") ?n3))

   ;; And the name maps back to the same socket it came from.
   (expect-eq "connection 1 resolves to itself" ?n1 (get-socket-logical-name ?c1))
   (expect-eq "connection 2 resolves to itself" ?n2 (get-socket-logical-name ?c2))
   (expect-eq "connection 3 resolves to itself" ?n3 (get-socket-logical-name ?c3))

   ;; The server side sees three separate clients, each with its own name.
   (bind ?a1 (accept ?srv))
   (bind ?a2 (accept ?srv))
   (bind ?a3 (accept ?srv))
   (bind ?an1 (get-socket-logical-name ?a1))
   (bind ?an2 (get-socket-logical-name ?a2))
   (bind ?an3 (get-socket-logical-name ?a3))
   (expect-neq "accepted 1 and 2 are named differently" ?an1 ?an2)
   (expect-neq "accepted 2 and 3 are named differently" ?an2 ?an3)
   (expect-neq "accepted 1 and 3 are named differently" ?an1 ?an3)

   ;; An accepted socket is named for its peer, and its peer is a client whose
   ;; own name carries a descriptor, so the two ends never collide even though
   ;; both live in this one process.
   (expect-neq "client 1 and its accepted end differ" ?n1 ?an1)
   (expect-neq "client 2 and its accepted end differ" ?n2 ?an2)

   ;; Writes go where they are addressed. Each client sends something different
   ;; and each accepted socket must read exactly its own peer's message.
   (printout ?n1 "from connection one" crlf)
   (printout ?n2 "from connection two" crlf)
   (printout ?n3 "from connection three" crlf)
   (flush-connection ?c1)
   (flush-connection ?c2)
   (flush-connection ?c3)
   (expect-eq "accepted 1 reads connection 1's message" "from connection one"   (readline ?an1))
   (expect-eq "accepted 2 reads connection 2's message" "from connection two"   (readline ?an2))
   (expect-eq "accepted 3 reads connection 3's message" "from connection three" (readline ?an3))

   ;; And back the other way.
   (printout ?an1 "reply to one" crlf)
   (printout ?an2 "reply to two" crlf)
   (printout ?an3 "reply to three" crlf)
   (flush-connection ?a1)
   (flush-connection ?a2)
   (flush-connection ?a3)
   (expect-eq "connection 1 reads its own reply" "reply to one"   (readline ?n1))
   (expect-eq "connection 2 reads its own reply" "reply to two"   (readline ?n2))
   (expect-eq "connection 3 reads its own reply" "reply to three" (readline ?n3))

   ;; Closing one connection leaves the others addressing their own sockets.
   (close-connection ?c1)
   (close-connection ?a1)
   (expect-eq "connection 2 still resolves to itself" ?n2 (get-socket-logical-name ?c2))
   (expect-eq "connection 3 still resolves to itself" ?n3 (get-socket-logical-name ?c3))

   (printout ?n2 "still mine" crlf)
   (flush-connection ?c2)
   (expect-eq "traffic still reaches the right peer after a close"
              "still mine" (readline ?an2))

   (close-connection ?c2)
   (close-connection ?c3)
   (close-connection ?a2)
   (close-connection ?a3)
   (close-connection ?srv))

(deffunction run-ipv6-tests ()
   (bind ?srv (create-socket AF_INET6 SOCK_STREAM))
   (setsockopt ?srv SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?srv ::1 ?*port6*)
   (listen ?srv)
   (bind ?c1 (create-socket AF_INET6 SOCK_STREAM))
   (bind ?n1 (connect ?c1 ::1 ?*port6*))
   (bind ?c2 (create-socket AF_INET6 SOCK_STREAM))
   (bind ?n2 (connect ?c2 ::1 ?*port6*))

   (expect-neq "two IPv6 connections are named differently" ?n1 ?n2)
   (expect-eq "an IPv6 connection keeps the bracketed peer"
              1 (str-index (sym-cat "[::1]:" ?*port6* "#") ?n1))

   (bind ?a1 (accept ?srv))
   (bind ?a2 (accept ?srv))
   (printout ?n2 "v6 two" crlf)
   (flush-connection ?c2)
   (expect-eq "the second IPv6 connection reaches its own peer"
              "v6 two" (readline (get-socket-logical-name ?a2)))

   (close-connection ?c1) (close-connection ?c2)
   (close-connection ?a1) (close-connection ?a2)
   (close-connection ?srv))

(deffunction run-unix-tests ()
   (remove ?*path*)
   (bind ?srv (create-socket AF_UNIX SOCK_STREAM))
   (bind-socket ?srv /tmp/clipsockets-test-concurrent.sock)
   (listen ?srv)
   (bind ?c1 (create-socket AF_UNIX SOCK_STREAM))
   (bind ?n1 (connect ?c1 /tmp/clipsockets-test-concurrent.sock))
   (bind ?c2 (create-socket AF_UNIX SOCK_STREAM))
   (bind ?n2 (connect ?c2 /tmp/clipsockets-test-concurrent.sock))

   ;; A unix client binds no path of its own, so the descriptor is the only
   ;; thing separating two connections to the same path.
   (expect-neq "two connections to one path are named differently" ?n1 ?n2)
   (expect-eq "a unix connection keeps the path it reached"
              1 (str-index "/tmp/clipsockets-test-concurrent.sock#" ?n1))

   (bind ?a1 (accept ?srv))
   (bind ?a2 (accept ?srv))
   (printout ?n2 "unix two" crlf)
   (flush-connection ?c2)
   (expect-eq "the second unix connection reaches its own peer"
              "unix two" (readline (get-socket-logical-name ?a2)))

   (close-connection ?c1) (close-connection ?c2)
   (close-connection ?a1) (close-connection ?a2)
   (close-connection ?srv)
   (remove ?*path*))

(run-distinct-name-tests)
(run-ipv6-tests)
(run-unix-tests)
(test-summary)
