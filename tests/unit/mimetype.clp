;;; (mimetype) -- requires: libmagic
;;;
;;; This UDF only exists in a MAGIC=1 build. run.sh reads the marker above and
;;; skips the file when the binary was linked without libmagic.

(load* "tests/lib/expect.clp")
(test-suite "mimetype")
(test-plan 4)

(deffunction run-tests ()
   (expect-eq "plain text is detected" text/plain (mimetype "README.md"))
   ;; A shell script is named by its "#!" line, which is a marker libmagic
   ;; reads out of the file itself. The makefile was here before and is not
   ;; a fair question: libmagic guesses that one from the shape of the text,
   ;; so editing the makefile changed the answer and failed this test.
   (expect-eq "a shell script is detected"
              text/x-shellscript (mimetype "scripts/fetch-clips.sh"))
   (expect-eq "a C source file is detected" text/x-c (mimetype "socketrtr.c"))

   ;; libmagic reports its own placeholder rather than failing outright.
   (expect-neq "a missing file does not report a real type"
               text/plain (mimetype "/no/such/file")))

(run-tests)
(test-summary)
