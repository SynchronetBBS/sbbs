/* js_shims.hpp - SM128 compatibility shims for mozjs185 API
 *
 * This file provides type aliases, macros, and inline function overloads
 * that allow code written against the mozjs185 (SpiderMonkey 1.8.5) API
 * to compile against SpiderMonkey 128 with minimal changes.
 *
 * Prerequisites: SM128 headers (jsapi.h, js/Context.h, etc.) must be included
 * before this file.
 */

#ifndef JS_SHIMS_HPP
#define JS_SHIMS_HPP

/* JS_DestroyScript: removed -- SM128 GC handles script lifetime */

/* ================================================================
 * SM128 compatibility shims
 * (mozjs185 types/macros that no longer exist in SpiderMonkey 128)
 * ================================================================ */

/* Primitive type aliases */
typedef JS::Value jsval;
typedef bool JSBool;
typedef int32_t int32;
typedef unsigned int uintN;

/* Boolean literals */
#define JS_TRUE  true
#define JS_FALSE false

/* jsval constructors */
#define JSVAL_VOID              JS::UndefinedValue()
#define JSVAL_NULL              JS::NullValue()
#define JSVAL_TRUE              JS::TrueValue()
#define JSVAL_FALSE             JS::FalseValue()
#define INT_TO_JSVAL(i)         JS::Int32Value(static_cast<int32_t>(i))
#define BOOLEAN_TO_JSVAL(b)     JS::BooleanValue(b)
#define DOUBLE_TO_JSVAL(d)      JS::DoubleValue(d)
#define STRING_TO_JSVAL(s)      JS::StringValue(s)
#define OBJECT_TO_JSVAL(o)      ((o) ? JS::ObjectValue(*(o)) : JS::NullValue())

/* jsval destructors / type checks */
#define JSVAL_TO_STRING(v)      ((v).toString())
#define JSVAL_TO_INT(v)         ((v).toInt32())
#define JSVAL_TO_BOOLEAN(v)     ((v).toBoolean())
#define JSVAL_TO_DOUBLE(v)      ((v).toNumber())
#define JSVAL_IS_VOID(v)        ((v).isUndefined())
#define JSVAL_IS_NULL(v)        ((v).isNull())
#define JSVAL_IS_BOOLEAN(v)     ((v).isBoolean())
#define JSVAL_IS_NUMBER(v)      ((v).isNumber())
#define JSVAL_IS_STRING(v)      ((v).isString())
/* JSVAL_IS_OBJECT / JSVAL_TO_OBJECT: defined below with null-inclusive semantics */
#define JSVAL_IS_INT(v)         ((v).isInt32())

/* Native function call args / return value helpers */
#define JS_ARGV(cx, vp)         ((vp) + 2)
#define JS_SET_RVAL(cx, vp, v)  do { (vp)[0] = (v); } while(0)
#define JS_RVAL(cx, vp)         ((vp)[0])
#define JS_THIS(cx, vp)         ((vp)[1])
/* JS_THIS_OBJECT: get 'this' as JSObject* from old-style native call.
 * SM128 native functions receive undefined 'this' in plain calls even in sloppy mode;
 * fall back to the current global (mimicking SM185 sloppy-mode 'this' coercion). */
#define JS_THIS_OBJECT(cx, vp)  (((vp)[1]).isObject() ? &(vp)[1].toObject() : JS::CurrentGlobalOrNull(cx))

/* JSType: JSTYPE_VOID was renamed to JSTYPE_UNDEFINED in SM128 */
#define JSTYPE_VOID             JSTYPE_UNDEFINED

/* JS_GetTypeName: replaced by JSTypeToString() */
#define JS_GetTypeName(cx, t)   JSTypeToString(t)

/* JS_NewArrayObject: use JS::NewArrayObject in SM128 (ignore old argc/argv) */
#define JS_NewArrayObject(cx, n, v)  JS::NewArrayObject((cx), static_cast<size_t>(n))

