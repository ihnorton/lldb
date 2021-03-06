//===-- AppleObjCRuntime.cpp --------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "AppleObjCRuntime.h"
#include "AppleObjCTrampolineHandler.h"

#include "clang/AST/Type.h"

#include "lldb/Breakpoint/BreakpointLocation.h"
#include "lldb/Core/ConstString.h"
#include "lldb/Core/Error.h"
#include "lldb/Core/Log.h"
#include "lldb/Core/Module.h"
#include "lldb/Core/ModuleList.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Core/Scalar.h"
#include "lldb/Core/Section.h"
#include "lldb/Core/StreamString.h"
#include "lldb/Expression/ClangFunction.h"
#include "lldb/Symbol/ClangASTContext.h"
#include "lldb/Symbol/ObjectFile.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/RegisterContext.h"
#include "lldb/Target/StopInfo.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Thread.h"

#include <vector>

using namespace lldb;
using namespace lldb_private;

#define PO_FUNCTION_TIMEOUT_USEC 15*1000*1000

AppleObjCRuntime::AppleObjCRuntime(Process *process) :
    ObjCLanguageRuntime (process),
    m_read_objc_library (false),
    m_objc_trampoline_handler_ap (),
    m_Foundation_major()
{
    ReadObjCLibraryIfNeeded (process->GetTarget().GetImages());
}

bool
AppleObjCRuntime::GetObjectDescription (Stream &str, ValueObject &valobj)
{
    CompilerType clang_type(valobj.GetCompilerType());
    bool is_signed;
    // ObjC objects can only be pointers (or numbers that actually represents pointers
    // but haven't been typecast, because reasons..)
    if (!clang_type.IsIntegerType (is_signed) && !clang_type.IsPointerType ())
        return false;
    
    // Make the argument list: we pass one arg, the address of our pointer, to the print function.
    Value val;
    
    if (!valobj.ResolveValue(val.GetScalar()))
        return false;
    
    ExecutionContext exe_ctx (valobj.GetExecutionContextRef());
    return GetObjectDescription(str, val, exe_ctx.GetBestExecutionContextScope());
                   
}
bool
AppleObjCRuntime::GetObjectDescription (Stream &strm, Value &value, ExecutionContextScope *exe_scope)
{
    if (!m_read_objc_library)
        return false;
        
    ExecutionContext exe_ctx;
    exe_scope->CalculateExecutionContext(exe_ctx);
    Process *process = exe_ctx.GetProcessPtr();
    if (!process)
        return false;
    
    // We need other parts of the exe_ctx, but the processes have to match.
    assert (m_process == process);
    
    // Get the function address for the print function.
    const Address *function_address = GetPrintForDebuggerAddr();
    if (!function_address)
        return false;
    
    Target *target = exe_ctx.GetTargetPtr();
    CompilerType clang_type = value.GetCompilerType();
    if (clang_type)
    {
        if (!ClangASTContext::IsObjCObjectPointerType(clang_type))
        {
            strm.Printf ("Value doesn't point to an ObjC object.\n");
            return false;
        }
    }
    else 
    {
        // If it is not a pointer, see if we can make it into a pointer.
        ClangASTContext *ast_context = target->GetScratchClangASTContext();
        CompilerType opaque_type = ast_context->GetBasicType(eBasicTypeObjCID);
        if (!opaque_type)
            opaque_type = ast_context->GetBasicType(eBasicTypeVoid).GetPointerType();
        //value.SetContext(Value::eContextTypeClangType, opaque_type_ptr);
        value.SetCompilerType (opaque_type);
    }

    ValueList arg_value_list;
    arg_value_list.PushValue(value);
    
    // This is the return value:
    ClangASTContext *ast_context = target->GetScratchClangASTContext();
    
    CompilerType return_clang_type = ast_context->GetCStringType(true);
    Value ret;
//    ret.SetContext(Value::eContextTypeClangType, return_clang_type);
    ret.SetCompilerType (return_clang_type);
    
    if (exe_ctx.GetFramePtr() == NULL)
    {
        Thread *thread = exe_ctx.GetThreadPtr();
        if (thread == NULL)
        {
            exe_ctx.SetThreadSP(process->GetThreadList().GetSelectedThread());
            thread = exe_ctx.GetThreadPtr();
        }
        if (thread)
        {
            exe_ctx.SetFrameSP(thread->GetSelectedFrame());
        }
    }
    
    // Now we're ready to call the function:
    ClangFunction func (*exe_ctx.GetBestExecutionContextScope(),
                        return_clang_type, 
                        *function_address, 
                        arg_value_list,
                        "objc-object-description");

    StreamString error_stream;
    
    lldb::addr_t wrapper_struct_addr = LLDB_INVALID_ADDRESS;
    func.InsertFunction(exe_ctx, wrapper_struct_addr, error_stream);

    EvaluateExpressionOptions options;
    options.SetUnwindOnError(true);
    options.SetTryAllThreads(true);
    options.SetStopOthers(true);
    options.SetIgnoreBreakpoints(true);
    options.SetTimeoutUsec(PO_FUNCTION_TIMEOUT_USEC);
    
    ExpressionResults results = func.ExecuteFunction (exe_ctx, 
                                                     &wrapper_struct_addr,
                                                     options,
                                                     error_stream, 
                                                     ret);
    if (results != eExpressionCompleted)
    {
        strm.Printf("Error evaluating Print Object function: %d.\n", results);
        return false;
    }
       
    addr_t result_ptr = ret.GetScalar().ULongLong(LLDB_INVALID_ADDRESS);
    
    char buf[512];
    size_t cstr_len = 0;    
    size_t full_buffer_len = sizeof (buf) - 1;
    size_t curr_len = full_buffer_len;
    while (curr_len == full_buffer_len)
    {
        Error error;
        curr_len = process->ReadCStringFromMemory(result_ptr + cstr_len, buf, sizeof(buf), error);
        strm.Write (buf, curr_len);
        cstr_len += curr_len;
    }
    return cstr_len > 0;
}

