// RUN: %clang --target=c166-none-elf -mcmodel=small -O0 -mllvm -verify-machineinstrs -S -o /dev/null %s
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -mllvm -verify-machineinstrs -S -o /dev/null %s
// RUN: %clang --target=c166-none-elf -mcmodel=small -O2 -mllvm -verify-machineinstrs -S -o - %s | FileCheck %s
// RUN: %clang --target=c166-none-elf -mcmodel=small -O3 -mllvm -verify-machineinstrs -S -o /dev/null %s
// RUN: %clang --target=c166-none-elf -mcmodel=small -Os -mllvm -verify-machineinstrs -S -o /dev/null %s
// RUN: %clang --target=c166-none-elf -mcmodel=small -Oz -mllvm -verify-machineinstrs -S -o /dev/null %s
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O2 -mllvm -verify-machineinstrs -S -o /dev/null %s
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -mllvm -verify-machineinstrs -S -o /dev/null %s

// Forwarding a word from the temporary double result must not disconnect the
// result from caller cleanup. In Small this previously deleted CALLSEQ_END
// and ADJSP while retaining CALLSEQ_START and the outgoing argument block.
// CHECK-LABEL: _extended_parameter_word:
// CHECK: calls seg(___extendsfdf2), sof(___extendsfdf2)
// CHECK: mov r1, [r4]
// CHECK: add r0, #12
// CHECK: rets
unsigned int extended_parameter_word(float value) {
	double extended = value;
	unsigned char *bytes = (unsigned char *)&extended;
	return bytes[0] | ((unsigned int)bytes[1] << 8);
}