/* JS_ReportError -> JS_ReportErrorASCII */
#define JS_ReportError  JS_ReportErrorASCII

/* Operation callback -> interrupt callback.
 * SM128 uses an ADD-only list; use JS_AddInterruptCallback for initial
 * registration, then JS_SetOperationCallback only to disable/re-enable. */
#define JS_SetOperationCallback(cx, fn) \
    do { \
        if ((fn) == nullptr) \
            JS_DisableInterruptCallback(cx); \
        else \
            JS_ResetInterruptCallback((cx), true); \
    } while (0)

/* Value-to-type conversions: const JS::Value& accepts both raw argv[] slots
 * and JS::RootedValue (via its operator const T&()).  We use fromMarkedLocation
 * to tell the GC tracer where the value lives. */
static inline JSString* JS_ValueToString(JSContext* cx, const JS::Value& v) {
    return JS::ToString(cx, JS::Handle<JS::Value>::fromMarkedLocation(&v));
}
static inline bool JS_ValueToInt32(JSContext* cx, const JS::Value& v, int32_t* ip) {
    JS::RootedValue rv(cx, v);
    return JS::ToInt32(cx, rv, ip);
}
/* JSVAL_ZERO: convenience macro removed in SM128 */
#define JSVAL_ZERO (JS::Int32Value(0))
#define JSVAL_ONE  (JS::Int32Value(1))

/* jsint / jsuint / jsdouble / uint32: removed in SM128 */
typedef int jsint;
typedef unsigned jsuint;
typedef double jsdouble;
typedef uint32_t uint32;

/* JS_ValueToNumber: use JS::ToNumber in SM128 */
static inline bool JS_ValueToNumber(JSContext* cx, jsval v, double* dp) {
    JS::RootedValue rv(cx, v);
    return JS::ToNumber(cx, rv, dp);
}

/* JS_ValueToECMAUint32: use JS::ToUint32 in SM128 */
static inline bool JS_ValueToECMAUint32(JSContext* cx, jsval v, uint32_t* ip) {
    JS::RootedValue rv(cx, v);
    return JS::ToUint32(cx, rv, ip);
}
/* JS_ValueToECMAInt32: use JS::ToInt32 in SM128 */
static inline bool JS_ValueToECMAInt32(JSContext* cx, jsval v, int32_t* ip) {
    JS::RootedValue rv(cx, v);
    return JS::ToInt32(cx, rv, ip);
}

/* JS_GetScopeChain: gone in SM128; return current global as fallback */
static inline JSObject* JS_GetScopeChain(JSContext* cx) {
    return JS::CurrentGlobalOrNull(cx);
}

/* JS_IdToValue: SM128 takes jsid by value and MutableHandle<Value>; shim for jsval* callers */
static inline bool JS_IdToValue(JSContext* cx, jsid id, jsval* vp) {
    JS::RootedValue rv(cx);
    bool ok = JS_IdToValue(cx, id, &rv);
    *vp = rv;
    return ok;
}

/* JS_ValueToObject: SM128 uses Handle<Value>/MutableHandle<JSObject*> */
static inline bool JS_ValueToObject(JSContext* cx, jsval val, JSObject** objp) {
    JS::RootedValue rv(cx, val);
    JS::RootedObject robj(cx);
    bool ok = JS_ValueToObject(cx, rv, &robj);
    *objp = robj;
    return ok;
}

/* Private data via reserved slot 0 (replacement for JSCLASS_HAS_PRIVATE /
 * JS_GetPrivate / JS_SetPrivate which are gone in SM128) */
#define JSCLASS_HAS_PRIVATE  JSCLASS_HAS_RESERVED_SLOTS(1)
static inline void* JS_GetPrivate(JSObject* obj) {
    return JS::GetMaybePtrFromReservedSlot<void>(obj, 0);
}
static inline void* JS_GetPrivate(JSContext*, JSObject* obj) {
    return JS_GetPrivate(obj);
}
static inline void JS_SetPrivate(JSObject* obj, void* data) {
    JS_SetReservedSlot(obj, 0, data ? JS::PrivateValue(data) : JS::UndefinedValue());
}
static inline bool JS_SetPrivate(JSContext*, JSObject* obj, void* data) {
    JS_SetPrivate(obj, data);
    return true;
}

