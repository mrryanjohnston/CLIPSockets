;;; (errno) and (errno-sym)
;;;
;;; Test bodies live inside a deffunction because top-level (bind ?x ...) does
;;; not survive from one batch command to the next -- the same reason the
;;; examples do their work in defrule right-hand sides.

(load* "tests/lib/expect.clp")
(test-suite "errno")
(test-plan 4)

(deffunction run-tests ()
   ;; A failed syscall must leave both a numeric and a symbolic errno behind.
   (expect-false "scandir of a missing directory returns FALSE"
                 (scandir "/no/such/directory/anywhere"))
   (expect-eq    "errno is ENOENT after the failure" ENOENT (errno-sym))
   (expect-gte   "errno is a positive integer" 1 (errno))

   ;; errno-sym must agree with the raw number it is translating.
   (expect-eq "ENOENT is errno 2" 2 (errno)))

(run-tests)
(test-summary)
