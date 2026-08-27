; RUN: llvm-mc -triple=c166-none-elf -show-encoding %s | FileCheck %s --check-prefix=ENC
; RUN: llvm-mc -triple=c166-none-elf -filetype=obj %s | llvm-readobj --file-headers --sections --symbols --relocations - | FileCheck %s --check-prefix=ELF
; RUN: llvm-mc -triple=c166-none-elf -filetype=obj %s | llvm-objdump -dr - | FileCheck %s --check-prefix=DIS

; Relocation numbers and SEG/SOF expression syntax belong to LLVM's C166 ELF
; ABI.

.text
.globl caller
.type caller,@function
caller:
  calla cc_uc, cof(near_callee)
; ENC: calla cc_uc, cof(near_callee)
; ENC-SAME: encoding: [0xca,0x00,A,A]
; ENC: fixup A - offset: 2, value: near_callee, kind: fixup_c166_cof16

  jmpa cc_uc, cof(near_branch)
; ENC: jmpa cc_uc, cof(near_branch)
; ENC-SAME: encoding: [0xea,0x00,A,A]
; ENC: fixup A - offset: 2, value: near_branch, kind: fixup_c166_cof16

  calli cc_uc, [r15]
; ENC: calli cc_uc, [r15]
; ENC-SAME: encoding: [0xab,0x0f]
; DIS: ab 0f{{.*}}calli cc_uc, [r15]

  ret
; ENC: ret
; ENC-SAME: encoding: [0xcb,0x00]
; DIS: cb 00{{.*}}ret

  calls seg(callee), sof(callee)
; ENC: calls seg(callee), sof(callee)
; ENC-SAME: encoding: [0xda,A,B,B]
; ENC: fixup A - offset: 1, value: callee, kind: fixup_c166_seg8
; ENC: fixup B - offset: 2, value: callee, kind: fixup_c166_sof16

  jmps seg(far_branch), sof(far_branch)
; ENC: jmps seg(far_branch), sof(far_branch)
; ENC-SAME: encoding: [0xfa,A,B,B]
; ENC: fixup A - offset: 1, value: far_branch, kind: fixup_c166_seg8
; ENC: fixup B - offset: 2, value: far_branch, kind: fixup_c166_sof16

  calls seg(0x123456), sof(0x123456)
; ENC: calls seg(1193046), sof(1193046)
; ENC-SAME: encoding: [0xda,A,B,B]
; ENC: fixup A - offset: 1, value: 1193046, kind: fixup_c166_seg8
; ENC: fixup B - offset: 2, value: 1193046, kind: fixup_c166_sof16
; DIS: da 12 56 34{{.*}}calls 18, 13398

  extp pag(far_data), #1
; ENC: extp pag(far_data), #1
; ENC-SAME: encoding: [0xd7,0x40,A,0b000000AA]
; ENC: fixup A - offset: 2, value: far_data, kind: fixup_c166_pag10

  mov r4, pof(far_data)
; ENC: mov r4, pof(far_data)
; ENC-SAME: encoding: [0xf2,0xf4,A,0b00AAAAAA]
; ENC: fixup A - offset: 2, value: far_data, kind: fixup_c166_pof14

  mov r3, #pof(far_data)
; ENC: mov r3, #pof(far_data)
; ENC-SAME: encoding: [0xe6,0xf3,A,0b00AAAAAA]
; ENC: fixup A - offset: 2, value: far_data, kind: fixup_c166_pof14

  mov r4, #pag(far_data)
; ENC: mov r4, #pag(far_data)
; ENC-SAME: encoding: [0xe6,0xf4,A,0b000000AA]
; ENC: fixup A - offset: 2, value: far_data, kind: fixup_c166_pag10

  mov pof(far_data), r12
; ENC: mov pof(far_data), r12
; ENC-SAME: encoding: [0xf6,0xfc,A,0b00AAAAAA]
; ENC: fixup A - offset: 2, value: far_data, kind: fixup_c166_pof14

  mov r4, dpp1(xnear_data)
