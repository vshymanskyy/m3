(module
  (import "mod" "mem" (memory 0 1))
  (type (;0;) (func (result i32)))
  (func (;0;) (type 0) (result i32)
    i32.const 0
  )
  (export "_start" (func 0)))

;; This module can be parsed, but not executed, due to the import that
;; isn't supported by run-wast.py.