/* JS_GetClass: SM128 uses JS::GetClass(obj) with no cx */
static inline const JSClass* JS_GetClass(JSContext*, JSObject* obj) {
    return JS::GetClass(obj);
}

/* JS_IsArrayObject: SM128 uses JS::IsArrayObject with bool* out param */
static inline bool JS_IsArrayObject(JSContext* cx, JSObject* obj) {
    bool result = false;
    JS::RootedObject robj(cx, obj);
    JS::IsArrayObject(cx, robj, &result);
    return result;
}

/* JS_GetArrayLength: SM128 uses JS::GetArrayLength with Handle */
static inline bool JS_GetArrayLength(JSContext* cx, JSObject* obj, unsigned* lenp) {
    JS::RootedObject robj(cx, obj);
    return JS::GetArrayLength(cx, robj, lenp);
}

/* JS_ValueToBoolean: SM128 uses JS::ToBoolean(HandleValue) -> bool directly */
static inline bool JS_ValueToBoolean(JSContext*, const JS::Value& v, bool* bp) {
    *bp = JS::ToBoolean(JS::Handle<JS::Value>::fromMarkedLocation(&v));
    return true;
}
/* Overload for int* (e.g. ciolib int flags) */
static inline bool JS_ValueToBoolean(JSContext* cx, const JS::Value& v, int* ip) {
    bool b = false;
    bool ok = JS_ValueToBoolean(cx, v, &b);
    *ip = b ? 1 : 0;
    return ok;
}

/* JS_GetElement/JS_SetElement: SM128 uses Handle<JSObject*> and Handle/MutableHandle<Value>.
 * These shims accept the old JSObject-ptr/jsval-ptr style. */
static inline bool JS_GetElement(JSContext* cx, JSObject* obj, uint32_t index, jsval* vp) {
    JS::RootedObject robj(cx, obj);
    JS::RootedValue rv(cx);
    bool ok = JS_GetElement(cx, robj, index, &rv);
    *vp = rv;
    return ok;
}
static inline bool JS_SetElement(JSContext* cx, JSObject* obj, uint32_t index, const jsval* vp) {
    JS::RootedObject robj(cx, obj);
    JS::RootedValue rv(cx, *vp);
    return JS_SetElement(cx, robj, index, rv);
}

/* JSVAL_TO_OBJECT: SM128 uses v.toObjectOrNull() (null-inclusive) */
#define JSVAL_TO_OBJECT(v)   ((v).toObjectOrNull())
/* JSVAL_IS_OBJECT: SM128 uses v.isObjectOrNull() (null-inclusive) */
#define JSVAL_IS_OBJECT(v)   ((v).isObjectOrNull())
/* UINT_TO_JSVAL: SM128 uses JS::NumberValue */
#define UINT_TO_JSVAL(u)     JS::NumberValue(static_cast<double>(u))

/* ---- Object/property API shims (JSObject* -> Handle<JSObject*>) ----
 *
 * Each shim roots the raw JSObject* and uses explicit Handle<> typed locals
 * for the inner call to avoid ambiguity with our own overloads (RootedObject
 * has operator JSObject*() which would otherwise cause recursion).
 * -------------------------------------------------------------------- */

/* JSPropertyOp / JSStrictPropertyOp: removed in SM128; used by JS_DefinePropertyWithTinyId shim */
typedef bool (*JSPropertyOp)(JSContext*, JS::Handle<JSObject*>, JS::Handle<jsid>, JS::MutableHandle<JS::Value>);
typedef bool (*JSStrictPropertyOp)(JSContext*, JS::Handle<JSObject*>, JS::Handle<jsid>, bool, JS::MutableHandle<JS::Value>);

