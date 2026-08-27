.section .symbols,"aw",@progbits
.p2align 1
.globl global_object
.type global_object,@object
global_object:
  .short 0
.size global_object, .-global_object

.p2align 1
.weak weak_object
.type weak_object,@object
weak_object:
  .short 0
.size weak_object, .-weak_object

.comm common_object, 2, 2

.section .text,"ax",@progbits
.p2align 1
.globl global_code
.type global_code,@function
global_code:
  .short 0
.size global_code, .-global_code

.p2align 1
.weak weak_code
.type weak_code,@function
weak_code:
  .short 0
.size weak_code, .-weak_code

.comm common_code, 2, 2
