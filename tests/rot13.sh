#!/usr/local/bin/slate

ch@(String Character traits) rot13
[| value upper |
  upper: ch isUppercase.
  value: (ch toLowercase as: Integer).
  (value >= 97) /\ (value < 110)
    ifTrue: [value: value + 13]
    ifFalse: [(value > 109) /\ (value <= 122)
                ifTrue: [value: value - 13]].
  upper
    ifTrue: [(value as: String Character) toUppercase]
    ifFalse: [value as: String Character]
].

s@(String traits) rot13
[| result |
  result: s newSameSize.
  s doWithIndex: [| :each :index |
    result at: index put: each rot13].
  result
].

lobby define: #Rot13Encoder &parents: {Encoder}.


c@(Rot13Encoder traits) convert
[
  [c in isAtEnd] whileFalse: [c out nextPut: c in next rot13].
].


(Rot13Encoder newFrom: Console reader to: Console writer) convert.



