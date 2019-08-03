(use-modules (gi)
             (test automake-test-lib))

(typelib-require ("GLib" "2.0"))

(automake-test
 (let* ([date (date:new-dmy 25 (make <%GDateMonth> 'december) 1990)]
        [date2 (copy date)])

   ;; Clear one GDate starting at the memory location in date2.  This
   ;; API is not great, since creating more than one contiguous
   ;; <GDate> isn't trivial using the introspected functions.
   (clear date2 1)

   (write date) (newline)
   (write date2) (newline)

   (not (valid? date2))))