/* JS_GetProperty */
static inline bool JS_GetProperty(JSContext* cx, JSObject* obj, const char* name, jsval* vp) {
    JS::RootedObject robj(cx, obj);
    JS::HandleObject hobj = robj;
    JS::RootedValue rv(cx);
    bool ok = JS_GetProperty(cx, hobj, name, &rv);
    *vp = rv;
    return ok;
}

/* JS_SetProperty: accept jsval by const-ref (covers both lvalue and rvalue/temp) */
static inline bool JS_SetProperty(JSContext* cx, JSObject* obj, const char* name, const jsval& v) {
    JS::RootedObject robj(cx, obj);
    JS::HandleObject hobj = robj;
    JS::RootedValue rv(cx, v);
    JS::HandleValue hval = rv;
    return JS_SetProperty(cx, hobj, name, hval);
}
/* Also accept jsval* (old code sometimes takes address of a local) */
static inline bool JS_SetProperty(JSContext* cx, JSObject* obj, const char* name, const jsval* vp) {
    return JS_SetProperty(cx, obj, name, *vp);
}

/* JS_HasProperty */
static inline bool JS_HasProperty(JSContext* cx, JSObject* obj, const char* name, bool* foundp) {
    JS::RootedObject robj(cx, obj);
    JS::HandleObject hobj = robj;
    return JS_HasProperty(cx, hobj, name, foundp);
}

/* JS_HasOwnProperty */
static inline bool JS_HasOwnProperty(JSContext* cx, JSObject* obj, const char* name, bool* foundp) {
    JS::RootedObject robj(cx, obj);
    JS::HandleObject hobj = robj;
    return JS_HasOwnProperty(cx, hobj, name, foundp);
}

/* JS_DefineProperty: 7-arg form (cx,obj,name,val,getter,setter,attrs).
 * Template getter/setter resolves NULL ambiguity; getter/setter dropped for
 * value properties (old JSPropertyOp type incompatible with SM128 anyway). */
template<typename G, typename S>
static inline bool JS_DefineProperty(JSContext* cx, JSObject* obj, const char* name,
                                      jsval value, G /*getter*/, S /*setter*/,
                                      unsigned attrs) {
    JS::RootedObject robj(cx, obj);
    JS::HandleObject hobj = robj;
    JS::RootedValue rv(cx, value);
    JS::HandleValue hval = rv;
    return JS_DefineProperty(cx, hobj, name, hval, attrs);
}

/* JS_DefinePropertyWithTinyId: tinyid mechanism gone in SM128; plain property */
static inline bool JS_DefinePropertyWithTinyId(JSContext* cx, JSObject* obj, const char* name,
                                                int8_t /*tinyid*/, jsval value,
                                                JSPropertyOp /*getter*/, JSStrictPropertyOp /*setter*/,
                                                unsigned attrs) {
    JS::RootedObject robj(cx, obj);
    JS::RootedValue rv(cx, value);
    return JS_DefineProperty(cx, static_cast<JS::HandleObject>(robj), name,
                              static_cast<JS::HandleValue>(rv), attrs);
}

/* JS_DefineObject: old 6-arg form (with proto) */
static inline JSObject* JS_DefineObject(JSContext* cx, JSObject* obj, const char* name,
                                         const JSClass* clasp, JSObject* /*proto*/,
                                         unsigned attrs) {
    JS::RootedObject robj(cx, obj);
    JS::HandleObject hobj = robj;
    return JS_DefineObject(cx, hobj, name, clasp, attrs);
}

/* JS_DefineProperties */
static inline bool JS_DefineProperties(JSContext* cx, JSObject* obj, const JSPropertySpec* ps) {
    JS::RootedObject robj(cx, obj);
    JS::HandleObject hobj = robj;
    return JS_DefineProperties(cx, hobj, ps);
}
/* jsSyncPropertySpec overload: defined after jsSyncPropertySpec typedef below */

