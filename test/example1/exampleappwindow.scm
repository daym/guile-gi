(define-module (test example1 exampleappwindow)
  #:use-module (gi)
  #:use-module (gi gtk-3)
  #:export (example-app-window-new))

(define EXAMPLE_APP_WINDOW_TYPE
  (register-type
   "ExampleAppWindow"                   ; type name
   <GtkApplicationWindow>               ; parent_type
   #f                                   ; No additional properties
   #f                                   ; No new signals
   #f))                                 ; No disposer func

(define (example-app-window-new app)
  (make-gobject
   EXAMPLE_APP_WINDOW_TYPE
   `(("application" . ,(cast app <GtkApplication>)))))
