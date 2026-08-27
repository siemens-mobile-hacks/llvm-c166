.section .c166.near.data,"aw",@progbits
.p2align 1
.globl _near_data
.type _near_data,@object
_near_data:
  .long 0x12345678
.size _near_data, .-_near_data

.section .c166.xnear.data,"aw",@progbits
.p2align 1
.globl _xnear_data
.type _xnear_data,@object
_xnear_data:
  .long 0x9abcdef0
.size _xnear_data, .-_xnear_data
