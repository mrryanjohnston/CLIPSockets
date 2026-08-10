;;; (signal <signal> SIG_IGN|SIG_DFL)
;;;
;;; Only the two dispositions exist. A CLIPS deffunction cannot be a handler:
;;; handlers run asynchronously and may only call async-signal-safe functions,
;;; so re-entering the engine from one is not an option.
;;;
;;; Each test file runs in its own process, so changing dispositions here
;;; cannot affect any other file.

(load* "tests/lib/expect.clp")
(test-suite "signal")
(test-plan 20)

(deffunction run-disposition-tests ()
   (expect-true "SIGPIPE can be ignored" (signal SIGPIPE SIG_IGN))
   (expect-true "and set back to the default" (signal SIGPIPE SIG_DFL))
   (expect-true "SIGHUP can be ignored" (signal SIGHUP SIG_IGN))
   (expect-true "SIGHUP restored" (signal SIGHUP SIG_DFL)))

(deffunction run-name-tests ()
   ;; Every settable name the switch knows. SIG_DFL is used so the process is
   ;; left in its ordinary state.
   (expect-true "SIGINT"   (signal SIGINT SIG_DFL))
   (expect-true "SIGQUIT"  (signal SIGQUIT SIG_DFL))
   (expect-true "SIGALRM"  (signal SIGALRM SIG_DFL))
   (expect-true "SIGTERM"  (signal SIGTERM SIG_DFL))
   (expect-true "SIGUSR1"  (signal SIGUSR1 SIG_DFL))
   (expect-true "SIGUSR2"  (signal SIGUSR2 SIG_DFL))
   (expect-true "SIGCHLD"  (signal SIGCHLD SIG_DFL))
   (expect-true "SIGCONT"  (signal SIGCONT SIG_DFL))
   (expect-true "SIGTSTP"  (signal SIGTSTP SIG_DFL))
   (expect-true "SIGWINCH" (signal SIGWINCH SIG_DFL)))

(deffunction run-rejection-tests ()
   ;; SIGKILL and SIGSTOP are recognised names, but the kernel refuses to let
   ;; either be caught or ignored. They are in the table so the failure says
   ;; so, rather than claiming the name is unknown.
   (expect-false "SIGKILL cannot be ignored" (signal SIGKILL SIG_IGN))
   (expect-eq    "which the kernel reports as EINVAL" EINVAL (errno-sym))
   (expect-false "SIGSTOP cannot be ignored" (signal SIGSTOP SIG_IGN))

   ;; The fault signals are deliberately absent: ignoring one re-runs the
   ;; faulting instruction forever.
   (expect-false "SIGSEGV is not offered" (signal SIGSEGV SIG_IGN))

   (expect-false "an unknown signal name" (signal NOT_A_SIGNAL SIG_IGN))
   (expect-false "an unknown disposition" (signal SIGPIPE NOT_A_DISPOSITION)))

(run-disposition-tests)
(run-name-tests)
(run-rejection-tests)
(test-summary)
