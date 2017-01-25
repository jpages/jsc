/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006 Jon Shier (jshier@iastate.edu)
 *  Copyright (C) 2003-2009, 2014 Apple Inc. All rights reseved.
 *  Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 *  Copyright (c) 2015 Canon Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "config.h"
#include "JSDOMWindowBase.h"

#include "Chrome.h"
#include "DOMWindow.h"
#include "Frame.h"
#include "InspectorController.h"
#include "JSDOMGlobalObjectTask.h"
#include "JSDOMWindowCustom.h"
#include "JSModuleLoader.h"
#include "JSNode.h"
#include "Logging.h"
#include "Page.h"
#include "RuntimeApplicationChecks.h"
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "WebCoreJSClientData.h"
#include <runtime/JSInternalPromiseDeferred.h>
#include <runtime/Microtask.h>
#include <wtf/MainThread.h>

#if PLATFORM(IOS)
#include "ChromeClient.h"
#include "WebSafeGCActivityCallbackIOS.h"
#include "WebSafeIncrementalSweeperIOS.h"
#endif

#if ENABLE(STREAMS_API)
#include "JSReadableStreamPrivateConstructors.h"
#include "ReadableStreamInternalsBuiltins.h"
#include "StreamInternalsBuiltins.h"
#include "WritableStreamInternalsBuiltins.h"
#endif

#if ENABLE(MEDIA_STREAM)
#include "RTCPeerConnectionInternalsBuiltins.h"
#endif

using namespace JSC;

