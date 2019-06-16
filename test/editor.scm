;; Copyright (C) 2019  Michael L. Gran
;; Copyright (C) 2019  Jan (janneke) Nieuwenhuizen <janneke@gnu.org>

;; This program is free software: you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.

;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with this program.  If not, see <https:;;www.gnu.org/licenses/>.
(use-modules (gi))

(typelib-load "Gio" "2.0")
(typelib-load "Gdk" "3.0")
(typelib-load "Gtk" "3.0")
(typelib-load "GLib" "2.0")
(typelib-load "WebKit2" "4.0")

;; Oddly, the introspection information does not provide a constructor
;; for GtkTextIter.
(define (TextIter-new)
  (make-gstruct <GtkTextIter>))

(define (print-goodbye widget data)
  (display "Goodbye World\n"))

(define (key-press widget event xtra)
  (format #t "key: ~s\n" (send event (get-keyval?)))
  #f)

(define (activate app user-data)
  (let ((window (cast (ApplicationWindow-new app) <GtkApplicationWindow>))
        (vbox (cast (VBox-new 0 0) <GtkVBox>))
        (editor (cast (TextView-new) <GtkTextView>))
        (button-box (cast (ButtonBox-new 0) <GtkButtonBox>))
        (button (Button-new-with-label "Quit"))
        (button2 (Button-new-with-label "Hello")))
    (send editor (add-events EVENT_MASK_KEY_PRESS_MASK))
    (send window (set-title "Window"))
    (send window (set-default-size 200 200))
    (send window (show-all))
    (send window (add vbox))
    (send vbox (add editor))
    (send vbox (add button-box))
    (connect button (clicked print-goodbye #f))
    (connect button (clicked (lambda x
                               (send window (destroy)))
                             #f))
    (connect editor (key-press-event key-press #f))

    ;; When the 'hello' button is clicked, write the current contents
    ;; of the editor to the console, and replace the buffer contents
    ;; with 'Hello, world'.
    (connect button2 (clicked (lambda x
                                (let ((buffer (send editor (get-buffer)))
                                      (iter1 (TextIter-new))
                                      (iter2 (TextIter-new)))
                                  (send buffer (get-bounds iter1 iter2))
                                  (let ((txt (send buffer (get-text iter1 iter2 #t))))
                                    (write txt) (newline))
                                  (send buffer (set-text "Hello, world" 12))))
                              #f))
                                      
    (send editor (grab-focus))
    (send button-box (add button2))
    (send button-box (add button))
    (send window (show-all))))

(define (main)
  (let ((app (Application-new "org.gtk.example" 0)))
    (connect app (activate activate #f))
    (send app (run (length (command-line)) (command-line)))))

(main)
