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
   (expect-contains "lists src"       src       (scandir "."))
   (expect-contains "lists tests"     tests     (scandir "."))

   ;; scandir is a thin wrapper over the syscall, so the dot entries come back
   ;; too. Pinning this documents the behaviour rather than hiding it.
   (expect-contains "includes ."  .  (scandir "."))
   (expect-contains "includes .." .. (scandir "."))

   (expect-contains "reads a subdirectory" expect.clp (scandir "tests/lib"))
   (expect-length   "subdirectory has exactly one file plus . and .."
                    3 (scandir "tests/lib"))

   (expect-false "missing directory returns FALSE" (scandir "/no/such/dir"))
   (expect-false "a regular file is not a directory" (scandir "makefile")))

(run-tests)
(test-summary)
