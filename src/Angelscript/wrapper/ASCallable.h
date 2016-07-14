#ifndef WRAPPER_CASCALLABLE_H
#define WRAPPER_CASCALLABLE_H

#include <cassert>
#include <cstdarg>
#include <cstdint>

#include <angelscript.h>

#include "CASContext.h"

class CASContext;
class CASArguments;

/**
*	@defgroup ASCallable Angelscript Call Utils
*
*	@{
*/

typedef uint32_t CallFlags_t;

namespace CallFlag
{
/**
*	Flags to affect function calls.
*/
enum CallFlag : CallFlags_t
{
	/**
	*	No flags.
	*/
	NONE = 0,
};
}

/**
*	Base class for callable types.
*	Do not use directly.
*/
class CASCallable
{
protected:
	template<typename CALLABLE, typename ARGS>
	friend bool CallFunction( CALLABLE& callable, CallFlags_t flags, const ARGS& args );

protected:
	/**
	*	Constructor.
	*	@param function Function to call.
	*	@param context Context to use for calls.
	*/
	CASCallable( asIScriptFunction& function, CASContext& context );

public:
	/**
	*	@return The function.
	*/
	asIScriptFunction& GetFunction() { return m_Function; }

	/**
	*	@return The context.
	*/
	CASContext& GetContext() { return m_Context; }

	/**
	*	@return Whether this function is valid.
	*/
	bool IsValid() const;

	/**
	*	Gets the return value.
	*	@param pReturnValue Pointer to the variable that will receive the return value. Must match the type being retrieved.
	*	@return true if the value was successfully retrieved, false otherwise.
	*/
	bool GetReturnValue( void* pReturnValue );

protected:
	/**
	*	Called before the arguments are set. Lets the callable type evaluate itself.
	*	Non-virtual, uses templates to invoke the correct type.
	*	@return true if the call should continue, false otherwise.
	*/
	bool PreSetArguments() { return true; }

	/**
	*	Called after arguments are set, before the function is executed. Lets the callable type evaluate itself.
	*	Non-virtual, uses templates to invoke the correct type.
	*	@return true if the call should continue, false otherwise.
	*/
	bool PreExecute() { return true; }

	/**
	*	Called after the function is executed. Lets the callable type evaluate itself.
	*	Non-virtual, uses templates to invoke the correct type.
	*	@param iResult Result of the asIScriptContext::Execute call.
	*	@return true if the call should continue, false otherwise.
	*/
	bool PostExecute( const int iResult ) { return true; }

private:
	asIScriptFunction& m_Function;
	CASContext& m_Context;

private:
	CASCallable( const CASCallable& ) = delete;
	CASCallable& operator=( const CASCallable& ) = delete;
};

/**
*	Helper class that defines the call methods for callable types.
*	Do not use directly.
*/
template<typename SUBCLASS>
class CASTCallable : public CASCallable
{
public:
	using CASCallable::CASCallable;

	/**
	*	Calls the function.
	*	@param flags Call flags.
	*	@param list Pointer to a va_list that contains the arguments for the function.
	*	@return true on success, false otherwise.
	*/
	bool VCall( CallFlags_t flags, va_list list )
	{
		return CallFunction( GetThisRef(), flags, list );
	}

	/**
	*	Calls the function.
	*	@param flags Call flags.
	*	@param ... The arguments for the function.
	*	@return true on success, false otherwise.
	*/
	bool Call( CallFlags_t flags, ... )
	{
		va_list list;

		va_start( list, flags );

		const auto success = VCall( flags, list );

		va_end( list );

		return success;
	}

	/**
	*	Calls the function.
	*	@param flags Call flags.
	*	@param args List of arguments.
	*	@return true on success, false otherwise.
	*/
	bool CallArgs( CallFlags_t flags, const CASArguments& args )
	{
		return CallFunction( GetThisRef(), flags, args );
	}

	/**
	*	Calls the function.
	*	@param flags Call flags.
	*	@param ... The arguments for the function.
	*	@return true on success, false otherwise.
	*/
	bool operator()( CallFlags_t flags, ... )
	{
		va_list list;

		va_start( list, flags );

		const auto success = VCall( flags, list );

		va_end( list );

		return success;
	}

private:
	inline SUBCLASS& GetThisRef() { return static_cast<SUBCLASS&>( *this ); }
};

