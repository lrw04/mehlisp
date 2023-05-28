(set! list (lambda x x))
(set! define (syntax (form . values)
               (if (symbolp form)
                   (list 'set! form (car values))
                   (list 'set! (car form) (cons 'lambda (cons (cdr form) values))))))
(define not null)
(define and (syntax l
              (if (null l)
                  t
                  (if (null (cdr l))
                      (car l)
                      (list 'if (car l) (cons 'and (cdr l)) nil)))))
(define (bindings->formals bindings)
  (if (null bindings)
      nil
      (cons (car (car bindings)) (bindings->formals (cdr bindings)))))
(define (bindings->values bindings)
  (if (null bindings)
      nil
      (cons (car (cdr (car bindings))) (bindings->values (cdr bindings)))))
(define let (syntax (bindings . body)
              (cons (cons 'lambda (cons (bindings->formals bindings) body)) 
                    (bindings->values bindings))))
(define progn (syntax l
                (if (null l)
                    nil
                    (if (null (cdr l))
                        (car l)
                        (let ((s (gensym)))
                          (list 'let (list (list s (car l)))
                            (cons 'progn (cdr l))))))))
(define or (syntax l
             (if (null l)
                 nil
                 (if (null (cdr l))
                     (car l)
                     (let ((s (gensym)))
                       (list 'let (list (list s (car l)))
                         (list 'if s s (cons 'or (cdr l)))))))))
(define let* (syntax (bindings . body)
               (if (null bindings)
                   (cons 'let (cons nil body))
                   (list 'let 
                         (list (car bindings)) 
                         (cons 'let* (cons (cdr bindings) body))))))
; `(let (,(car bindings)) (let* ,(cdr bindings) ,@))
(define (println val)
  (display val)
  (newline))
