; Um pequeno programa para o runtime de bootstrap.

(defun fatorial (n)
  (if (< n 2)
      1
      (* n (fatorial (- n 1)))))

(defmacro quando (teste &rest corpo)
  (list 'if teste (cons 'progn corpo) nil))

(define mensagem "Ola do Sefirah Lisp")
(quando (= (fatorial 5) 120)
  (print mensagem)
  (list 'fatorial-de-5 (fatorial 5)))
