(use-modules (gi)
             (test automake-test-lib))

(typelib-require ("GLib" "2.0") ("Gtk" "3.0"))

;; This test exercises float32 argument conversions and return types.
(automake-test
 (begin
   (init)
   (let ([entry (cast (entry:new) <GtkEntry>)])
     (with-object entry (set-alignment 0))
     (format #t "entry: ~S~%" entry)
     (format #t "entry alignment: ~S~%" (with-object entry (get-alignment)))
     (= 0 (with-object entry (get-alignment))))))