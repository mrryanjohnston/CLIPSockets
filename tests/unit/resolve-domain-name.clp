;;; (resolve-domain-name)
;;;
;;; Only localhost is resolved here. Pointing the suite at a real domain would
;;; make it fail on an offline machine, which is a property of the network and
;;; not of this code.

(load* "tests/lib/expect.clp")
(test-suite "resolve-domain-name")
(test-plan 5)

(deffunction run-tests ()
   (bind ?addrs (resolve-domain-name localhost))
   (expect-true  "localhost resolves" ?addrs)
   (expect-gte   "at least one address comes back" 1 (length$ ?addrs))
   (expect-contains "the loopback address is among them" 127.0.0.1 ?addrs)

   ;; Duplicates are expected -- one entry per socket type -- which is why the
   ;; README suggests intersection$ on the result.
   (expect-gte "duplicates are not filtered out" 1
               (length$ (resolve-domain-name localhost)))

   ;; .invalid is reserved by RFC 2606 precisely so that it never resolves, so
   ;; this fails the same way with or without a working network.
   (expect-false "a name that cannot resolve returns FALSE"
                 (resolve-domain-name no-such-host.invalid)))

(run-tests)
(test-summary)