/* JS_NewObject: old 4-arg form (cx, clasp, proto, parent); proto/parent dropped */
static inline JSObject* JS_NewObject(JSContext* cx, const JSClass* clasp,
                                      JSObject* /*proto*/, JSObject* /*parent*/) {
    return JS_NewObject(cx, clasp);
}

/* JS_InitClass: old 10-arg form; SM128 adds protoClass param */
static inline JSObject* JS_InitClass(JSContext* cx, JSObject* obj, JSObject* parent_proto,
                                      const JSClass* clasp, JSNative constructor, unsigned nargs,
                                      const JSPropertySpec* ps, const JSFunctionSpec* fs,
                                      const JSPropertySpec* static_ps, const JSFunctionSpec* static_fs) {
    JS::RootedObject robj(cx, obj);
    JS::HandleObject hobj = robj;
    JS::RootedObject rproto(cx, parent_proto);
    JS::HandleObject hproto = rproto;
    return JS_InitClass(cx, hobj, nullptr, hproto, clasp->name, constructor, nargs,
                        ps, fs, static_ps, static_fs);
}
/* JS_GetGlobalObject: removed in SM128 */
static inline JSObject* JS_GetGlobalObject(JSContext* cx) {
    return JS::CurrentGlobalOrNull(cx);
}

/* JS_NewContext: SM128 signature is (uint32_t maxbytes, JSRuntime* = nullptr).
 * Callers passing old (JSRuntime*, stacksize) form need to be updated
 * to use jsrt_GetNew() or JS_NewContext(JAVASCRIPT_MAX_BYTES) directly. */

/* JS_DeleteProperty: 3-arg form (no ObjectOpResult) */
static inline bool JS_DeleteProperty(JSContext* cx, JSObject* obj, const char* name) {
    JS::RootedObject robj(cx, obj);
    JS::HandleObject hobj = robj;
    return JS_DeleteProperty(cx, hobj, name);
}

/* JS_CallFunctionName / JS_CallFunctionValue / JS_CallFunction: SM128 uses
 * Handle<JSObject*>, HandleValueArray, MutableHandle<Value> signatures */
static inline bool JS_CallFunctionName(JSContext* cx, JSObject* obj, const char* name,
        uintN argc, jsval* argv, jsval* rval) {
    JS::RootedObject robj(cx, obj); JS::HandleObject hobj = robj;
    JS::RootedValueVector args(cx);
    for (uintN i = 0; i < argc; i++) { if (!args.append(argv[i])) return false; }
    JS::RootedValue rv(cx);
    bool ok = JS_CallFunctionName(cx, hobj, name, args, &rv);
    if (rval) *rval = rv;
    return ok;
}
static inline bool JS_CallFunctionValue(JSContext* cx, JSObject* obj, jsval fun,
        uintN argc, jsval* argv, jsval* rval) {
    JS::RootedObject robj(cx, obj); JS::HandleObject hobj = robj;
    JS::RootedValue rfun(cx, fun); JS::HandleValue hfun = rfun;
    JS::RootedValueVector args(cx);
    for (uintN i = 0; i < argc; i++) { if (!args.append(argv[i])) return false; }
    JS::RootedValue rv(cx);
    bool ok = JS::Call(cx, hobj, hfun, args, &rv);
    if (rval) *rval = rv;
    return ok;
}
static inline bool JS_CallFunction(JSContext* cx, JSObject* obj, JSFunction* fun,
        uintN argc, jsval* argv, jsval* rval) {
    JS::RootedObject robj(cx, obj); JS::HandleObject hobj = robj;
    JS::RootedFunction rfun(cx, fun);
    JS::RootedValueVector args(cx);
    for (uintN i = 0; i < argc; i++) { if (!args.append(argv[i])) return false; }
    JS::RootedValue rv(cx);
    bool ok = JS_CallFunction(cx, hobj, rfun, args, &rv);
    if (rval) *rval = rv;
    return ok;
}

