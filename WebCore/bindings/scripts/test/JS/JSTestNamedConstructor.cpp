/*
    This file is part of the WebKit open source project.
    This file has been generated by generate-bindings.pl. DO NOT MODIFY!

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "JSTestNamedConstructor.h"

#include "ExceptionCode.h"
#include "JSDOMBinding.h"
#include "JSDOMConstructor.h"
#include <runtime/Error.h>
#include <wtf/GetPtr.h>

using namespace JSC;

namespace WebCore {

// Attributes

JSC::EncodedJSValue jsTestNamedConstructorConstructor(JSC::ExecState*, JSC::JSObject*, JSC::EncodedJSValue, JSC::PropertyName);

class JSTestNamedConstructorPrototype : public JSC::JSNonFinalObject {
public:
    typedef JSC::JSNonFinalObject Base;
    static JSTestNamedConstructorPrototype* create(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::Structure* structure)
    {
        JSTestNamedConstructorPrototype* ptr = new (NotNull, JSC::allocateCell<JSTestNamedConstructorPrototype>(vm.heap)) JSTestNamedConstructorPrototype(vm, globalObject, structure);
        ptr->finishCreation(vm);
        return ptr;
    }

    DECLARE_INFO;
    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
    }

private:
    JSTestNamedConstructorPrototype(JSC::VM& vm, JSC::JSGlobalObject*, JSC::Structure* structure)
        : JSC::JSNonFinalObject(vm, structure)
    {
    }

    void finishCreation(JSC::VM&);
};

typedef JSDOMConstructorNotConstructable<JSTestNamedConstructor> JSTestNamedConstructorConstructor;
typedef JSDOMNamedConstructor<JSTestNamedConstructor> JSTestNamedConstructorNamedConstructor;

template<> void JSTestNamedConstructorConstructor::initializeProperties(VM& vm, JSDOMGlobalObject& globalObject)
{
    putDirect(vm, vm.propertyNames->prototype, JSTestNamedConstructor::getPrototype(vm, &globalObject), DontDelete | ReadOnly | DontEnum);
    putDirect(vm, vm.propertyNames->name, jsNontrivialString(&vm, String(ASCIILiteral("TestNamedConstructor"))), ReadOnly | DontEnum);
    putDirect(vm, vm.propertyNames->length, jsNumber(0), ReadOnly | DontEnum);
}

template<> const ClassInfo JSTestNamedConstructorConstructor::s_info = { "TestNamedConstructorConstructor", &Base::s_info, 0, CREATE_METHOD_TABLE(JSTestNamedConstructorConstructor) };

template<> EncodedJSValue JSC_HOST_CALL JSTestNamedConstructorNamedConstructor::construct(ExecState* state)
{
    auto* castedThis = jsCast<JSTestNamedConstructorNamedConstructor*>(state->callee());
    if (UNLIKELY(state->argumentCount() < 1))
        return throwVMError(state, createNotEnoughArgumentsError(state));
    ExceptionCode ec = 0;
    String str1 = state->argument(0).toString(state)->value(state);
    if (UNLIKELY(state->hadException()))
        return JSValue::encode(jsUndefined());
    String str2 = state->argument(1).toString(state)->value(state);
    if (UNLIKELY(state->hadException()))
        return JSValue::encode(jsUndefined());
    String str3 = state->argument(2).isUndefined() ? String() : state->uncheckedArgument(2).toString(state)->value(state);
    if (UNLIKELY(state->hadException()))
        return JSValue::encode(jsUndefined());
    RefPtr<TestNamedConstructor> object = TestNamedConstructor::createForJSConstructor(*castedThis->document(), str1, str2, str3, ec);
    if (ec) {
        setDOMException(state, ec);
        return JSValue::encode(JSValue());
    }
    return JSValue::encode(asObject(toJS(state, castedThis->globalObject(), object.get())));
}

template<> void JSTestNamedConstructorNamedConstructor::initializeProperties(VM& vm, JSDOMGlobalObject& globalObject)
{
    putDirect(vm, vm.propertyNames->prototype, JSTestNamedConstructor::getPrototype(vm, &globalObject), DontDelete | ReadOnly | DontEnum);
    putDirect(vm, vm.propertyNames->name, jsNontrivialString(&vm, String(ASCIILiteral("Audio"))), ReadOnly | DontEnum);
    putDirect(vm, vm.propertyNames->length, jsNumber(0), ReadOnly | DontEnum);
}

template<> const ClassInfo JSTestNamedConstructorNamedConstructor::s_info = { "AudioConstructor", &Base::s_info, 0, CREATE_METHOD_TABLE(JSTestNamedConstructorNamedConstructor) };

/* Hash table for prototype */

static const HashTableValue JSTestNamedConstructorPrototypeTableValues[] =
{
    { "constructor", DontEnum | ReadOnly, NoIntrinsic, { (intptr_t)static_cast<PropertySlot::GetValueFunc>(jsTestNamedConstructorConstructor), (intptr_t) static_cast<PutPropertySlot::PutValueFunc>(0) } },
};

const ClassInfo JSTestNamedConstructorPrototype::s_info = { "TestNamedConstructorPrototype", &Base::s_info, 0, CREATE_METHOD_TABLE(JSTestNamedConstructorPrototype) };

void JSTestNamedConstructorPrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    reifyStaticProperties(vm, JSTestNamedConstructorPrototypeTableValues, *this);
}

