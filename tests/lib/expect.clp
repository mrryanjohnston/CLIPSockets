;;; Assertion vocabulary for the CLIPSockets test suite.
;;;
;;; This file contains constructs only, so test files pull it in quietly with
;;; (load* "tests/lib/expect.clp"). Tests are run from the repository root.
;;;
;;; The builtin (assert) belongs to the fact base, so assertions here are
;;; named expect-*. A test file looks like:
;;;
;;;   (load* "tests/lib/expect.clp")
;;;   (test-suite "errno")
;;;   (expect-eq "errno starts clean" 0 (errno))
;;;   (test-summary)
;;;
;;; Passing checks are silent; failures print a diagnostic. (test-summary)
;;; prints the sentinel line that run.sh looks for and exits 1 if anything
;;; failed, so a test file that dies early cannot be mistaken for a pass.

(defglobal ?*tests-run*     = 0)
(defglobal ?*tests-failed*  = 0)
(defglobal ?*tests-skipped* = 0)
(defglobal ?*planned*       = 0)
(defglobal ?*suite*         = "unnamed")

(deffunction test-suite (?name)
   (bind ?*suite* ?name)
   (return ?name))

;; Declare how many checks the file should run. A CLIPS evaluation error --
;; a UDF returning void where a value was expected, say -- halts the enclosing
;; deffunction without failing anything, so the remaining checks are simply
;; never reached. Without a planned count that silently looks like a pass.
(deffunction test-plan (?n)
   (bind ?*planned* ?n)
   (return ?n))

(deffunction test-pass ()
   (bind ?*tests-run* (+ ?*tests-run* 1))
   (return TRUE))

;; A check that the library cannot do. Examples are a protocol version that the
;; build does not have, and a failure that the library reports through a
;; channel that it does not have. The count includes a skipped check as a check
;; that ran, because the plan finds a run that stopped early and a skipped check
;; stopped nothing. A skipped check never fails.
;;
;; No code in this file decides which checks are skippable. That decision
;; belongs with the code under test, and tests/lib/tls.clp makes it for the TLS
;; backends. Each test loads this file, and some builds do not have those
;; functions.
(deffunction test-skip (?label ?why)
   (bind ?*tests-run* (+ ?*tests-run* 1))
   (bind ?*tests-skipped* (+ ?*tests-skipped* 1))
   (printout t "  SKIP [" ?*suite* "] " ?label crlf)
   (printout t "       " ?why crlf)
   (return TRUE))

(deffunction test-fail (?label ?detail)
   (bind ?*tests-run* (+ ?*tests-run* 1))
   (bind ?*tests-failed* (+ ?*tests-failed* 1))
   (printout t "  FAIL [" ?*suite* "] " ?label crlf)
   (printout t "       " ?detail crlf)
   (return FALSE))

(deffunction expect-eq (?label ?expected ?actual)
   (if (eq ?expected ?actual)
      then (test-pass)
      else (test-fail ?label (str-cat "expected <" ?expected "> got <" ?actual ">"))))

(deffunction expect-neq (?label ?unexpected ?actual)
   (if (neq ?unexpected ?actual)
      then (test-pass)
      else (test-fail ?label (str-cat "expected anything but <" ?unexpected ">"))))

(deffunction expect-true (?label ?actual)
   (if (neq ?actual FALSE)
      then (test-pass)
      else (test-fail ?label "expected a non-FALSE value, got FALSE")))

(deffunction expect-false (?label ?actual)
   (if (eq ?actual FALSE)
      then (test-pass)
      else (test-fail ?label (str-cat "expected FALSE, got <" ?actual ">"))))

;; Numeric comparisons, for values that are only bounded (a timeout that must
;; be at least what we set, a byte count that must be positive, and so on).
(deffunction expect-gte (?label ?floor ?actual)
   (if (and (numberp ?actual) (>= ?actual ?floor))
      then (test-pass)
      else (test-fail ?label (str-cat "expected >= " ?floor ", got <" ?actual ">"))))

