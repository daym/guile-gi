(use-modules (gi) (gi repository)
             (test automake-test-lib))

(unless (false-if-exception (require "Gio" "2.0"))
  (exit EXIT_SKIPPED))

(load-by-name "Gio" "File")

(automake-test
 (equal? "/tmp"
         (file:get-path
          (file:get-child (file:new-for-path "/") "tmp"))))
