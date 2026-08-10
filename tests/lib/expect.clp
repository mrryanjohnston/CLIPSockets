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

(defglobal ?*tests-run*    = 0)
(defglobal ?*tests-failed* = 0)
(defglobal ?*planned*      = 0)
(defglobal ?*suite*        = "unnamed")

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
               " failed=" ?*tests-failed* crlf)
   (if (> ?*tests-failed* 0)
      then (exit 1)
      else (exit 0)))
