;;; What bind-socket and connect must refuse.
;;;
;;; The two functions make a sockaddr from the same two arguments, an address
;;; and a port. Neither function can select a value for either argument.
;;;
;;; The code refuses an address unless the address is exactly correct. The old
;;; inet_addr(3) is much less strict. It reads "127.1" as 127.0.0.1, it accepts
;;; hexadecimal and octal in each octet, and it reports an incorrect address
;;; with the same value as the correct broadcast address 255.255.255.255. As a
;;; result, a caller cannot tell the two apart. A program that binds a listen
;;; socket to an incorrect address and gets a success listens at a location
;;; that it did not select. inet_pton(3) is strict on all three points, and the
;;; AF_INET6 code already used it.
;;;
;;; tests/error/missing-port.clp covers an absent port. That condition stops
;;; the process and does not fail, and it needs a file of its own.
;;;
;;; Each attempt below uses a socket of its own. The code cannot bind a socket
;;; that it already bound, because the kernel gives EINVAL. As a result, one
;;; socket for each check would make each check after the first pass for a
;;; cause that has nothing to do with its address.

(load* "tests/lib/expect.clp")
(test-suite "address-parsing")
(test-plan 13)

(defglobal ?*port*  = 19431)
(defglobal ?*port6* = 19432)

;;; One bind attempt on a socket of its own. The function closes that socket
;;; in both conditions.
(deffunction try-bind (?domain ?address ?port)
   (bind ?s (create-socket ?domain SOCK_STREAM))
   (setsockopt ?s SOL_SOCKET SO_REUSEADDR 1)
   (bind ?result (bind-socket ?s ?address ?port))
   (close-connection ?s)
   (return ?result))

;;; Incorrect IPv4 addresses and short forms of IPv4 addresses.
(deffunction run-address-tests ()
   (expect-false "bind-socket refuses dotted shorthand"
                 (try-bind AF_INET "127.1" ?*port*))
   (expect-false "bind-socket refuses a hexadecimal octet"
                 (try-bind AF_INET "0x7f.0.0.1" ?*port*))
   (expect-false "bind-socket refuses octets out of range"
                 (try-bind AF_INET "999.999.999.999" ?*port*))
   (expect-false "bind-socket refuses five octets"
                 (try-bind AF_INET "127.0.0.0.1" ?*port*))
   (expect-false "bind-socket refuses a name where an address belongs"
                 (try-bind AF_INET "not-an-address" ?*port*))
   (expect-false "bind-socket refuses an empty address"
                 (try-bind AF_INET "" ?*port*))

   ;; inet_addr could not tell the broadcast address from a failure. It is a
   ;; real address, and the code must still parse it. As a result, the call
   ;; gives a name, although this host has no interface to bind it to.
   (expect-neq "bind-socket still parses 255.255.255.255"
               FALSE (try-bind AF_INET "255.255.255.255" ?*port*))

   ;; An address that the code refuses must leave the socket free and not
   ;; partly bound. A later attempt with a correct address then operates.
   (bind ?t (create-socket AF_INET SOCK_STREAM))
   (setsockopt ?t SOL_SOCKET SO_REUSEADDR 1)
   (expect-false "a refused address does not bind" (bind-socket ?t "127.1" ?*port*))
   (expect-eq   "and the socket is still usable afterwards"
                (sym-cat "127.0.0.1:" ?*port*) (bind-socket ?t "127.0.0.1" ?*port*))
   (close-connection ?t)

   ;; connect gets the same test. A server listens on the address that the
   ;; short form gives. Without that server the call fails in both conditions,
   ;; and the check would pass with the short form accepted or refused.
   (bind ?srv (create-socket AF_INET SOCK_STREAM))
   (setsockopt ?srv SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?srv "127.0.0.1" ?*port*)
   (listen ?srv)

   (bind ?c (create-socket AF_INET SOCK_STREAM))
   (expect-false "connect refuses dotted shorthand too"
                 (connect ?c "127.1" ?*port*))
   (close-connection ?c)
   (close-connection ?srv))

;;; IPv6 already uses inet_pton. As a result, these checks are protection and
;;; not a correction. Code that replaces the two copies must keep the strict
;;; rules of the AF_INET6 path. An address that the code does not parse is the
;;; dangerous condition here. The code sets the sockaddr to zero first. As a
;;; result, an empty sockaddr holds the wildcard address, and a listen socket
;;; for one interface then answers on each interface.
(deffunction run-inet6-tests ()
   (expect-false "bind-socket refuses a malformed IPv6 address"
                 (try-bind AF_INET6 "::gg" ?*port6*))
   (expect-false "bind-socket refuses an IPv4 address on an IPv6 socket"
                 (try-bind AF_INET6 "127.0.0.1" ?*port6*))
   (expect-eq    "and still accepts a good one"
                 (sym-cat "[::1]:" ?*port6*) (try-bind AF_INET6 "::1" ?*port6*)))

(run-address-tests)
(run-inet6-tests)
(test-summary)
