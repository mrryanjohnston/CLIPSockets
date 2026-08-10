;;; (mimetype) -- requires: libmagic
;;;
;;; This UDF only exists in a MAGIC=1 build. run.sh reads the marker above and
;;; skips the file when the binary was linked without libmagic.

(load* "tests/lib/expect.clp")
(test-suite "mimetype")
(test-plan 4)

(deffunction run-tests ()
   (expect-eq "plain text is detected" text/plain (mimetype "README.md"))
   (expect-eq "the makefile is detected" text/x-makefile (mimetype "makefile"))
   (expect-eq "a C source file is detected" text/x-c (mimetype "src/socketrtr.c"))

   ;; libmagic reports its own placeholder rather than failing outright.
   (expect-neq "a missing file does not report a real type"
               text/plain (mimetype "/no/such/file")))

(run-tests)
(test-summary)
