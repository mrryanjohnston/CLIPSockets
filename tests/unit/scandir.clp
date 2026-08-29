;;; (scandir)
;;;
;;; Tests run with the repository root as the working directory.

(load* "tests/lib/expect.clp")
(test-suite "scandir")
(test-plan 10)

(deffunction run-tests ()
   ;; Entries this repository is guaranteed to have.
   (expect-contains "lists makefile"  makefile  (scandir "."))
   (expect-contains "lists README.md" README.md (scandir "."))
   (expect-contains "lists examples"  examples  (scandir "."))
   (expect-contains "lists tests"     tests     (scandir "."))

   ;; scandir is a thin wrapper over the syscall, so the dot entries come back
   ;; too. Pinning this documents the behaviour rather than hiding it.
   (expect-contains "includes ."  .  (scandir "."))
   (expect-contains "includes .." .. (scandir "."))

   (expect-contains "reads a subdirectory" expect.clp (scandir "tests/lib"))
   ;; This is not an exact count. This directory holds the helper files of
   ;; the suite, and it gets more files as the suite grows. That number says
   ;; nothing about scandir. scandir must list each entry that is there, and
   ;; that includes the entries that start with a dot.
   (expect-gte      "subdirectory lists its files plus . and .."
                    3 (length$ (scandir "tests/lib")))

   (expect-false "missing directory returns FALSE" (scandir "/no/such/dir"))
   (expect-false "a regular file is not a directory" (scandir "makefile")))

(run-tests)
(test-summary)
