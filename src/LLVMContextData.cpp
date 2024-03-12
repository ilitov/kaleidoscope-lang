#include "LLVMContextData.hpp"

LLVMOptContextData::LLVMOptContextData(llvm::LLVMContext &llvmCtx)
    : m_SI(llvmCtx, true /* Debug logging */) {
    m_SI.registerCallbacks(m_PIC, &m_MAM);

    // Add transform passes
    //
    // Do simple "peephole" optimizations and bit-twiddling optzns
    m_FPM.addPass(llvm::InstCombinePass());
    // Reassociate expressions
    m_FPM.addPass(llvm::ReassociatePass());
    // Eliminate Common SubExpressions
    m_FPM.addPass(llvm::GVNPass());
    // Simplify the control flow graph (deleting unreachable blocks, etc)
    m_FPM.addPass(llvm::SimplifyCFGPass());

    // Register analysis passes used in these transform passes
    llvm::PassBuilder passBuilder;
    passBuilder.registerModuleAnalyses(m_MAM);
    passBuilder.registerFunctionAnalyses(m_FAM);
    passBuilder.crossRegisterProxies(m_LAM, m_FAM, m_CGAM, m_MAM);
}

LLVMContextData::LLVMContextData(std::string_view moduleName)
    : m_llvmContext(std::make_unique<llvm::LLVMContext>())
    , m_builder(*m_llvmContext)
    , m_llvmModule(std::make_unique<llvm::Module>(moduleName, *m_llvmContext))
    , m_namedValues()
    , m_llvmOpt(*m_llvmContext) {
}
