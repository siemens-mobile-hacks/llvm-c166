add_llvm_component_library(LLVMC166Disassembler
  C166Disassembler.cpp

  LINK_COMPONENTS
  C166Info
  MC
  MCDisassembler
  Support

  ADD_TO_COMPONENT
  C166
  )
