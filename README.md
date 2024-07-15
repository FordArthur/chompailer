# Chompailer (The Glorious Chorilang Compiler)

### Chorilang

Chorilang's syntax is similar to Haskell's:


```haskell
infixl 5 *;
* :: Num a => a -> a -> a;
* a b = times a b;

let my_global_var = "local vars are declared the same";
let my_global_var_with_types :: WayOfTypingVars = expression :: AnotherWayOfTypingVars; --// They are (will) be equivalent because of the typechecker

func :: Type, Type, Type -> Type
func arg1 arg2 arg3 = expression1; --// (Comments are actually C style, just using Haskell coments for the highlighting)
                          expression2; --// lines dont matter, nor indentation
                          exp;
                          return_expression;
main :: IO Unit
main = exp;
```
