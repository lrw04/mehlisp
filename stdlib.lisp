(define (reverse lst)
  (define (reverse-iter lst acc)
    (if (null lst)
        acc
        (reverse-iter (cdr lst) (cons (car lst) acc))))
  (reverse-iter lst nil))

(define (zip-cars lst)
  (define (zip-cars-iter lst acc)
    (if (null lst)
        (reverse acc)
        (zip-cars-iter (cdr lst) (cons (car (car lst)) acc))))
  (zip-cars-iter lst nil))

(define (zip-cadrs lst)
  (define (zip-cadrs-iter lst acc)
    (if (null lst)
        (reverse acc)
        (zip-cadrs-iter (cdr lst) (cons (car (cdr (car lst))) acc))))
  (zip-cadrs-iter lst nil))

(defmacro let (var . forms)
  `((lambda ,(zip-cars var) ,@forms) . ,(zip-cadrs var)))
