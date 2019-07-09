;; bytes_new_null_size.scm - testing bindings of GLib's array functions
;; Copyright (C) 2018 Michael L. Gran
;;
;;  This program is free software: you can redistribute it and/or modify
;;  it under the terms of the GNU General Public License as published by
;;  the Free Software Foundation, either version 3 of the License, or
;;  (at your option) any later version.
;;
;;  This program is distributed in the hope that it will be useful,
;;  but WITHOUT ANY WARRANTY; without even the implied warranty of
;;  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;  GNU General Public License for more details.

;;  You should have received a copy of the GNU General Public License
;;  along with this program.  If not, see <http://www.gnu.org/licenses/>.

(use-modules (gi)
             (test automake-test-lib))

(typelib-require ("GLib" "2.0"))

(setlocale LC_ALL "C")
(automake-test
 (begin
   (let* ((self (bytes:new #f 0))
          (siz (call-method self "get-size")))
     (format #t "New Byte Array: ~S~%" self)
     (format #t "Size: ~S~%" siz)
     (and (gbytes? self)
          (equal? 0 siz)))))
