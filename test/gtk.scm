(use-modules (gi) (gi repository) (gi types) (gi util)
             (srfi srfi-1)
             (srfi srfi-64))

(test-begin "gtk.scm")

(define load-by-name?
  (compose
   (negate null?)
   load-by-name))

(if (false-if-exception (require "Gtk" "3.0"))
    (test-assert "init"
      (and (load-by-name? "Gtk" "init_check")
           (init-check!)))
    (begin
      (test-expect-fail "init")
      (test-assert "init" #f)))

(unless (test-passed?)
  (test-skip most-positive-fixnum))

(test-assert "load Entry"
  (every load-by-name? '("Gtk" "Gtk") '("Editable" "Entry")))

(let ((entry #f))
  (if (test-passed?)
      (set! entry (entry:new))
      (test-skip 4))

  (test-assert "entry alignment 0"
    (begin
      (set-alignment entry 0)
      (= (get-alignment entry) 0)))

  (test-assert "entry alignment 0.5"
    (begin
      (set-alignment entry 0.5)
      (= (get-alignment entry) 0.5)))

  (test-assert "entry alignment 1"
    (begin
      (set-alignment entry 1)
      (= (get-alignment entry) 1)))

  (test-equal "insert"
    5
    (editable:insert-text entry "Lorem ipsum dolor sit amet" 5 0)))

(test-assert "load Box"
  (every load-by-name? '("Gtk" "Gtk") '("Box" "Orientation")))

(unless (test-passed?)
  (test-skip 2))

(test-assert "new box"
  (let ((box (box:new (symbol->orientation 'vertical) 1)))
    (and (is-a? box <GtkBox>)
         (= 1 (spacing box)))))

(test-assert "make box"
  (let ((box (make <GtkBox> #:orientation 'vertical #:spacing 2)))
    (and (is-a? box <GtkBox>)
         (= 2 (spacing box)))))

(test-assert "load TreeStore"
  (every load-by-name? '("Gtk" "Gtk" "Gtk") '("TreeModel" "TreeStore" "TreeIter")))

(test-assert "tree-store:new (lowlevel)"
  (let ((tree-store (tree-store:new (vector G_TYPE_LONG G_TYPE_LONG G_TYPE_LONG))))
    (and (= 3 (tree-model:get-n-columns tree-store)))))

(test-assert "tree-store:set-value"
  (let* ((tree-store (tree-store:new (vector G_TYPE_LONG G_TYPE_LONG G_TYPE_LONG)))
         (tree-iter (make <GtkTreeIter>))
         (value (make <GValue>))
         (value2 (make <GValue>)))
    (begin
      (tree-store:insert! tree-store tree-iter #f -1)
      (set! (value G_TYPE_LONG) 1234)
      (tree-store:set-value tree-store tree-iter 0 value)
      (tree-model:get-value! tree-store tree-iter 0 value2)
      (equal? (value) (value2)))))

(define (long-value-vec . x)
  (list->vector
   (map
    (lambda (a)
      (let ((val (make <GValue>)))
        (set! (val G_TYPE_LONG) a)
        val))
    x)))

(test-assert "tree-store:set"
  (let* ((tree-store (tree-store:new (vector G_TYPE_LONG G_TYPE_LONG G_TYPE_LONG)))
         (tree-iter (make <GtkTreeIter>))
         (value1 (make <GValue>))
         (value2 (make <GValue>)))
    (begin
      (tree-store:insert! tree-store tree-iter #f -1)
      (tree-store:set tree-store
                      tree-iter
                      (list->int-vector '(0 1 2))
                      (long-value-vec 11 22 33))
      (tree-model:get-value! tree-store tree-iter 0 value1)
      (tree-model:get-value! tree-store tree-iter 1 value2)
      (and (equal? 11 (value1))
           (equal? 22 (value2))))))

(test-assert "load TreeView"
  (every load-by-name? '("Gtk" "Gtk") '("TreeStore" "TreeView")))

(test-assert "tree-view:set-model (lowlevel)"
  (let ((tree-store (tree-store:new (vector G_TYPE_LONG G_TYPE_LONG G_TYPE_LONG)))
        (tree-view (tree-view:new)))
    (tree-view:set-model tree-view tree-store)))

; Not possible--because the columns are not properties:
;(test-assert "make tree store (highlevel)"
;  (let ((tree-store (make <GtkTreeStore> #:columns (vector G_TYPE_LONG G_TYPE_LONG G_TYPE_LONG))))
;    (and (= 3 (get-n-columns tree-store)))))

(test-end "gtk.scm")
