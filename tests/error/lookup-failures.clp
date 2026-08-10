;;; Every UDF that resolves a socket must fail cleanly when handed a logical
;;; name that no socket carries.
;;;
;;; A descriptor that is merely wrong (9999) and a name that is merely unknown
;;; take different routes through the lookup helpers, so both are worth having:
;;; tests/error/bad-descriptors.clp covers the integer form.

(load* "tests/lib/expect.clp")
(test-suite "lookup-failures")
(test-plan 16)

(deffunction run-tests ()
   (expect-false "get-timeout" (get-timeout no-such-name))
   (expect-false "set-timeout" (set-timeout no-such-name 5))
   (expect-false "poll" (poll no-such-name 10))
   (expect-false "listen" (listen no-such-name))
   (expect-false "accept" (accept no-such-name))
   (expect-false "shutdown-connection" (shutdown-connection no-such-name))
   (expect-false "getsockopt" (getsockopt no-such-name SOL_SOCKET SO_REUSEADDR))
   (expect-false "setsockopt" (setsockopt no-such-name SOL_SOCKET SO_REUSEADDR 1))
   (expect-false "fcntl-add-status-flags" (fcntl-add-status-flags no-such-name O_NONBLOCK))
   (expect-false "fcntl-remove-status-flags" (fcntl-remove-status-flags no-such-name O_NONBLOCK))
   (expect-false "flush-connection" (flush-connection no-such-name))
   (expect-false "empty-connection" (empty-connection no-such-name))
   (expect-false "set-not-buffered" (set-not-buffered no-such-name))
   (expect-false "rcvfrom" (rcvfrom no-such-name))
   (expect-false "sendto" (sendto no-such-name AF_INET "127.0.0.1" 18999 "x"))

   ;; connect takes only a descriptor, so its lookup failure needs the integer.
   (expect-false "connect" (connect 9999 127.0.0.1 18999)))

(run-tests)
(test-summary)