lldb::ModuleSP
AppleObjCRuntime::GetObjCModule ()
{
    ModuleSP module_sp (m_objc_module_wp.lock());
    if (module_sp)
        return module_sp;

    Process *process = GetProcess();
    if (process)
    {
        const ModuleList& modules = process->GetTarget().GetImages();
        for (uint32_t idx = 0; idx < modules.GetSize(); idx++)
        {
            module_sp = modules.GetModuleAtIndex(idx);
            if (AppleObjCRuntime::AppleIsModuleObjCLibrary(module_sp))
            {
                m_objc_module_wp = module_sp;
                return module_sp;
            }
        }
    }
    return ModuleSP();
}

Address *
AppleObjCRuntime::GetPrintForDebuggerAddr()
{
    if (!m_PrintForDebugger_addr.get())
    {
        const ModuleList &modules = m_process->GetTarget().GetImages();
        
        SymbolContextList contexts;
        SymbolContext context;
        
        if ((!modules.FindSymbolsWithNameAndType(ConstString ("_NSPrintForDebugger"), eSymbolTypeCode, contexts)) &&
           (!modules.FindSymbolsWithNameAndType(ConstString ("_CFPrintForDebugger"), eSymbolTypeCode, contexts)))
            return NULL;
        
        contexts.GetContextAtIndex(0, context);
        
        m_PrintForDebugger_addr.reset(new Address(context.symbol->GetAddress()));
    }
    
    return m_PrintForDebugger_addr.get();
}

bool
AppleObjCRuntime::CouldHaveDynamicValue (ValueObject &in_value)
{
    return in_value.GetCompilerType().IsPossibleDynamicType (NULL,
                                                          false, // do not check C++
                                                          true); // check ObjC
}

