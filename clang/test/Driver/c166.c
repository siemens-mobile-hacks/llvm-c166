// REQUIRES: c166-registered-target
// RUN: %clang -### --target=c166-none-elf -mcmodel=large -c %s 2>&1 | FileCheck %s --check-prefix=DEFAULT
// RUN: %clang -### --target=c166-none-elf -mcmodel=small -c %s 2>&1 | FileCheck %s --check-prefix=SMALL
// RUN: %clang -### --target=c166-none-elf -mcmodel=medium -c %s 2>&1 | FileCheck %s --check-prefix=MEDIUM
// RUN: %clang -### --target=c166-none-elf -mcmodel=tiny -c %s 2>&1 | FileCheck %s --check-prefix=TINY
// RUN: %clang -### --target=c166-none-elf -mcmodel=huge -c %s 2>&1 | FileCheck %s --check-prefix=HUGE
// RUN: %clang -### --target=c166-none-elf -mcpu=generic -c %s 2>&1 | FileCheck %s --check-prefix=GENERIC
// RUN: not %clang --target=c166-none-elf -mcpu=c167 -c %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=INVALID-CPU
// RUN: %clang -### --target=c166-none-elf -nostartfiles -nolibc %s 2>&1 | FileCheck %s --check-prefix=LINK
// RUN: %clang -### --target=c166-none-elf -mcmodel=small -nostartfiles -nolibc %s 2>&1 | FileCheck %s --check-prefix=SMALL-LINK
// RUN: %clang -### --target=c166-none-elf -mcmodel=medium -nostartfiles -nolibc %s 2>&1 | FileCheck %s --check-prefix=MEDIUM-LINK
// RUN: %clang -### --target=c166-none-elf -mcmodel=tiny -nostartfiles -nolibc %s 2>&1 | FileCheck %s --check-prefix=TINY-LINK
// RUN: %clang -### --target=c166-none-elf -mcmodel=huge -nostartfiles -nolibc %s 2>&1 | FileCheck %s --check-prefix=HUGE-LINK
// RUN: %clang --target=c166-none-elf -mcmodel=small --print-libgcc-file-name | FileCheck %s --check-prefix=SMALL-RT
// RUN: %clang --target=c166-none-elf -mcmodel=medium --print-libgcc-file-name | FileCheck %s --check-prefix=MEDIUM-RT
// RUN: %clang --target=c166-none-elf -mcmodel=tiny --print-libgcc-file-name | FileCheck %s --check-prefix=TINY-RT
// RUN: %clang --target=c166-none-elf -mcmodel=huge --print-libgcc-file-name | FileCheck %s --check-prefix=HUGE-RT
// RUN: %clang -### --target=c166-none-elf -mcmodel=small -x assembler-with-cpp -c %s 2>&1 | FileCheck %s --check-prefix=SMALL-AS
// RUN: %clang -### --target=c166-none-elf -mcmodel=medium -x assembler-with-cpp -c %s 2>&1 | FileCheck %s --check-prefix=MEDIUM-AS
// RUN: %clang -### --target=c166-none-elf -mcmodel=tiny -x assembler-with-cpp -c %s 2>&1 | FileCheck %s --check-prefix=TINY-AS
// RUN: %clang -### --target=c166-none-elf -mcmodel=huge -x assembler-with-cpp -c %s 2>&1 | FileCheck %s --check-prefix=HUGE-AS
// RUN: not %clang -### --target=c166-none-elf -mcmodel=bogus -c %s 2>&1 | FileCheck %s --check-prefix=INVALID-MODEL

// DEFAULT: "-cc1"
// DEFAULT-SAME: "-triple" "c166-unknown-none-elf"
// DEFAULT-SAME: "-mframe-pointer=none"
// DEFAULT-SAME: "-mcmodel=large"
// DEFAULT-SAME: "-target-cpu" "c166"

// SMALL: "-cc1"
// SMALL-SAME: "-triple" "c166-unknown-none-elf"
// SMALL-SAME: "-mcmodel=small"

// MEDIUM: "-cc1"
// MEDIUM-SAME: "-triple" "c166-unknown-none-elf"
// MEDIUM-SAME: "-target-abi" "medium"
// MEDIUM-SAME: "-mcmodel=medium"

// TINY: "-cc1"
// TINY-SAME: "-target-abi" "tiny"
// TINY-SAME: "-mcmodel=tiny"

// HUGE: "-cc1"
// HUGE-SAME: "-target-abi" "huge"
// HUGE-SAME: "-mcmodel=large"

// GENERIC: "-cc1"
// GENERIC-SAME: "-target-cpu" "generic"

// INVALID-CPU: error: unknown target CPU 'c167'
// INVALID-CPU-NEXT: note: valid target CPU values are: c166, generic

// LINK: ld.lld"
// LINK-SAME: "-m" "c166elf"
// LINK-SAME: "--start-group"
// LINK-SAME: libclang_rt.builtins.a"
// LINK-SAME: "--end-group"

// SMALL-LINK: ld.lld"
// SMALL-LINK-SAME: "--start-group"
// SMALL-LINK-SAME: libclang_rt.builtins-small.a"
// SMALL-LINK-SAME: "--end-group"

// MEDIUM-LINK: ld.lld"
// MEDIUM-LINK-SAME: "--start-group"
// MEDIUM-LINK-SAME: libclang_rt.builtins-medium.a"
// MEDIUM-LINK-SAME: "--end-group"

// TINY-LINK: ld.lld"
// TINY-LINK-SAME: libclang_rt.builtins-tiny.a"

// HUGE-LINK: ld.lld"
// HUGE-LINK-SAME: libclang_rt.builtins-huge.a"

// SMALL-RT: libclang_rt.builtins-small.a
// MEDIUM-RT: libclang_rt.builtins-medium.a
// TINY-RT: libclang_rt.builtins-tiny.a
// HUGE-RT: libclang_rt.builtins-huge.a

// SMALL-AS: "-cc1as"
// SMALL-AS-SAME: "-target-abi" "small"
// MEDIUM-AS: "-cc1as"
// MEDIUM-AS-SAME: "-target-abi" "medium"

// TINY-AS: "-cc1as"
// TINY-AS-SAME: "-target-abi" "tiny"
// HUGE-AS: "-cc1as"
// HUGE-AS-SAME: "-target-abi" "huge"

// INVALID-MODEL: error: unsupported argument 'bogus' to option '-mcmodel=' for target 'c166-unknown-none-elf'
