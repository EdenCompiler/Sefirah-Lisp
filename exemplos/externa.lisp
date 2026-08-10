(defun chamar_dobro (valor)
  (external-i64 "dobrar_i64" valor))

(defun combinar_externo (a b)
  (external-i64 "combinar_i64" a b))