(deffunction expect-lte (?label ?ceiling ?actual)
   (if (and (numberp ?actual) (<= ?actual ?ceiling))
      then (test-pass)
      else (test-fail ?label (str-cat "expected <= " ?ceiling ", got <" ?actual ">"))))

;; Multifield helpers. The trailing $? soaks up a multifield returned by a UDF,
;; so these are called as (expect-contains "label" item (scandir ".")).
(deffunction expect-contains (?label ?item $?mf)
   (if (member$ ?item ?mf)
      then (test-pass)
      else (test-fail ?label (str-cat "<" ?item "> not found in <" (implode$ ?mf) ">"))))

(deffunction expect-not-contains (?label ?item $?mf)
   (if (not (member$ ?item ?mf))
      then (test-pass)
      else (test-fail ?label (str-cat "<" ?item "> unexpectedly present"))))

(deffunction expect-length (?label ?n $?mf)
   (if (= ?n (length$ ?mf))
      then (test-pass)
      else (test-fail ?label (str-cat "expected " ?n " fields, got " (length$ ?mf)
                                      " <" (implode$ ?mf) ">"))))

;; Error-path helper: assert the errno left behind by the preceding call.
;; Call it immediately after the failing UDF, before anything that could
;; clobber errno.
(deffunction expect-errno (?label ?sym)
   (bind ?actual (errno-sym))
   (if (eq ?actual ?sym)
      then (test-pass)
      else (test-fail ?label (str-cat "expected errno " ?sym ", got <" ?actual ">"))))

;; Captures the text that a UDF writes to STDERR.
;;
;; A message is part of what these functions give. Most of them answer a bad
;; argument with FALSE, and FALSE alone does not name the argument and does not
;; give the cause. The message on STDERR does that, and it needs a test of
;; its own. A message that names the incorrect function, or that has no newline
;; at the end, is a defect that the return value alone cannot show.
;;
;; (dribble-on) makes this capture possible. Its router answers for STDERR and
;; also for STDOUT. As a result, each write to either channel goes into the
;; file and also goes to its usual destination. No other function in CLIPS can
;; see STDERR from inside the process.
;;
;; Use it in this manner:
;;
;;   (capture-start)
;;   (getsockopt not-a-socket SOL_SOCKET SO_REUSEADDR)
;;   (bind ?said (capture-lines))
;;   (expect-contains "getsockopt names itself" "getsockopt: ..." ?said)
;;
;; No code between the two calls can print, and that includes the expect-*
;; functions. The capture takes that output also. Keep the call under test
;; alone.
;;
;; The function gives lines and not one string, because the division into lines
;; is the check. A message with no newline is not a line of its own. As a
;; result, an exact match against a line shows the terminator and also the
;; text.
(defglobal ?*capture-path* = "/tmp/clipsockets-test-capture.txt")

(deffunction capture-start ()
   (return (dribble-on ?*capture-path*)))

(deffunction capture-lines ()
   (dribble-off)
   (bind ?lines (create$))
   (if (open ?*capture-path* capture-file "r") then
      (while (neq (bind ?line (readline capture-file)) EOF) do
         (bind ?lines (create$ ?lines ?line)))
      (close capture-file))
   (remove ?*capture-path*)
   (return ?lines))

;; Ends the run. The ##SUMMARY line is the sentinel run.sh requires: without
;; it the file is reported as incomplete rather than passing.
(deffunction test-summary ()
   (if (and (> ?*planned* 0) (neq ?*planned* ?*tests-run*))
      then
      (printout t "  FAIL [" ?*suite* "] planned " ?*planned*
                  " checks but ran " ?*tests-run* crlf)
      (printout t "       execution halted early -- look for a CLIPS error above" crlf)
      (bind ?*tests-failed* (+ ?*tests-failed* 1)))

   (printout t "##SUMMARY suite=" ?*suite*
               " run=" ?*tests-run*
               " planned=" ?*planned*
               " failed=" ?*tests-failed*
               " skipped=" ?*tests-skipped* crlf)
   (if (> ?*tests-failed* 0)
      then (exit 1)
      else (exit 0)))
