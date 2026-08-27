add_llvm_component_library(LLVMC166AsmParser
  C166AsmParser.cpp

  LINK_COMPONENTS
  C166Desc
  C166Info
  MC
  MCParser
  Support

  ADD_TO_COMPONENT
  C166
  )