bool
AppleObjCRuntime::GetDynamicTypeAndAddress (ValueObject &in_value, 
                                            lldb::DynamicValueType use_dynamic, 
                                            TypeAndOrName &class_type_or_name, 
                                            Address &address)
{
    return false;
}

bool
AppleObjCRuntime::AppleIsModuleObjCLibrary (const ModuleSP &module_sp)
{
    if (module_sp)
    {
        const FileSpec &module_file_spec = module_sp->GetFileSpec();
        static ConstString ObjCName ("libobjc.A.dylib");
        
        if (module_file_spec)
        {
            if (module_file_spec.GetFilename() == ObjCName)
                return true;
        }
    }
    return false;
}

// we use the version of Foundation to make assumptions about the ObjC runtime on a target
uint32_t
AppleObjCRuntime::GetFoundationVersion ()
{
    if (!m_Foundation_major.hasValue())
    {
        const ModuleList& modules = m_process->GetTarget().GetImages();
        uint32_t major = UINT32_MAX;
        for (uint32_t idx = 0; idx < modules.GetSize(); idx++)
        {
            lldb::ModuleSP module_sp = modules.GetModuleAtIndex(idx);
            if (!module_sp)
                continue;
            if (strcmp(module_sp->GetFileSpec().GetFilename().AsCString(""),"Foundation") == 0)
            {
                module_sp->GetVersion(&major,1);
                m_Foundation_major = major;
                return major;
            }
        }
        return LLDB_INVALID_MODULE_VERSION;
    }
    else
        return m_Foundation_major.getValue();
}

bool
AppleObjCRuntime::IsModuleObjCLibrary (const ModuleSP &module_sp)
{
    return AppleIsModuleObjCLibrary(module_sp);
}

bool
AppleObjCRuntime::ReadObjCLibrary (const ModuleSP &module_sp)
{
    // Maybe check here and if we have a handler already, and the UUID of this module is the same as the one in the
    // current module, then we don't have to reread it?
    m_objc_trampoline_handler_ap.reset(new AppleObjCTrampolineHandler (m_process->shared_from_this(), module_sp));
    if (m_objc_trampoline_handler_ap.get() != NULL)
    {
        m_read_objc_library = true;
        return true;
    }
    else
        return false;
}

ThreadPlanSP
AppleObjCRuntime::GetStepThroughTrampolinePlan (Thread &thread, bool stop_others)
{
    ThreadPlanSP thread_plan_sp;
    if (m_objc_trampoline_handler_ap.get())
        thread_plan_sp = m_objc_trampoline_handler_ap->GetStepThroughDispatchPlan (thread, stop_others);
    return thread_plan_sp;
}

//------------------------------------------------------------------
// Static Functions
//------------------------------------------------------------------
enum ObjCRuntimeVersions
AppleObjCRuntime::GetObjCVersion (Process *process, ModuleSP &objc_module_sp)
{
    if (!process)
        return eObjC_VersionUnknown;
        
    Target &target = process->GetTarget();
    const ModuleList &target_modules = target.GetImages();
    Mutex::Locker modules_locker(target_modules.GetMutex());
    
    size_t num_images = target_modules.GetSize();
    for (size_t i = 0; i < num_images; i++)
    {
        ModuleSP module_sp = target_modules.GetModuleAtIndexUnlocked(i);
        // One tricky bit here is that we might get called as part of the initial module loading, but
        // before all the pre-run libraries get winnowed from the module list.  So there might actually
        // be an old and incorrect ObjC library sitting around in the list, and we don't want to look at that.
        // That's why we call IsLoadedInTarget.
        
        if (AppleIsModuleObjCLibrary (module_sp) && module_sp->IsLoadedInTarget(&target))
        {
            objc_module_sp = module_sp;
            ObjectFile *ofile = module_sp->GetObjectFile();
            if (!ofile)
                return eObjC_VersionUnknown;
            
            SectionList *sections = module_sp->GetSectionList();
            if (!sections)
                return eObjC_VersionUnknown;    
            SectionSP v1_telltale_section_sp = sections->FindSectionByName(ConstString ("__OBJC"));
            if (v1_telltale_section_sp)
            {
                return eAppleObjC_V1;
            }
            return eAppleObjC_V2;
        }
    }
            
    return eObjC_VersionUnknown;
}