/* JSVAL_IS_DOUBLE: gone in SM128; JS::Value has isDouble() method */
#define JSVAL_IS_DOUBLE(v)  ((v).isDouble())

/* JS_GetStringCharsAndLength / JS_GetStringCharsZ: gone in SM128.
 * Use JS::StableStringChars in production code; for migration, these stubs
 * use GetTwoByteStringCharsAndLength which requires a NoGC context.
 * Files using these APIs should be fixed to use StableStringChars directly. */

/* JS_WriteStructuredClone / JS_ReadStructuredClone: use SM128 API from <js/StructuredClone.h> */
#include <js/StructuredClone.h>
/* Structured clone version constant for JS_ReadStructuredClone compat */

/* JS_GetParent: gone in SM128; return global as fallback */
static inline JSObject* JS_GetParent(JSContext* cx, JSObject* obj) {
    (void)obj; return JS::CurrentGlobalOrNull(cx);
}

/* JS_CompileScript: gone; use JS::Compile or CompileUtf8 */
static inline JSScript* JS_CompileScript(JSContext* cx, JSObject* /*obj*/,
        const char* src, size_t len, const char* filename, unsigned lineno) {
    JS::CompileOptions opts(cx);
    opts.setFileAndLine(filename ? filename : "<eval>", lineno > 0 ? lineno : 1);
    JS::SourceText<mozilla::Utf8Unit> source;
    if (!source.init(cx, src, len, JS::SourceOwnership::Borrowed))
        return nullptr;
    return JS::Compile(cx, opts, source);
}

/* JS_FlattenString: gone in SM128; no-op (SM128 strings are already flat) */
static inline JSString* JS_FlattenString(JSContext* cx, JSString* str) {
    (void)cx; return str;
}

/* JS_NewNumberValue: gone in SM128; just assign the double value */
static inline bool JS_NewNumberValue(JSContext* cx, double d, jsval* rval) {
    (void)cx; *rval = JS::NumberValue(d); return true;
}

/* JS_GetPendingException: shim for jsval* callers */
static inline bool JS_GetPendingException(JSContext* cx, jsval* vp) {
    JS::RootedValue rv(cx);
    bool ok = JS_GetPendingException(cx, &rv);
    *vp = rv;
    return ok;
}

/* JS_ObjectIsFunction: SM128 - JSFunction* is separate from JSObject*; shim for raw ptr */
static inline bool JS_ObjectIsFunction(JSContext* cx, JSObject* obj) {
    (void)cx;
    return JS_ObjectIsFunction(obj);
}

/* JS_NewObjectWithGivenProto: SM128 drops parent (3rd) arg; shim for 4-arg callers */
static inline JSObject* JS_NewObjectWithGivenProto(JSContext* cx, const JSClass* clasp,
        JSObject* proto, JSObject* /*parent*/) {
    JS::RootedObject rproto(cx, proto);
    return JS_NewObjectWithGivenProto(cx, clasp, rproto);
}

/* JS_SetRuntimePrivate / JS_GetRuntimePrivate: gone in SM128.
 * Emulated via jsrt_*RuntimePrivate() in js_rtpool.cpp (single shared map). */
#include "js_rtpool.h"
static inline void JS_SetRuntimePrivate(JSRuntime* rt, void* data) { jsrt_SetRuntimePrivate(rt, data); }
static inline void* JS_GetRuntimePrivate(JSRuntime* rt) { return jsrt_GetRuntimePrivate(rt); }

/* JS_DefineElement: old form with getter/setter (tinyid mechanism) - drop them */
template<typename G, typename S>
static inline bool JS_DefineElement(JSContext* cx, JSObject* obj, uint32_t index,
        jsval value, G, S, unsigned attrs) {
    JS::RootedObject robj(cx, obj); JS::HandleObject hobj = robj;
    JS::RootedValue rv(cx, value);
    return JS_DefineElement(cx, hobj, index, rv, attrs);
}