/**
*	A regular function.
*/
class CASFunction final : public CASTCallable<CASFunction>
{
public:
	CASFunction( asIScriptFunction& function, CASContext& context )
		: CASTCallable( function, context )
	{
	}
};

/**
*	An object method.
*/
class CASMethod final : public CASTCallable<CASMethod>
{
protected:
	template<typename CALLABLE, typename ARGS>
	friend bool CallFunction( CALLABLE& callable, CallFlags_t flags, const ARGS& args );

public:
	/**
	*	@copydoc CASCallable::CASCallable( asIScriptFunction& function, CASContext& context )
	*	@param pThis Pointer to the object instance.
	*/
	CASMethod( asIScriptFunction& function, CASContext& context, void* pThis );

	bool IsValid() const;

protected:
	bool PreSetArguments();

private:
	void* m_pThis;
};

namespace as
{
/**
*	Performs a function call.
*	Do not use directly.
*	@param functor Functor that Can perform function calls.
*	@param pContext Script context. If null, acquires a context using asIScriptEngine::RequestContext.
*	@param flags Call flags.
*	@param pFunction Function to call.
*	@param args Argument list to use for the call.
*	@tparam FUNCTOR Functor type.
*	@tparam ARGS Argument list type.
*	@return true on success, false otherwise.
*/
template<typename FUNCTOR, typename ARGS>
inline bool VCall( FUNCTOR functor, asIScriptContext* pContext, CallFlags_t flags, asIScriptFunction* pFunction, const ARGS& args )
{
	assert( pFunction );

	if( !pFunction )
		return false;

	if( pContext )
	{
		CASContext ctx( *pContext );

		return functor( *pFunction, ctx, flags, args );
	}
	else
	{
		CASOwningContext ctx( *pFunction->GetEngine() );

		return functor( *pFunction, ctx, flags, args );
	}
}

/**
*	Internal helper functor used for calls.
*/
struct CASFunctionFunctor final
{
	/**
	*	Calls a function using varargs.
	*	@param function Function to call.
	*	@param context Context to use.
	*	@param flags Call flags.
	*	@param list List of arguments.
	*/
	bool operator()( asIScriptFunction& function, CASContext& context, CallFlags_t flags, va_list list )
	{
		CASFunction func( function, context );

		if( !func.IsValid() )
			return false;

		return func.VCall( flags, list );
	}

	/**
	*	Calls a function using an argument list.
	*	@param function Function to call.
	*	@param context Context to use.
	*	@param flags Call flags.
	*	@param args List of arguments.
	*/
	bool operator()( asIScriptFunction& function, CASContext& context, CallFlags_t flags, const CASArguments& args )
	{
		CASFunction func( function, context );

		if( !func.IsValid() )
			return false;

		return func.CallArgs( flags, args );
	}
};

/**
*	Internal helper functor used for calls.
*/
struct CASMethodFunctor final
{
	void* const pThis;

	/**
	*	Constructor.
	*	@param pThis This pointer.
	*/
	CASMethodFunctor( void* pThis )
		: pThis( pThis )
	{
	}

	/**
	*	Calls an object method using varargs.
	*	@param function Function to call.
	*	@param context Context to use.
	*	@param flags Call flags.
	*	@param list List of arguments.
	*/
	bool operator()( asIScriptFunction& function, CASContext& context, CallFlags_t flags, va_list list )
	{
		CASMethod method( function, context, pThis );

		if( !method.IsValid() )
			return false;

		return method.VCall( flags, list );
	}

