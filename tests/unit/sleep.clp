;;; (sleep)
;;;
;;; The UDF is registered as "ld" so both integers and floats are accepted.

(load* "tests/lib/expect.clp")
(test-suite "sleep")
(test-plan 5)

(deffunction run-tests ()
   (expect-eq "an integer argument is accepted" 0 (sleep 0))
   (expect-eq "a float argument is accepted" 0 (sleep 0.01))

   ;; Fractional seconds convert to nanoseconds, not microseconds: (sleep 0.25)
   ;; must take about a quarter second rather than a quarter millisecond.
   (bind ?start (time))
   (sleep 0.25)
   (bind ?elapsed (- (time) ?start))
   (expect-gte "sleep 0.25 waits at least a quarter second" 0.2 ?elapsed)
   (expect-lte "sleep 0.25 does not overshoot wildly" 2.0 ?elapsed)

   (expect-false "a negative duration is rejected" (sleep -1)))

(run-tests)
(test-summary)