/* JS_DefineFunction: SM128 takes Handle<JSObject*>; shim for raw JSObject* callers */
static inline JSFunction* JS_DefineFunction(JSContext* cx, JSObject* obj, const char* name,
        JSNative call, unsigned nargs, unsigned attrs) {
    JS::RootedObject robj(cx, obj); JS::HandleObject hobj = robj;
    return JS_DefineFunction(cx, hobj, name, call, nargs, attrs);
}

/* JS_DefineFunctions: SM128 takes Handle<JSObject*>; shim for raw JSObject* callers */
static inline bool JS_DefineFunctions(JSContext* cx, JSObject* obj, const JSFunctionSpec* fs) {
    JS::RootedObject robj(cx, obj); JS::HandleObject hobj = robj;
    return JS_DefineFunctions(cx, hobj, fs);
}

/* JS_NewCompartmentAndGlobalObject: gone; use JS_NewGlobalObject.
 * After creating the global, enter its realm permanently (for the context's
 * lifetime). Old SBBS code expects the context to remain in the global's realm
 * after this call, since there were no realms in pre-SM52. */
#include <js/Realm.h>
static inline JSObject* JS_NewCompartmentAndGlobalObject(JSContext* cx,
        const JSClass* clasp, JSObject* /*proto*/) {
    JS::RealmOptions options;
    JSObject* glob = JS_NewGlobalObject(cx, clasp, nullptr, JS::FireOnNewGlobalHook, options);
    if (glob) {
        /* Enter the new global's realm; the previous realm is discarded since
         * this is a new context with no prior realm. */
        JS::EnterRealm(cx, glob);
    }
    return glob;
}

/* JS_InitStandardClasses: gone; use JS::InitRealmStandardClasses */
static inline bool JS_InitStandardClasses(JSContext* cx, JSObject* /*glob*/) {
    return JS::InitRealmStandardClasses(cx);
}

/* JS_SetReservedSlot: SM128 returns void, old API returned bool via cx-first 4-arg form */
static inline bool JS_SetReservedSlot(JSContext* /*cx*/, JSObject* obj, uint32_t slot, jsval v) {
    ::JS_SetReservedSlot(obj, slot, v); return true;
}

/* JSIdArray / JS_Enumerate / JS_DestroyIdArray: replaced by JS::IdVector in SM128 */
struct JSIdArray {
    int    length;
    jsid*  vector;
};
static inline JSIdArray* JS_Enumerate(JSContext* cx, JSObject* obj) {
    JS::RootedObject robj(cx, obj); JS::HandleObject hobj = robj;
    JS::Rooted<JS::IdVector> ids(cx, JS::IdVector(cx));
    if (!JS_Enumerate(cx, hobj, &ids))
        return nullptr;
    int len = (int)ids.get().length();
    JSIdArray* arr = (JSIdArray*)malloc(sizeof(JSIdArray) + len * sizeof(jsid));
    if (!arr) return nullptr;
    arr->length = len;
    arr->vector = (jsid*)((char*)arr + sizeof(JSIdArray));
    for (int i = 0; i < len; i++)
        arr->vector[i] = ids.get()[i];
    return arr;
}
static inline void JS_DestroyIdArray(JSContext* cx, JSIdArray* arr) {
    (void)cx; free(arr);
}

/* JS_InstanceOf: SM128 takes Handle<JSObject*>; shim for raw JSObject* callers */
static inline bool JS_InstanceOf(JSContext* cx, JSObject* obj, const JSClass* clasp, JS::CallArgs* args) {
    JS::RootedObject robj(cx, obj);
    JS::HandleObject hobj = robj;
    return JS_InstanceOf(cx, hobj, clasp, args);
}

/* JS_GetInstancePrivate: gone in SM128; check class then get private */
static inline void* JS_GetInstancePrivate(JSContext* cx, JSObject* obj, const JSClass* clasp, JS::CallArgs* args) {
    JS::RootedObject robj(cx, obj);
    JS::HandleObject hobj = robj;
    if (!JS_InstanceOf(cx, hobj, clasp, args))
        return nullptr;
    return JS_GetPrivate(obj);
}