const ClassInfo JSTestNamedConstructor::s_info = { "TestNamedConstructor", &Base::s_info, 0, CREATE_METHOD_TABLE(JSTestNamedConstructor) };

JSTestNamedConstructor::JSTestNamedConstructor(Structure* structure, JSDOMGlobalObject& globalObject, Ref<TestNamedConstructor>&& impl)
    : JSDOMWrapper<TestNamedConstructor>(structure, globalObject, WTF::move(impl))
{
}

JSObject* JSTestNamedConstructor::createPrototype(VM& vm, JSGlobalObject* globalObject)
{
    return JSTestNamedConstructorPrototype::create(vm, globalObject, JSTestNamedConstructorPrototype::createStructure(vm, globalObject, globalObject->objectPrototype()));
}

JSObject* JSTestNamedConstructor::getPrototype(VM& vm, JSGlobalObject* globalObject)
{
    return getDOMPrototype<JSTestNamedConstructor>(vm, globalObject);
}

void JSTestNamedConstructor::destroy(JSC::JSCell* cell)
{
    JSTestNamedConstructor* thisObject = static_cast<JSTestNamedConstructor*>(cell);
    thisObject->JSTestNamedConstructor::~JSTestNamedConstructor();
}

EncodedJSValue jsTestNamedConstructorConstructor(ExecState* state, JSObject* baseValue, EncodedJSValue, PropertyName)
{
    JSTestNamedConstructorPrototype* domObject = jsDynamicCast<JSTestNamedConstructorPrototype*>(baseValue);
    if (!domObject)
        return throwVMTypeError(state);
    return JSValue::encode(JSTestNamedConstructor::getConstructor(state->vm(), domObject->globalObject()));
}

JSValue JSTestNamedConstructor::getConstructor(VM& vm, JSGlobalObject* globalObject)
{
    return getDOMConstructor<JSTestNamedConstructorConstructor>(vm, *jsCast<JSDOMGlobalObject*>(globalObject));
}

JSValue JSTestNamedConstructor::getNamedConstructor(VM& vm, JSGlobalObject* globalObject)
{
    return getDOMConstructor<JSTestNamedConstructorNamedConstructor>(vm, *jsCast<JSDOMGlobalObject*>(globalObject));
}

bool JSTestNamedConstructorOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, SlotVisitor& visitor)
{
    auto* jsTestNamedConstructor = jsCast<JSTestNamedConstructor*>(handle.slot()->asCell());
    if (jsTestNamedConstructor->wrapped().hasPendingActivity())
        return true;
    UNUSED_PARAM(visitor);
    return false;
}

void JSTestNamedConstructorOwner::finalize(JSC::Handle<JSC::Unknown> handle, void* context)
{
    auto* jsTestNamedConstructor = jsCast<JSTestNamedConstructor*>(handle.slot()->asCell());
    auto& world = *static_cast<DOMWrapperWorld*>(context);
    uncacheWrapper(world, &jsTestNamedConstructor->wrapped(), jsTestNamedConstructor);
}

#if ENABLE(BINDING_INTEGRITY)
#if PLATFORM(WIN)
#pragma warning(disable: 4483)
extern "C" { extern void (*const __identifier("??_7TestNamedConstructor@WebCore@@6B@")[])(); }
#else
extern "C" { extern void* _ZTVN7WebCore20TestNamedConstructorE[]; }
#endif
#endif

JSC::JSValue toJSNewlyCreated(JSC::ExecState*, JSDOMGlobalObject* globalObject, TestNamedConstructor* impl)
{
    if (!impl)
        return jsNull();
    return createNewWrapper<JSTestNamedConstructor>(globalObject, impl);
}

JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject* globalObject, TestNamedConstructor* impl)
{
    if (!impl)
        return jsNull();
    if (JSValue result = getExistingWrapper<JSTestNamedConstructor>(globalObject, impl))
        return result;

#if ENABLE(BINDING_INTEGRITY)
    void* actualVTablePointer = *(reinterpret_cast<void**>(impl));
#if PLATFORM(WIN)
    void* expectedVTablePointer = reinterpret_cast<void*>(__identifier("??_7TestNamedConstructor@WebCore@@6B@"));
#else
    void* expectedVTablePointer = &_ZTVN7WebCore20TestNamedConstructorE[2];
#if COMPILER(CLANG)
    // If this fails TestNamedConstructor does not have a vtable, so you need to add the
    // ImplementationLacksVTable attribute to the interface definition
    COMPILE_ASSERT(__is_polymorphic(TestNamedConstructor), TestNamedConstructor_is_not_polymorphic);
#endif
#endif
    // If you hit this assertion you either have a use after free bug, or
    // TestNamedConstructor has subclasses. If TestNamedConstructor has subclasses that get passed
    // to toJS() we currently require TestNamedConstructor you to opt out of binding hardening
    // by adding the SkipVTableValidation attribute to the interface IDL definition
    RELEASE_ASSERT(actualVTablePointer == expectedVTablePointer);
#endif
    return createNewWrapper<JSTestNamedConstructor>(globalObject, impl);
}

TestNamedConstructor* JSTestNamedConstructor::toWrapped(JSC::JSValue value)
{
    if (auto* wrapper = jsDynamicCast<JSTestNamedConstructor*>(value))
        return &wrapper->wrapped();
    return nullptr;
}

}