; ENC: mov r4, dpp1(xnear_data)
; ENC-SAME: encoding: [0xf2,0xf4,A,0x40'A']
; ENC: fixup A - offset: 2, value: xnear_data, kind: fixup_c166_dpp1_16

  mov dpp1(xnear_data), r12
; ENC: mov dpp1(xnear_data), r12
; ENC-SAME: encoding: [0xf6,0xfc,A,0x40'A']
; ENC: fixup A - offset: 2, value: xnear_data, kind: fixup_c166_dpp1_16

  mov r5, dpp2(near_data)
; ENC: mov r5, dpp2(near_data)
; ENC-SAME: encoding: [0xf2,0xf5,A,0x80'A']
; ENC: fixup A - offset: 2, value: near_data, kind: fixup_c166_dpp2_16

  mov dpp2(near_data), r13
; ENC: mov dpp2(near_data), r13
; ENC-SAME: encoding: [0xf6,0xfd,A,0x80'A']
; ENC: fixup A - offset: 2, value: near_data, kind: fixup_c166_dpp2_16

  mov r6, #dpp1(xnear_data)
; ENC: mov r6, #dpp1(xnear_data)
; ENC-SAME: encoding: [0xe6,0xf6,A,A]
; ENC: fixup A - offset: 2, value: xnear_data, kind: fixup_c166_dpp1_16

  mov r7, #dpp2(near_data)
; ENC: mov r7, #dpp2(near_data)
; ENC-SAME: encoding: [0xe6,0xf7,A,A]
; ENC: fixup A - offset: 2, value: near_data, kind: fixup_c166_dpp2_16

  mov r8, #cof(near_callee)
; ENC: mov r8, #cof(near_callee)
; ENC-SAME: encoding: [0xe6,0xf8,A,A]
; ENC: fixup A - offset: 2, value: near_callee, kind: fixup_c166_cof16

  jmpr cc_eq, external_branch
; ENC: jmpr cc_eq, external_branch
; ENC-SAME: encoding: [0x2d,A]
; ENC: fixup A - offset: 1, value: external_branch, kind: fixup_c166_pc8

.data
.byte ext8
.short ext16
.long ext32
.long paged(extpaged)

; ELF: Type: Relocatable
; ELF: Machine: EM_C166 (0x74)
; ELF: Flags [ (0x121)
; ELF: EF_C166_CODE_HUGE
; ELF: EF_C166_CORE_8X166
; ELF: EF_C166_DATA_FAR
; ELF: Name: .rela.text
; ELF: Name: .rela.data
; ELF: R_C166_COF16 near_callee
; ELF: R_C166_COF16 near_branch
; ELF: R_C166_SEG8 callee
; ELF: R_C166_SOF16 callee
; ELF: R_C166_SEG8 far_branch
; ELF: R_C166_SOF16 far_branch
; ELF: R_C166_PAG10 far_data
; ELF: R_C166_POF14 far_data
; ELF: R_C166_POF14 far_data
; ELF: R_C166_PAG10 far_data
; ELF: R_C166_POF14 far_data
; ELF: R_C166_DPP1_16 xnear_data
; ELF: R_C166_DPP1_16 xnear_data
; ELF: R_C166_DPP2_16 near_data
; ELF: R_C166_DPP2_16 near_data
; ELF: R_C166_DPP1_16 xnear_data
; ELF: R_C166_DPP2_16 near_data
; ELF: R_C166_COF16 near_callee
; ELF: R_C166_PC8_RELAX external_branch
; ELF: R_C166_8 ext8
; ELF: R_C166_16 ext16
; ELF: R_C166_32 ext32
; ELF: R_C166_PAGED32 extpaged
; ELF-DAG: Name: caller
; ELF-DAG: Name: callee
; ELF-DAG: Name: near_callee
; ELF-DAG: Name: near_branch
; ELF-DAG: Name: far_data
; ELF-DAG: Name: xnear_data
; ELF-DAG: Name: near_data
; ELF-DAG: Name: external_branch
; ELF-DAG: Name: extpaged