/* JS_DeepFreezeObject / JS_FreezeObject: SM128 takes Handle<JSObject*> */
static inline bool JS_DeepFreezeObject(JSContext* cx, JSObject* obj) {
    JS::RootedObject robj(cx, obj); JS::HandleObject hobj = robj;
    return JS_DeepFreezeObject(cx, hobj);
}
static inline bool JS_FreezeObject(JSContext* cx, JSObject* obj) {
    JS::RootedObject robj(cx, obj); JS::HandleObject hobj = robj;
    return JS_FreezeObject(cx, hobj);
}

/* Date object shims: SM128 uses JS::date::NewDateObject and JS::date::ObjectIsDate */
static inline JSObject* JS_NewDateObjectMsec(JSContext* cx, double msec) {
    return JS::NewDateObject(cx, JS::TimeClip(msec));
}
static inline bool JS_ObjectIsDate(JSContext* cx, JSObject* obj) {
    JS::RootedObject robj(cx, obj);
    bool result = false;
    JS::ObjectIsDate(cx, robj, &result);
    return result;
}

/* JS_ValueToFunction: SM128 takes HandleValue; shim for jsval callers */
static inline JSFunction* JS_ValueToFunction(JSContext* cx, jsval v) {
    JS::RootedValue rv(cx, v);
    JS::HandleValue hv = rv;
    return JS_ValueToFunction(cx, hv);
}

/* JS_DestroyScript: already defined as no-op macro above (line 120) */

/* JS_ClearScope: removed; no direct equivalent */
static inline void JS_ClearScope(JSContext* cx, JSObject* obj) {
    (void)cx; (void)obj;
}

/* JS_CompileFile: SM128 uses JS::CompileUtf8Path.
 * Returns JSScript* cast to JSObject* for source compat; callers should
 * change their variable type to JSScript* in a proper SM128 port. */
static inline JSScript* JS_CompileFile(JSContext* cx, JSObject* /*scope*/, const char* path) {
    JS::CompileOptions opts(cx);
    opts.setFileAndLine(path, 1);
    return JS::CompileUtf8Path(cx, opts, path);
}

/* JS_ExecuteScript: old 4-arg form (cx, scope_obj, script, rval).
 * SM128 no longer takes a scope object; scope is implicit (current global).
 * 'scope' arg is silently ignored. */
static inline bool JS_ExecuteScript(JSContext* cx, JSObject* /*scope*/,
                                     JSScript* script, jsval* rval) {
    JS::RootedScript rs(cx, script);
    JS::RootedValue rv(cx);
    bool ok = JS_ExecuteScript(cx, rs, &rv);
    if (rval) *rval = rv;
    return ok;
}
/* JSObject* script variant (old code may store script as JSObject*) */
static inline bool JS_ExecuteScript(JSContext* cx, JSObject* /*scope*/,
                                     JSObject* script, jsval* rval) {
    return JS_ExecuteScript(cx, /*scope=*/(JSObject*)nullptr,
                             reinterpret_cast<JSScript*>(script), rval);
}

/* JS_EvaluateScript: compile+execute a string; returns bool like old API */
static inline bool JS_EvaluateScript(JSContext* cx, JSObject* obj,
        const char* src, size_t len, const char* filename, unsigned lineno, jsval* rval) {
    JSScript* script = JS_CompileScript(cx, obj, src, len, filename, lineno);
    if (!script) return false;
    return JS_ExecuteScript(cx, obj, script, rval);
}

/* JS_GetContextPrivate / JS_SetContextPrivate: still exist but verify */
/* (These are used in exec.cpp; if they compile, leave as-is) */

/* ================================================================
 * End SM128 compatibility shims
 * ================================================================ */

#endif /* JS_SHIMS_HPP */