namespace WebCore {

static bool shouldAllowAccessFrom(const JSGlobalObject* thisObject, ExecState* exec)
{
    return BindingSecurity::shouldAllowAccessToDOMWindow(exec, asJSDOMWindow(thisObject)->wrapped());
}

const ClassInfo JSDOMWindowBase::s_info = { "Window", &JSDOMGlobalObject::s_info, 0, CREATE_METHOD_TABLE(JSDOMWindowBase) };

const GlobalObjectMethodTable JSDOMWindowBase::s_globalObjectMethodTable = { &shouldAllowAccessFrom, &supportsProfiling, &supportsRichSourceInfo, &shouldInterruptScript, &javaScriptRuntimeFlags, &queueTaskToEventLoop, &shouldInterruptScriptBeforeTimeout, &moduleLoaderResolve, &moduleLoaderFetch, nullptr, nullptr, &moduleLoaderEvaluate };

JSDOMWindowBase::JSDOMWindowBase(VM& vm, Structure* structure, PassRefPtr<DOMWindow> window, JSDOMWindowShell* shell)
    : JSDOMGlobalObject(vm, structure, &shell->world(), &s_globalObjectMethodTable)
    , m_windowCloseWatchpoints((window && window->frame()) ? IsWatched : IsInvalidated)
    , m_wrapped(window)
    , m_shell(shell)
    , m_privateFunctions(vm)
{
}

void JSDOMWindowBase::finishCreation(VM& vm, JSDOMWindowShell* shell)
{
    Base::finishCreation(vm, shell);
    ASSERT(inherits(info()));

    m_privateFunctions.init(*this);

#if ENABLE(STREAMS_API) || ENABLE(MEDIA_STREAM)
    JSVMClientData& clientData = *static_cast<JSVMClientData*>(vm.clientData);
#endif

    GlobalPropertyInfo staticGlobals[] = {
        GlobalPropertyInfo(vm.propertyNames->document, jsNull(), DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->window, m_shell, DontDelete | ReadOnly),
#if ENABLE(STREAMS_API)
        GlobalPropertyInfo(clientData.builtinNames().streamClosedPrivateName(), jsNumber(1), DontDelete | ReadOnly),
        GlobalPropertyInfo(clientData.builtinNames().streamClosingPrivateName(), jsNumber(2), DontDelete | ReadOnly),
        GlobalPropertyInfo(clientData.builtinNames().streamErroredPrivateName(), jsNumber(3), DontDelete | ReadOnly),
        GlobalPropertyInfo(clientData.builtinNames().streamReadablePrivateName(), jsNumber(4), DontDelete | ReadOnly),
        GlobalPropertyInfo(clientData.builtinNames().streamWaitingPrivateName(), jsNumber(5), DontDelete | ReadOnly),
        GlobalPropertyInfo(clientData.builtinNames().streamWritablePrivateName(), jsNumber(6), DontDelete | ReadOnly),
        GlobalPropertyInfo(clientData.builtinNames().ReadableStreamControllerPrivateName(), createReadableStreamControllerPrivateConstructor(vm, *this), DontDelete | ReadOnly),
        GlobalPropertyInfo(clientData.builtinNames().ReadableStreamReaderPrivateName(), createReadableStreamReaderPrivateConstructor(vm, *this), DontDelete | ReadOnly),
#define DECLARE_GLOBAL_STATIC(name)\
        GlobalPropertyInfo(\
            clientData.builtinFunctions().readableStreamInternalsBuiltins().name##PrivateName(), m_privateFunctions.readableStreamInternals().m_##name##Function.get() , DontDelete | ReadOnly),
        WEBCORE_FOREACH_READABLESTREAMINTERNALS_BUILTIN_FUNCTION_NAME(DECLARE_GLOBAL_STATIC)
#undef DECLARE_GLOBAL_STATIC
#define DECLARE_GLOBAL_STATIC(name)\
        GlobalPropertyInfo(\
            clientData.builtinFunctions().streamInternalsBuiltins().name##PrivateName(), m_privateFunctions.streamInternals().m_##name##Function.get() , DontDelete | ReadOnly),
        WEBCORE_FOREACH_STREAMINTERNALS_BUILTIN_FUNCTION_NAME(DECLARE_GLOBAL_STATIC)
#undef DECLARE_GLOBAL_STATIC
#define DECLARE_GLOBAL_STATIC(name)\
        GlobalPropertyInfo(\
            clientData.builtinFunctions().writableStreamInternalsBuiltins().name##PrivateName(), m_privateFunctions.writableStreamInternals().m_##name##Function.get() , DontDelete | ReadOnly),
        WEBCORE_FOREACH_WRITABLESTREAMINTERNALS_BUILTIN_FUNCTION_NAME(DECLARE_GLOBAL_STATIC)
#undef DECLARE_GLOBAL_STATIC
#endif
#if ENABLE(MEDIA_STREAM)
#define DECLARE_GLOBAL_STATIC(name)\
        GlobalPropertyInfo(\
            clientData.builtinFunctions().rTCPeerConnectionInternalsBuiltins().name##PrivateName(), m_privateFunctions.rTCPeerConnectionInternals().m_##name##Function.get() , DontDelete | ReadOnly),
        WEBCORE_FOREACH_RTCPEERCONNECTIONINTERNALS_BUILTIN_FUNCTION_NAME(DECLARE_GLOBAL_STATIC)
#undef DECLARE_GLOBAL_STATIC
#endif
    };

    addStaticGlobals(staticGlobals, WTF_ARRAY_LENGTH(staticGlobals));
}

void JSDOMWindowBase::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSDOMWindowBase* thisObject = jsCast<JSDOMWindowBase*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    thisObject->m_privateFunctions.visit(visitor);
}

void JSDOMWindowBase::destroy(JSCell* cell)
{
    static_cast<JSDOMWindowBase*>(cell)->JSDOMWindowBase::~JSDOMWindowBase();
}

void JSDOMWindowBase::updateDocument()
{
    ASSERT(m_wrapped->document());
    ExecState* exec = globalExec();
    symbolTablePutWithAttributesTouchWatchpointSet(this, exec, exec->vm().propertyNames->document, toJS(exec, this, m_wrapped->document()), DontDelete | ReadOnly);
}

ScriptExecutionContext* JSDOMWindowBase::scriptExecutionContext() const
{
    return m_wrapped->document();
}

void JSDOMWindowBase::printErrorMessage(const String& message) const
{
    printErrorMessageForFrame(wrapped().frame(), message);
}

bool JSDOMWindowBase::supportsProfiling(const JSGlobalObject* object)
{
    const JSDOMWindowBase* thisObject = static_cast<const JSDOMWindowBase*>(object);
    Frame* frame = thisObject->wrapped().frame();
    if (!frame)
        return false;

    Page* page = frame->page();
    if (!page)
        return false;

    return page->inspectorController().profilerEnabled();
}

bool JSDOMWindowBase::supportsRichSourceInfo(const JSGlobalObject* object)
{
    const JSDOMWindowBase* thisObject = static_cast<const JSDOMWindowBase*>(object);
    Frame* frame = thisObject->wrapped().frame();
    if (!frame)
        return false;

    Page* page = frame->page();
    if (!page)
        return false;

    bool enabled = page->inspectorController().enabled();
    ASSERT(enabled || !thisObject->debugger());
    ASSERT(enabled || !supportsProfiling(thisObject));
    return enabled;
}

static inline bool shouldInterruptScriptToPreventInfiniteRecursionWhenClosingPage(Page* page)
{
    // See <rdar://problem/5479443>. We don't think that page can ever be NULL
    // in this case, but if it is, we've gotten into a state where we may have
    // hung the UI, with no way to ask the client whether to cancel execution.
    // For now, our solution is just to cancel execution no matter what,
    // ensuring that we never hang. We might want to consider other solutions
    // if we discover problems with this one.
    ASSERT(page);
    return !page;
}

bool JSDOMWindowBase::shouldInterruptScript(const JSGlobalObject* object)
{
    const JSDOMWindowBase* thisObject = static_cast<const JSDOMWindowBase*>(object);
    ASSERT(thisObject->wrapped().frame());
    Page* page = thisObject->wrapped().frame()->page();
    return shouldInterruptScriptToPreventInfiniteRecursionWhenClosingPage(page);
}

bool JSDOMWindowBase::shouldInterruptScriptBeforeTimeout(const JSGlobalObject* object)
{
    const JSDOMWindowBase* thisObject = static_cast<const JSDOMWindowBase*>(object);
    ASSERT(thisObject->wrapped().frame());
    Page* page = thisObject->wrapped().frame()->page();

    if (shouldInterruptScriptToPreventInfiniteRecursionWhenClosingPage(page))
        return true;

#if PLATFORM(IOS)
    if (page->chrome().client().isStopping())
        return true;
#endif

    return JSGlobalObject::shouldInterruptScriptBeforeTimeout(object);
}

RuntimeFlags JSDOMWindowBase::javaScriptRuntimeFlags(const JSGlobalObject* object)
{
    const JSDOMWindowBase* thisObject = static_cast<const JSDOMWindowBase*>(object);
    Frame* frame = thisObject->wrapped().frame();
    if (!frame)
        return RuntimeFlags();
    return frame->settings().javaScriptRuntimeFlags();
}

void JSDOMWindowBase::queueTaskToEventLoop(const JSGlobalObject* object, PassRefPtr<Microtask> task)
{
    const JSDOMWindowBase* thisObject = static_cast<const JSDOMWindowBase*>(object);
    thisObject->scriptExecutionContext()->postTask(JSGlobalObjectTask((JSDOMWindowBase*)thisObject, task));
}

void JSDOMWindowBase::willRemoveFromWindowShell()
{
    setCurrentEvent(0);
}

JSDOMWindowShell* JSDOMWindowBase::shell() const
{
    return m_shell;
}

VM& JSDOMWindowBase::commonVM()
{
    ASSERT(isMainThread());

    static VM* vm = nullptr;
    if (!vm) {
        ScriptController::initializeThreading();
        vm = &VM::createLeaked(LargeHeap).leakRef();
#if !PLATFORM(IOS)
        vm->setExclusiveThread(std::this_thread::get_id());
#else
        vm->heap.setFullActivityCallback(WebSafeFullGCActivityCallback::create(&vm->heap));
        vm->heap.setEdenActivityCallback(WebSafeEdenGCActivityCallback::create(&vm->heap));

        vm->heap.setIncrementalSweeper(std::make_unique<WebSafeIncrementalSweeper>(&vm->heap));
        vm->heap.machineThreads().addCurrentThread();
#endif

#if PLATFORM(MAC)
        if (applicationIsITunes() || applicationIsIBooks() || Settings::shouldRewriteConstAsVar())
            vm->setShouldRewriteConstAsVar(true);
#endif

        initNormalWorldClientData(vm);
    }

    return *vm;
}

// JSDOMGlobalObject* is ignored, accessing a window in any context will
// use that DOMWindow's prototype chain.
JSValue toJS(ExecState* exec, JSDOMGlobalObject*, DOMWindow* domWindow)
{
    return toJS(exec, domWindow);
}

JSValue toJS(ExecState* exec, DOMWindow* domWindow)
{
    if (!domWindow)
        return jsNull();
    Frame* frame = domWindow->frame();
    if (!frame)
        return jsNull();
    return frame->script().windowShell(currentWorld(exec));
}

JSDOMWindow* toJSDOMWindow(Frame* frame, DOMWrapperWorld& world)
{
    if (!frame)
        return 0;
    return frame->script().windowShell(world)->window();
}

JSDOMWindow* toJSDOMWindow(JSValue value)
{
    if (!value.isObject())
        return 0;
    while (!value.isNull()) {
        JSObject* object = asObject(value);
        const ClassInfo* classInfo = object->classInfo();
        if (classInfo == JSDOMWindow::info())
            return jsCast<JSDOMWindow*>(object);
        if (classInfo == JSDOMWindowShell::info())
            return jsCast<JSDOMWindowShell*>(object)->window();
        value = object->prototype();
    }
    return 0;
}

void JSDOMWindowBase::fireFrameClearedWatchpointsForWindow(DOMWindow* window)
{
    JSC::VM& vm = JSDOMWindowBase::commonVM();
    JSVMClientData* clientData = static_cast<JSVMClientData*>(vm.clientData);
    Vector<Ref<DOMWrapperWorld>> wrapperWorlds;
    clientData->getAllWorlds(wrapperWorlds);
    for (unsigned i = 0; i < wrapperWorlds.size(); ++i) {
        DOMObjectWrapperMap& wrappers = wrapperWorlds[i]->m_wrappers;
        auto result = wrappers.find(window);
        if (result == wrappers.end())
            continue;
        JSC::JSObject* wrapper = result->value.get();
        if (!wrapper)
            continue;
        JSDOMWindowBase* jsWindow = JSC::jsCast<JSDOMWindowBase*>(wrapper);
        jsWindow->m_windowCloseWatchpoints.fireAll("Frame cleared");
    }
}


JSC::JSInternalPromise* JSDOMWindowBase::moduleLoaderResolve(JSC::JSGlobalObject* globalObject, JSC::ExecState* exec, JSC::JSValue moduleName, JSC::JSValue importerModuleKey)
{
    JSDOMWindowBase* thisObject = JSC::jsCast<JSDOMWindowBase*>(globalObject);
    if (RefPtr<Document> document = thisObject->wrapped().document())
        return document->moduleLoader()->resolve(globalObject, exec, moduleName, importerModuleKey);
    JSC::JSInternalPromiseDeferred* deferred = JSC::JSInternalPromiseDeferred::create(exec, globalObject);
    return deferred->reject(exec, jsUndefined());
}

JSC::JSInternalPromise* JSDOMWindowBase::moduleLoaderFetch(JSC::JSGlobalObject* globalObject, JSC::ExecState* exec, JSC::JSValue moduleKey)
{
    JSDOMWindowBase* thisObject = JSC::jsCast<JSDOMWindowBase*>(globalObject);
    if (RefPtr<Document> document = thisObject->wrapped().document())
        return document->moduleLoader()->fetch(globalObject, exec, moduleKey);
    JSC::JSInternalPromiseDeferred* deferred = JSC::JSInternalPromiseDeferred::create(exec, globalObject);
    return deferred->reject(exec, jsUndefined());
}

JSC::JSValue JSDOMWindowBase::moduleLoaderEvaluate(JSC::JSGlobalObject* globalObject, JSC::ExecState* exec, JSC::JSValue moduleKey, JSC::JSValue moduleRecord)
{
    JSDOMWindowBase* thisObject = JSC::jsCast<JSDOMWindowBase*>(globalObject);
    if (RefPtr<Document> document = thisObject->wrapped().document())
        return document->moduleLoader()->evaluate(globalObject, exec, moduleKey, moduleRecord);
    return JSC::jsUndefined();
}

} // namespace WebCore
