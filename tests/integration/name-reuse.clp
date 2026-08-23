;;; A logical name is correct only while its socket is open.
;;;
;;; concurrent-connections.clp shows that two sockets that are open at the same
;;; time never share a name. That is a statement about one moment. This file is
;;; about a period of time, and the answer there is different. The code uses a
;;; name again, and it gives the same name with no message.
;;;
;;; The name of a connection is "<peer>#<descriptor>". The peer comes from the
;;; address of the connect call, and the descriptor comes from the kernel. The
;;; kernel gives the lowest free descriptor. As a result, a close of one
;;; connection and a connect to the same address give the same descriptor and
;;; the same name. The name is not similar. It is the same string.
;;;
;;; This is important because programs keep names. rule-driven-server.clp puts
;;; a name in a fact slot and uses it several rule firings later, and this
;;; library is for that pattern. A fact with the name of a connection that
;;; closed does not fail when the code uses the name again. The name addresses
;;; the socket that now has that descriptor, and the write goes to a different
;;; peer.
;;;
;;; As a result, the test must check the two halves together. The code frees
;;; the name when the socket closes, and the name comes back on a different
;;; socket. A test of one half alone looks correct and is not.

(load* "tests/lib/expect.clp")
(test-suite "name-reuse")
(test-plan 18)

(defglobal ?*port-a* = 19371)
(defglobal ?*port-b* = 19372)

;;; A listen socket that stays open for the full file.
(deffunction listener (?port)
   (bind ?fd (create-socket AF_INET SOCK_STREAM))
   (setsockopt ?fd SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?fd 127.0.0.1 ?port)
   (listen ?fd)
   (return ?fd))

(deffunction run-tests ()
   (bind ?srv-a (listener ?*port-a*))
   (bind ?srv-b (listener ?*port-b*))

   ;;=================================================================
   ;; What a name is made of
   ;;=================================================================
   (bind ?c1 (create-socket AF_INET SOCK_STREAM))
   (bind ?n1 (connect ?c1 127.0.0.1 ?*port-a*))
   (bind ?a1 (accept ?srv-a))

   (expect-eq "the connection is named for the peer it reached"
              1 (str-index (sym-cat "127.0.0.1:" ?*port-a* "#") ?n1))
   (expect-eq "and the name resolves back to its own socket"
              ?n1 (get-socket-logical-name ?c1))

   ;; The name of the accepted end also comes from its peer, which here is
   ;; the client above. As a result, the two ends of one connection have
   ;; different names, although both of them are in this process.
   (bind ?an1 (get-socket-logical-name ?a1))
   (expect-eq "the accepted end is named for the client that reached it"
              1 (str-index "127.0.0.1:" ?an1))
   (expect-neq "and is not named the same as the client" ?n1 ?an1)

   ;;=================================================================
   ;; Closing gives the name up
   ;;=================================================================
   (close-connection ?c1)
   (close-connection ?a1)

   (expect-false "a closed descriptor has no name"
                 (get-socket-logical-name ?c1))

   ;; The name is not only old. It is unknown. Each call that takes a name
   ;; reports that it cannot find the name, and no call operates on a
   ;; socket.
   (expect-false "the name resolves to nothing" (poll ?n1 0 POLLIN))
   (expect-false "closing by the same name again fails"
                 (close-connection ?n1))
   (expect-false "and so does setting a timeout on it" (set-timeout ?n1 1000))

   ;;=================================================================
   ;; And hands it to the next socket
   ;;=================================================================
   ;; No code opened a different descriptor between the two calls. As a
   ;; result, the same descriptor is free and the kernel gives it again. A
   ;; connect to the same address then makes the same name from the same two
   ;; parts.
   (bind ?c2 (create-socket AF_INET SOCK_STREAM))
   (bind ?n2 (connect ?c2 127.0.0.1 ?*port-a*))
   (bind ?a2 (accept ?srv-a))

   (expect-eq "the kernel reissues the descriptor" ?c1 ?c2)
   (expect-eq "so the new connection has the old connection's name" ?n1 ?n2)
   (expect-eq "and answers to it" ?n2 (get-socket-logical-name ?c2))

   ;; This is the result, as the failure that it would be in a program. The
   ;; code got ?n1 before the first connection closed and did not change it.
   ;; A write with ?n1 now goes to a different socket, and no code reports a
   ;; problem.
   (printout ?n1 "addressed to a connection that closed" crlf)
   (flush-connection ?c2)
   (expect-eq "a write to the stale name arrives at the new peer"
              "addressed to a connection that closed"
              (readline (get-socket-logical-name ?a2)))

   ;;=================================================================
   ;; The descriptor alone is not the name
   ;;=================================================================
   ;; The same sequence, with a different address. The kernel gives the same
   ;; descriptor again. If the descriptor alone made the name, the two names
   ;; would be equal here. The name also has the peer in it, and the two names
   ;; are different.
   (close-connection ?c2)
   (close-connection ?a2)

   (bind ?c3 (create-socket AF_INET SOCK_STREAM))
   (bind ?n3 (connect ?c3 127.0.0.1 ?*port-b*))
   (bind ?a3 (accept ?srv-b))

   (expect-eq "the descriptor is reused once more" ?c1 ?c3)
   (expect-neq "but a different peer makes a different name" ?n1 ?n3)

   ;;=================================================================
   ;; Reuse needs the socket to be gone first
   ;;=================================================================
   ;; Two connections to one address, and both are open. The descriptors are
   ;; different because the code freed neither of them, and the names are
   ;; also different. A close causes the second use of a name. The address
   ;; does not.
   (bind ?c4 (create-socket AF_INET SOCK_STREAM))
   (bind ?n4 (connect ?c4 127.0.0.1 ?*port-b*))
   (bind ?a4 (accept ?srv-b))

   (expect-neq "two connections open at once never share a descriptor"
               ?c3 ?c4)
   (expect-neq "nor a name" ?n3 ?n4)

   ;; Each name also reaches its own peer, with the two names in use at the
   ;; same time.
   (printout ?n3 "from the third" crlf)
   (printout ?n4 "from the fourth" crlf)
   (flush-connection ?c3)
   (flush-connection ?c4)
   (expect-eq "the third connection reaches its own peer"
              "from the third" (readline (get-socket-logical-name ?a3)))
   (expect-eq "the fourth connection reaches its own peer"
              "from the fourth" (readline (get-socket-logical-name ?a4)))

   (close-connection ?c3)
   (close-connection ?c4)
   (close-connection ?a3)
   (close-connection ?a4)
   (close-connection ?srv-a)
   (close-connection ?srv-b))

(run-tests)
(test-summary)
