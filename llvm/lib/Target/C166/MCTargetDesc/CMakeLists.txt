add_llvm_component_library(LLVMC166Desc
  C166AsmBackend.cpp
  C166BitExpr.cpp
  C166ELFObjectWriter.cpp
  C166InstPrinter.cpp
  C166MCAsmInfo.cpp
  C166MCCodeEmitter.cpp
  C166MCTargetDesc.cpp
  C166TargetStreamer.cpp

  LINK_COMPONENTS
  C166Info
  MC
  Support

  ADD_TO_COMPONENT
  C166
  )
