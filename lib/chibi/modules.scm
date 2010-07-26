
(define (file->sexp-list file)
  (call-with-input-file file
    (lambda (in)
      (let lp ((res '()))
        (let ((x (read in)))
          (if (eof-object? x)
              (reverse res)
              (lp (cons x res))))))))

(define (module? x) (vector? x))

(define (module-ast mod) (vector-ref mod 3))
(define (module-ast-set! mod x) (vector-set! mod 3 x))

(define (analyze-module-source name mod recursive?)
  (let ((env (module-env mod))
        (dir (if (equal? name '(scheme)) "" (module-name-prefix name))))
    (define (include-source file)
      (cond ((find-module-file (string-append dir file))
             => (lambda (x) (cons 'body (file->sexp-list x))))
            (else (error "couldn't find include" file))))
    (let lp ((ls (module-meta-data mod)) (res '()))
      (cond
       ((not (pair? ls))
        (reverse res))
       (else
        (case (and (pair? (car ls)) (caar ls))
          ((import import-immutable)
           (for-each
            (lambda (m)
              (let* ((mod2-name+imports (resolve-import m))
                     (mod2-name (car mod2-name+imports)))
                (if recursive?
                    (analyze-module mod2-name #t))))
            (cdar ls))
           (lp (cdr ls) res))
          ((include)
           (lp (append (map include-source (cdar ls)) (cdr ls)) res))
          ((body)
           (let lp2 ((ls2 (cdar ls)) (res res))
             (cond
              ((pair? ls2)
               (lp2 (cdr ls2) (cons (analyze (car ls2) env) res)))
              (else
               (lp (cdr ls) res)))))
          (else
           (lp (cdr ls) res))))))))

(define (analyze-module name . o)
  (let ((recursive? (and (pair? o) (car o)))
        (res (load-module name)))
    (if (not (module-ast res))
        (module-ast-set! res (analyze-module-source name res recursive?)))
    res))

(define (module-ref mod var-name . o)
  (let ((cell (env-cell (module-env (if (module? mod) mod (load-module mod)))
                        var-name)))
    (if cell
        (cdr cell)
        (if (pair? o) (car o) (error "no binding in module" mod var-name)))))

(define (module-contains? mod var-name)
  (and (env-cell (module-env (if (module? mod) mod (load-module mod))) var-name)
       #t))

(define (containing-module x)
  (let lp1 ((ls *modules*))
    (and (pair? ls)
         (let ((env (module-env (cdar ls))))
           (let lp2 ((e-ls (env-exports env)))
             (cond ((null? e-ls) (lp1 (cdr ls)))
                   ((eq? x (cdr (env-cell env (car e-ls)))) (car ls))
                   (else (lp2 (cdr e-ls)))))))))

(define (procedure-analysis x)
  (let ((mod (containing-module x)))
    (and mod
         (let lp ((ls (module-ast (analyze-module (car mod)))))
           (and (pair? ls)
                (if (and (set? (car ls))
                         (eq? (procedure-name x) (ref-name (set-var (car ls)))))
                    (set-value (car ls))
                    (lp (cdr ls))))))))