	/**
	*	Calls an object method using an argument list.
	*	@param function Function to call.
	*	@param context Context to use.
	*	@param flags Call flags.
	*	@param args List of arguments.
	*/
	bool operator()( asIScriptFunction& function, CASContext& context, CallFlags_t flags, const CASArguments& args )
	{
		CASMethod method( function, context, pThis );

		if( !method.IsValid() )
			return false;

		return method.CallArgs( flags, args );
	}
};

/**
*	Overload that calls global functions.
*	Do not use directly.
*	@param pContext Script context. If null, acquires a context using asIScriptEngine::RequestContext.
*	@param flags Call flags.
*	@param pFunction Function to call.
*	@param args Argument list to use for the call.
*	@tparam ARGS Argument list type.
*	@return true on success, false otherwise.
*/
template<typename ARGS>
inline bool VCallFunc( asIScriptContext* pContext, CallFlags_t flags, asIScriptFunction* pFunction, const ARGS& args )
{
	return VCall( CASFunctionFunctor(), pContext, flags, pFunction, args );
}

/**
*	Overload that calls object methods.
*	Do not use directly.
*	@param pThis This pointer.
*	@param pContext Script context. If null, acquires a context using asIScriptEngine::RequestContext.
*	@param flags Call flags.
*	@param pFunction Function to call.
*	@param args Argument list to use for the call.
*	@tparam ARGS Argument list type.
*	@return true on success, false otherwise.
*/
template<typename ARGS>
inline bool VCallFunc( void* pThis, asIScriptContext* pContext, CallFlags_t flags, asIScriptFunction* pFunction, const ARGS& args )
{
	return VCall( CASMethodFunctor( pThis ), pContext, flags, pFunction, args );
}

/**
*	Implements a single function call.
*/
#define __IMPLEMENT_SINGLE_CALL( call, preCall, postCall )	\
	preCall()												\
	const auto success = call;								\
	postCall()												\
	return success

/**
*	Implements function overloads for calling functions or object methods.
*	@param funcCallName Name of the functions to define for global function calls.
*	@param methodCallName Name of the functions to define for object method calls.
*	@param funcToCall Which function to call to perform the call.
*	@param argType Argument list argument type and name.
*	@param argName Argument list name.
*	@param preCall Function macro to use before a call occurs.
*	@param postCall Function macro to use after a call has occurred.
*/
#define __IMPLEMENT_CALL( funcCallName, methodCallName, funcToCall, argType, argName, preCall, postCall )						\
inline bool funcCallName( asIScriptContext* pContext, CallFlags_t flags, asIScriptFunction* pFunction, argType )				\
{																																\
	__IMPLEMENT_SINGLE_CALL( ( funcToCall( pContext, flags, pFunction, argName ) ), preCall, postCall );						\
}																																\
																																\
inline bool funcCallName( CallFlags_t flags, asIScriptFunction* pFunction, argType )											\
{																																\
	__IMPLEMENT_SINGLE_CALL( 																									\
		( funcToCall( static_cast<asIScriptContext*>( nullptr ), flags, pFunction, argName ) ), 								\
		preCall, postCall );																									\
}																																\
																																\
inline bool funcCallName( asIScriptContext* pContext, asIScriptFunction* pFunction, argType )									\
{																																\
	__IMPLEMENT_SINGLE_CALL( ( funcToCall( pContext, CallFlag::NONE, pFunction, argName ) ), preCall, postCall );				\
}																																\
																																\
inline bool funcCallName( asIScriptFunction* pFunction, argType )																\
{																																\
	__IMPLEMENT_SINGLE_CALL( 																									\
		( funcToCall( static_cast<asIScriptContext*>( nullptr ), CallFlag::NONE, pFunction, argName ) ), 						\
		preCall, postCall );																									\
}																																\
																																\
inline bool methodCallName( void* pThis, asIScriptContext* pContext, CallFlags_t flags, asIScriptFunction* pFunction, argType )	\
{																																\
	__IMPLEMENT_SINGLE_CALL( ( funcToCall( pThis, pContext, flags, pFunction, argName ) ), preCall, postCall );					\
}																																\
																																\
inline bool methodCallName( void* pThis, CallFlags_t flags, asIScriptFunction* pFunction, argType )								\
{																																\
	__IMPLEMENT_SINGLE_CALL( ( funcToCall( pThis, nullptr, flags, pFunction, argName ) ), preCall, postCall );					\
}																																\
																																\
inline bool methodCallName( void* pThis, asIScriptContext* pContext, asIScriptFunction* pFunction, argType )					\
{																																\
	__IMPLEMENT_SINGLE_CALL( ( funcToCall( pThis, pContext, CallFlag::NONE, pFunction, argName ) ), preCall, postCall );		\
}																																\
																																\
inline bool methodCallName( void* pThis, asIScriptFunction* pFunction, argType )												\
{																																\
	__IMPLEMENT_SINGLE_CALL( ( funcToCall( pThis, nullptr, CallFlag::NONE, pFunction, argName ) ), preCall, postCall );			\
}

/**
*	Simplified version of __IMPLEMENT_CALL that uses the same name for global function and object method call functions.
*/
#define __IMPLEMENT_CALL_SIMPLE( funcName, funcToCall, argType, argName, preCall, postCall )	\
__IMPLEMENT_CALL( funcName, funcName, funcToCall, argType, argName, preCall, postCall )

#define __DO_NOTHING()

/*
*	Implement calls for argument lists.
*/
__IMPLEMENT_CALL_SIMPLE( CallArgs, VCallFunc, const CASArguments& args, args, __DO_NOTHING, __DO_NOTHING )

/*
*	Implement calls for va_list types.
*/
__IMPLEMENT_CALL_SIMPLE( VCall, VCallFunc, va_list list, list, __DO_NOTHING, __DO_NOTHING )

#define __VA_ARG_PARAM ...

#define __BEGIN_VA_LIST()		\
 va_list list;					\
va_start( list, pFunction );

#define __END_VA_LIST()		\
va_end( list );

/*
*	Implement calls for varargs.
*/
__IMPLEMENT_CALL_SIMPLE( Call, VCallFunc, __VA_ARG_PARAM, list, __BEGIN_VA_LIST, __END_VA_LIST )

/*
*	Clean up macros
*/
#undef __IMPLEMENT_SINGLE_CALL
#undef __DO_NOTHING
#undef __VA_ARG_PARAM
#undef __BEGIN_VA_LIST
#undef __END_VA_LIST

/*
*	These are all obsolete, but still work.
*	TODO: remove.
*/

/**
*	Calls the given function using the given context.
*	@param pFunction Function to call.
*	@param pContext Context to use.
*	@param flags Flags.
*	@param list Argument list.
*/
bool VCallFunction( asIScriptFunction* pFunction, asIScriptContext* pContext, CallFlags_t flags, va_list list );

/**
*	@see VCallFunction( asIScriptFunction* pFunction, asIScriptContext* pContext, CallFlags_t flags, va_list list )
*/
bool VCallFunction( asIScriptFunction* pFunction, asIScriptContext* pContext, va_list list );

/**
*	Acquires a context using asIScriptEngine::RequestContext
*	@see VCallFunction( asIScriptFunction* pFunction, asIScriptContext* pContext, CallFlags_t flags, va_list list )
*/
bool VCallFunction( asIScriptFunction* pFunction, CallFlags_t flags, va_list list );

/**
*	Acquires a context using asIScriptEngine::RequestContext
*	@see VCallFunction( asIScriptFunction* pFunction, asIScriptContext* pContext, CallFlags_t flags, va_list list )
*/
bool VCallFunction( asIScriptFunction* pFunction, va_list list );

/**
*	Calls the given function using the given context.
*	@param pFunction Function to call.
*	@param pContext Context to use.
*	@param flags Flags.
*	@param ... Arguments.
*/
bool CallFunction( asIScriptFunction* pFunction, asIScriptContext* pContext, CallFlags_t flags, ... );

/**
*	@see CallFunction( asIScriptFunction* pFunction, asIScriptContext* pContext, CallFlags_t flags, ... )
*/
bool CallFunction( asIScriptFunction* pFunction, asIScriptContext* pContext, ... );

/**
*	Acquires a context using asIScriptEngine::RequestContext
*	@see CallFunction( asIScriptFunction* pFunction, asIScriptContext* pContext, CallFlags_t flags, ... )
*/
bool CallFunction( asIScriptFunction* pFunction, CallFlags_t flags, ... );

/**
*	Acquires a context using asIScriptEngine::RequestContext
*	@see CallFunction( asIScriptFunction* pFunction, asIScriptContext* pContext, CallFlags_t flags, ... )
*/
bool CallFunction( asIScriptFunction* pFunction, ... );

/**
*	Calls the given function using the given context.
*	@param pFunction Function to call.
*	@param pContext Context to use.
*	@param flags Flags.
*	@param args Arguments.
*/
bool CallFunctionArgs( asIScriptFunction* pFunction, asIScriptContext* pContext, CallFlags_t flags, const CASArguments& args );

/**
*	@see CallFunctionArgs( asIScriptFunction* pFunction, asIScriptContext* pContext, CallFlags_t flags, const CASArguments& args )
*/
bool CallFunctionArgs( asIScriptFunction* pFunction, asIScriptContext* pContext, const CASArguments& args );

/**
*	Acquires a context using asIScriptEngine::RequestContext
*	@see CallFunctionArgs( asIScriptFunction* pFunction, asIScriptContext* pContext, CallFlags_t flags, const CASArguments& args )
*/
bool CallFunctionArgs( asIScriptFunction* pFunction, CallFlags_t flags, const CASArguments& args );

/**
*	Acquires a context using asIScriptEngine::RequestContext
*	@see CallFunctionArgs( asIScriptFunction* pFunction, asIScriptContext* pContext, CallFlags_t flags, const CASArguments& args )
*/
bool CallFunctionArgs( asIScriptFunction* pFunction, const CASArguments& args );

/**
*	Calls the given function using the given context.
*	@param pThis This pointer.
*	@param pFunction Function to call.
*	@param pContext Context to use.
*	@param flags Flags.
*	@param list Argument list.
*/
bool VCallMethod( void* pThis, asIScriptFunction* pFunction, asIScriptContext* pContext, CallFlags_t flags, va_list list );

/**
*	@see VCallMethod( void* pThis, asIScriptFunction* pFunction, asIScriptContext* pContext, CallFlags_t flags, va_list list )
*/
bool VCallMethod( void* pThis, asIScriptFunction* pFunction, asIScriptContext* pContext, va_list list );

/**
*	Acquires a context using asIScriptEngine::RequestContext
*	@see VCallMethod( void* pThis, asIScriptFunction* pFunction, asIScriptContext* pContext, CallFlags_t flags, va_list list )
*/
bool VCallMethod( void* pThis, asIScriptFunction* pFunction, CallFlags_t flags, va_list list );

/**
*	Acquires a context using asIScriptEngine::RequestContext
*	@see VCallMethod( void* pThis, asIScriptFunction* pFunction, asIScriptContext* pContext, CallFlags_t flags, va_list list )
*/
bool VCallMethod( void* pThis, asIScriptFunction* pFunction, va_list list );

/**
*	Calls the given function using the given context.
*	@param pThis This pointer.
*	@param pFunction Function to call.
*	@param pContext Context to use.
*	@param flags Flags.
*	@param ... Arguments.
*/
bool CallMethod( void* pThis, asIScriptFunction* pFunction, asIScriptContext* pContext, CallFlags_t flags, ... );

/**
*	@see CallMethod( void* pThis, asIScriptFunction* pFunction, asIScriptContext* pContext, CallFlags_t flags, ... )
*/
bool CallMethod( void* pThis, asIScriptFunction* pFunction, asIScriptContext* pContext, ... );

/**
*	Acquires a context using asIScriptEngine::RequestContext
*	@see CallMethod( void* pThis, asIScriptFunction* pFunction, asIScriptContext* pContext, CallFlags_t flags, ... )
*/
bool CallMethod( void* pThis, asIScriptFunction* pFunction, CallFlags_t flags, ... );

/**
*	Acquires a context using asIScriptEngine::RequestContext
*	@see CallMethod( void* pThis, asIScriptFunction* pFunction, asIScriptContext* pContext, CallFlags_t flags, ... )
*/
bool CallMethod( void* pThis, asIScriptFunction* pFunction, ... );

/**
*	Calls the given function using the given context.
*	@param pThis This pointer.
*	@param pFunction Function to call.
*	@param pContext Context to use.
*	@param flags Flags.
*	@param args Arguments.
*/
bool CallMethodArgs( void* pThis, asIScriptFunction* pFunction, asIScriptContext* pContext, CallFlags_t flags, const CASArguments& args );

/**
*	@see CallMethodArgs( asIScriptFunction* pFunction, asIScriptContext* pContext, CallFlags_t flags, const CASArguments& args )
*/
bool CallMethodArgs( void* pThis, asIScriptFunction* pFunction, asIScriptContext* pContext, const CASArguments& args );

/**
*	Acquires a context using asIScriptEngine::RequestContext
*	@see CallMethodArgs( asIScriptFunction* pFunction, asIScriptContext* pContext, CallFlags_t flags, const CASArguments& args )
*/
bool CallMethodArgs( void* pThis, asIScriptFunction* pFunction, CallFlags_t flags, const CASArguments& args );

/**
*	Acquires a context using asIScriptEngine::RequestContext
*	@see CallMethodArgs( asIScriptFunction* pFunction, asIScriptContext* pContext, CallFlags_t flags, const CASArguments& args )
*/
bool CallMethodArgs( void* pThis, asIScriptFunction* pFunction, const CASArguments& args );
}

/** @} */

#endif //WRAPPER_CASCALLABLE_H