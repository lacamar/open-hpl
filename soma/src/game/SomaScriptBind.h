/*
 * Binds plain C++ functions and lambdas to AngelScript declarations through the generic calling
 * convention (ABI independent). Arguments: primitives/enums by value, everything else through
 * the pointer AngelScript passes (references, value objects, handles). Methods take the object
 * as the first parameter (T& / const T& / T*).
 */

#ifndef SOMA_SCRIPT_BIND_H
#define SOMA_SCRIPT_BIND_H

#include <angelscript.h>

#include "SomaScriptApi.h"

#include <cassert>
#include <new>
#include <string>
#include <type_traits>
#include <utility>

namespace SomaBind
{
	template <typename T, typename = void> struct Arg
	{
		// Primitives and enums are stored in the argument slot itself
		static T Get(asIScriptGeneric *g, asUINT i) { return *(T *)g->GetAddressOfArg(i); }
	};
	template <typename T> struct Arg<T &>
	{
		static T &Get(asIScriptGeneric *g, asUINT i) { return **(T **)g->GetAddressOfArg(i); }
	};
	template <typename T> struct Arg<T *>
	{
		// Placeholders returned by unimplemented functions never reach engine code
		static T *Get(asIScriptGeneric *g, asUINT i)
		{
			T *p = *(T **)g->GetAddressOfArg(i);
			return SomaScriptIsDummy((void *)p) ? nullptr : p;
		}
	};
	template <typename T> struct Arg<T, typename std::enable_if<std::is_class<T>::value>::type>
	{
		static T Get(asIScriptGeneric *g, asUINT i) { return *(T *)g->GetArgObject(i); }
	};

	template <typename T> struct Obj
	{
		static T Get(asIScriptGeneric *g) { return *(typename std::remove_reference<T>::type *)g->GetObject(); }
	};
	template <typename T> struct Obj<T &>
	{
		static T &Get(asIScriptGeneric *g) { return *(T *)g->GetObject(); }
	};
	template <typename T> struct Obj<T *>
	{
		static T *Get(asIScriptGeneric *g) { return (T *)g->GetObject(); }
	};

	template <typename R, typename = void> struct Ret
	{
		static void Set(asIScriptGeneric *g, R r) { *(R *)g->GetAddressOfReturnLocation() = r; }
	};
	template <typename R> struct Ret<R, typename std::enable_if<std::is_class<R>::value>::type>
	{
		static void Set(asIScriptGeneric *g, R r) { new (g->GetAddressOfReturnLocation()) R(std::move(r)); }
	};
	template <typename R> struct Ret<R &>
	{
		static void Set(asIScriptGeneric *g, R &r) { g->SetReturnAddress((void *)&r); }
	};
	template <typename R> struct Ret<R *>
	{
		static void Set(asIScriptGeneric *g, R *r) { g->SetReturnAddress((void *)r); }
	};

	template <typename F> struct Traits;
	template <typename R, typename... A> struct Traits<R (*)(A...)>
	{
		using Result = R;
		static constexpr size_t Arity = sizeof...(A);
	};

	template <auto F, typename R, typename... A, size_t... I>
	void CallFunc(asIScriptGeneric *g, R (*)(A...), std::index_sequence<I...>)
	{
		if constexpr (std::is_void<R>::value)
			F(Arg<A>::Get(g, I)...);
		else
			Ret<R>::Set(g, F(Arg<A>::Get(g, I)...));
	}

	template <auto F> void GenericFunc(asIScriptGeneric *g)
	{
		CallFunc<F>(g, F, std::make_index_sequence<Traits<decltype(F)>::Arity>());
	}

	template <auto F, typename R, typename O, typename... A, size_t... I>
	void CallMethod(asIScriptGeneric *g, R (*)(O, A...), std::index_sequence<I...>)
	{
		if constexpr (std::is_void<R>::value)
			F(Obj<O>::Get(g), Arg<A>::Get(g, I)...);
		else
			Ret<R>::Set(g, F(Obj<O>::Get(g), Arg<A>::Get(g, I)...));
	}

	template <auto F> void GenericMethod(asIScriptGeneric *g)
	{
		if (SomaScriptIsDummy(g->GetObject()))
		{
			SomaScriptStubCall(g);
			return;
		}
		CallMethod<F>(g, F, std::make_index_sequence<Traits<decltype(F)>::Arity - 1>());
	}

	// Constructors: first parameter is the memory to construct into
	template <auto F, typename P, typename... A, size_t... I>
	void CallConstruct(asIScriptGeneric *g, void (*)(P, A...), std::index_sequence<I...>)
	{
		F((P)g->GetObject(), Arg<A>::Get(g, I)...);
	}

	template <auto F> void GenericConstruct(asIScriptGeneric *g)
	{
		CallConstruct<F>(g, F, std::make_index_sequence<Traits<decltype(F)>::Arity - 1>());
	}
}

// Script-facing registration; a failed registration is a programming error in the declaration.
#define SOMA_FUNC(engine, decl, ...) \
	do { int r_ = (engine)->RegisterGlobalFunction(decl, asFUNCTION((SomaBind::GenericFunc<__VA_ARGS__>)), asCALL_GENERIC); assert(r_ >= 0 || r_ == asALREADY_REGISTERED); (void)r_; } while (0)
#define SOMA_METHOD(engine, type, decl, ...) \
	do { int r_ = (engine)->RegisterObjectMethod(type, decl, asFUNCTION((SomaBind::GenericMethod<__VA_ARGS__>)), asCALL_GENERIC); assert(r_ >= 0 || r_ == asALREADY_REGISTERED); (void)r_; } while (0)
#define SOMA_CONSTRUCT(engine, type, decl, ...) \
	do { int r_ = (engine)->RegisterObjectBehaviour(type, asBEHAVE_CONSTRUCT, decl, asFUNCTION((SomaBind::GenericConstruct<__VA_ARGS__>)), asCALL_GENERIC); assert(r_ >= 0 || r_ == asALREADY_REGISTERED); (void)r_; } while (0)

// Skips declarations already registered (hand-written natives take precedence over generated ones)
#define SOMA_METHOD_NEW(engine, type, decl, ...) \
	do { asITypeInfo *t_ = (engine)->GetTypeInfoByName(type); if (t_ && t_->GetMethodByDecl(decl) == NULL) \
		(engine)->RegisterObjectMethod(type, decl, asFUNCTION((SomaBind::GenericMethod<__VA_ARGS__>)), asCALL_GENERIC); } while (0)
#define SOMA_FUNC_NEW(engine, decl, ...) \
	do { if ((engine)->GetGlobalFunctionByDecl(decl) == NULL) \
		(engine)->RegisterGlobalFunction(decl, asFUNCTION((SomaBind::GenericFunc<__VA_ARGS__>)), asCALL_GENERIC); } while (0)

#endif // SOMA_SCRIPT_BIND_H