void
AppleObjCRuntime::SetExceptionBreakpoints ()
{
    const bool catch_bp = false;
    const bool throw_bp = true;
    const bool is_internal = true;
    
    if (!m_objc_exception_bp_sp)
    {
        m_objc_exception_bp_sp = LanguageRuntime::CreateExceptionBreakpoint (m_process->GetTarget(),
                                                                            GetLanguageType(),
                                                                            catch_bp, 
                                                                            throw_bp, 
                                                                            is_internal);
        if (m_objc_exception_bp_sp)
            m_objc_exception_bp_sp->SetBreakpointKind("ObjC exception");
    }
    else
        m_objc_exception_bp_sp->SetEnabled(true);
}


void
AppleObjCRuntime::ClearExceptionBreakpoints ()
{
    if (!m_process)
        return;
    
    if (m_objc_exception_bp_sp.get())
    {
        m_objc_exception_bp_sp->SetEnabled (false);
    }
}

bool
AppleObjCRuntime::ExceptionBreakpointsAreSet ()
{
    return m_objc_exception_bp_sp && m_objc_exception_bp_sp->IsEnabled();
}

bool
AppleObjCRuntime::ExceptionBreakpointsExplainStop (lldb::StopInfoSP stop_reason)
{
    if (!m_process)
        return false;
    
    if (!stop_reason || 
        stop_reason->GetStopReason() != eStopReasonBreakpoint)
        return false;
    
    uint64_t break_site_id = stop_reason->GetValue();
    return m_process->GetBreakpointSiteList().BreakpointSiteContainsBreakpoint (break_site_id,
                                                                                m_objc_exception_bp_sp->GetID());
}

bool
AppleObjCRuntime::CalculateHasNewLiteralsAndIndexing()
{
    if (!m_process)
        return false;
    
    Target &target(m_process->GetTarget());
    
    static ConstString s_method_signature("-[NSDictionary objectForKeyedSubscript:]");
    static ConstString s_arclite_method_signature("__arclite_objectForKeyedSubscript");
    
    SymbolContextList sc_list;
    
    if (target.GetImages().FindSymbolsWithNameAndType(s_method_signature, eSymbolTypeCode, sc_list) ||
        target.GetImages().FindSymbolsWithNameAndType(s_arclite_method_signature, eSymbolTypeCode, sc_list))
        return true;
    else
        return false;
}

lldb::SearchFilterSP
AppleObjCRuntime::CreateExceptionSearchFilter ()
{
    Target &target = m_process->GetTarget();
    
    if (target.GetArchitecture().GetTriple().getVendor() == llvm::Triple::Apple)
    {
        FileSpecList filter_modules;
        filter_modules.Append(FileSpec("libobjc.A.dylib", false));
        return target.GetSearchFilterForModuleList(&filter_modules);
    }
    else
    {
        return LanguageRuntime::CreateExceptionSearchFilter();
    }
}

void
AppleObjCRuntime::ReadObjCLibraryIfNeeded (const ModuleList &module_list)
{
    if (!HasReadObjCLibrary ())
    {
        Mutex::Locker locker (module_list.GetMutex ());

        size_t num_modules = module_list.GetSize();
        for (size_t i = 0; i < num_modules; i++)
        {
            auto mod = module_list.GetModuleAtIndex (i);
            if (IsModuleObjCLibrary (mod))
            {
                ReadObjCLibrary (mod);
                break;
            }
        }
    }
}

void
AppleObjCRuntime::ModulesDidLoad (const ModuleList &module_list)
{
    ReadObjCLibraryIfNeeded (module_list);
}

