; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
; RUN: llvm-objcopy --dump-section=.text=%t.actual --dump-section=.expected=%t.expected %t.o /dev/null
; RUN: cmp %t.actual %t.expected
; RUN: llvm-objdump -d --section=.text --no-leading-addr --no-show-raw-insn %t.o | sed -n '/^[[:space:]]/p' | llvm-mc -triple=c166 -filetype=obj -o %t.roundtrip.o
; RUN: llvm-objcopy --dump-section=.text=%t.roundtrip %t.roundtrip.o /dev/null
; RUN: cmp %t.actual %t.roundtrip

; Cover the entire SFR short-address domain, including named aliases and
; unnamed slots. Expected bytes follow the ISA formats, not decoder output.
; This tests encodings, not the presence of each register on a given device.
.macro immediate op, opcode, value
  .text
  \op sfr(index), #\value
  .section .expected,"a",@progbits
  .byte \opcode, index
  .short \value
.endm

.macro memory op, opcode, store=0
  .text
  .if \store
    \op 0x4002, sfr(index)
  .else
    \op sfr(index), 0x4002
  .endif
  .section .expected,"a",@progbits
  .byte \opcode, index
  .short 0x4002
.endm

.macro direct_source op, opcode
  .text
  \op sfr(index), sfr((index + 1) % 240)
  .section .expected,"a",@progbits
  .byte \opcode, index
  .short 0xfe00 + 2 * ((index + 1) % 240)
.endm

.macro single op, opcode
  .text
  \op sfr(index)
  .section .expected,"a",@progbits
  .byte \opcode, index
.endm

.macro alu op, base, compare=0
  immediate \op, (\base + 6), 128
  memory \op, (\base + 2)
  direct_source \op, (\base + 2)
  .if !\compare
    memory \op, (\base + 4), 1
  .endif
.endm

.set index, 0
.rept 240
  immediate mov, 0xe6, 0x1234
  immediate movb, 0xe7, 128
  memory mov, 0xf2
  memory mov, 0xf6, 1
  direct_source mov, 0xf2
  memory movb, 0xf3
  memory movb, 0xf7, 1
  direct_source movb, 0xf3
  memory movbs, 0xd2
  memory movbs, 0xd5, 1
  memory movbz, 0xc2
  memory movbz, 0xc5, 1
  alu add, 0x00
  alu addc, 0x10
  alu sub, 0x20
  alu subc, 0x30
  alu cmp, 0x40, 1
  alu xor, 0x50
  alu and, 0x60
  alu or, 0x70
  alu addb, 0x01
  alu addcb, 0x11
  alu subb, 0x21
  alu subcb, 0x31
  alu cmpb, 0x41, 1
  alu xorb, 0x51
  alu andb, 0x61
  alu orb, 0x71
  single push, 0xec
  single pop, 0xfc
  single retp, 0xeb
  immediate scxt, 0xc6, 0x1234
  memory scxt, 0xd6
  memory pcall, 0xe2
  .set index, index + 1
.endr
