#include <algorithm>
#include <cassert>
#include <limits>

#include "Angelscript/util/ASUtil.h"
#include "Angelscript/util/StringUtils.h"

#include "Angelscript/CASModule.h"
#include "Angelscript/CASModuleDescriptor.h"

#include "CASEvent.h"
#include "CASEventCaller.h"

#include "CASEventManager.h"

CASEventManager::CASEventManager( asIScriptEngine& engine, const char* const pszNamespace )
	: m_Engine( engine )
{
	assert( pszNamespace );

	m_Engine.AddRef();

	m_szNamespace = pszNamespace;

	as::Trim( m_szNamespace );
}

CASEventManager::~CASEventManager()
{
	m_Engine.Release();

	UnhookAllFunctions();
}

CASEvent* CASEventManager::GetEventByIndex( const uint32_t uiIndex )
{
	asDWORD uiAccessMask = 0xFFFFFFFF;

	auto pCtx = asGetActiveContext();

	if( pCtx )
	{
		//A script is calling us, get the access mask from the calling module.
		auto pModule = GetModuleFromScriptContext( pCtx );

		if( !pModule )
		{
			as::CASCallerInfo info;

			as::GetCallerInfo( info, pCtx );

			as::Critical( "CEventManager::GetEventByIndex: %s(%d, %d): Couldn't get module for index \"%u\"!\n", info.pszSection, info.iLine, info.iColumn, uiIndex );
			return nullptr;
		}

		uiAccessMask = pModule->GetDescriptor().GetAccessMask();
	}

	if( uiIndex < GetEventCount() )
	{
		auto pEvent = m_Events[ uiIndex ];

		if( pEvent->GetAccessMask() & uiAccessMask )
			return pEvent;

		as::CASCallerInfo info;

		as::GetCallerInfo( info, pCtx );

		as::Verbose( "CEventManager::GetEventByIndex: %s( %d, %d ): Access denied for event \"%s::%s\" (index %u)\n", 
					 info.pszSection, info.iLine, info.iColumn, 
					 pEvent->GetCategory(), pEvent->GetName(), uiIndex );
		return nullptr;
	}

	return nullptr;
}

CASEvent* CASEventManager::FindEventByName( const std::string& szName )
{
	asDWORD uiAccessMask = 0xFFFFFFFF;

	auto pCtx = asGetActiveContext();

	if( pCtx )
	{
		//A script is calling us, get the access mask from the calling module.
		auto pModule = GetModuleFromScriptContext( pCtx );

		if( !pModule )
		{
			as::CASCallerInfo info;

			as::GetCallerInfo( info, pCtx );

			as::Critical( "CEventManager::FindEventByName: %s(%d, %d): Couldn't get calling module for event \"%s\"!\n", info.pszSection, info.iLine, info.iColumn, szName.c_str() );
			return nullptr;
		}

		uiAccessMask = pModule->GetDescriptor().GetAccessMask();
	}


	auto szNamespace = as::ExtractNamespaceFromName( szName );
	const auto szEventName = as::ExtractNameFromName( szName );

	if( !m_szNamespace.empty() )
	{
		const auto szPrefix = m_szNamespace + "::";

		const auto index = szNamespace.find( szPrefix );

		//If the user specified the event namespace as the namespace, strip it.
		if( index != std::string::npos )
		{
			szNamespace = szNamespace.substr( szPrefix.length() );
		}
	}

	for( auto pEvent : m_Events )
	{
		if( szNamespace == pEvent->GetCategory() && 
			szEventName == pEvent->GetName() )
		{
			//Access mask must allow use of this event.
			if( pEvent->GetAccessMask() & uiAccessMask )
				return pEvent;

			as::CASCallerInfo info;

			as::GetCallerInfo( info, pCtx );

			as::Verbose( "CEventManager::FindEventByName: %s( %d, %d ): Access denied for event \"%s\"\n", info.pszSection, info.iLine, info.iColumn, szName.c_str() );
			return nullptr;
		}
	}

	return nullptr;
}

bool CASEventManager::HookEvent( const std::string& szName, void* pValue, const int iTypeId )
{
	auto pEvent = FindEventByName( szName );

	if( !pEvent )
	{
		as::CASCallerInfo info;

		as::GetCallerInfo( info );

		as::Critical( "CEventManager::HookEvent: %s(%d, %d): Couldn't find event \"%s\"!\n", info.pszSection, info.iLine, info.iColumn, szName.c_str() );
		return false;
	}

	return pEvent->Hook( pValue, iTypeId );
}

void CASEventManager::UnhookEvent( const std::string& szName, void* pValue, const int iTypeId )
{
	auto pEvent = FindEventByName( szName );

	if( !pEvent )
	{
		as::CASCallerInfo info;

		as::GetCallerInfo( info );

		as::Critical( "CEventManager::UnhookEvent: %s(%d, %d): Couldn't find event \"%s\"!\n", info.pszSection, info.iLine, info.iColumn, szName.c_str() );
	}

	pEvent->Unhook( pValue, iTypeId );
}

bool CASEventManager::AddEvent( CASEvent* pEvent )
{
	assert( pEvent );

	if( !pEvent )
		return false;

	if( std::find( m_Events.begin(), m_Events.end(), pEvent ) != m_Events.end() )
		return true;

	//Hit maximum number of events.
	if( GetEventCount() >= UINT32_MAX )
		return false;

	m_Events.push_back( pEvent );

	return true;
}

void CASEventManager::RegisterEvents( asIScriptEngine& engine )
{
	std::string szOldNS = engine.GetDefaultNamespace();

	const asDWORD accessMask = engine.SetDefaultAccessMask( 0xFFFFFFFF );

	engine.RegisterGlobalProperty( "CEventManager g_EventManager", this );

	asUINT uiHookIndex = engine.GetFuncdefCount();

	std::string szNS;
	std::string szDeclaration;

	int result;

	for( auto pEvent : m_Events )
	{
		szNS = m_szNamespace;

		if( *pEvent->GetCategory() )
		{
			szNS += "::";
			szNS += pEvent->GetCategory();
		}

		result = engine.SetDefaultNamespace( szNS.c_str() );
		assert( result >= 0 );

		engine.SetDefaultAccessMask( pEvent->GetAccessMask() );

		szDeclaration = std::string( "::CEvent " ) + pEvent->GetName();

		engine.RegisterGlobalProperty( szDeclaration.c_str(), pEvent );

		result = engine.SetDefaultNamespace( "" );
		assert( result >= 0 );

		szDeclaration = std::string( "HookReturnCode " ) + pEvent->GetName() + "Hook(" + pEvent->GetArguments() + ")";

		result = engine.RegisterFuncdef( szDeclaration.c_str() );
		assert( result >= 0 );

		pEvent->SetFuncDef( engine.GetFuncdefByIndex( uiHookIndex++ )->GetFuncdefSignature() );
	}

	result = engine.SetDefaultNamespace( szOldNS.c_str() );
	assert( result >= 0 );

	engine.SetDefaultAccessMask( accessMask );
}

void CASEventManager::UnhookModuleFunctions( CASModule* pModule )
{
	assert( pModule );

	if( !pModule )
		return;

	for( auto pEvent : m_Events )
	{
		pEvent->RemoveFunctionsOfModule( pModule );
	}
}

void CASEventManager::UnhookAllFunctions()
{
	for( auto pEvent : m_Events )
	{
		pEvent->RemoveAllFunctions();
	}
}

void CASEventManager::DumpHookedFunctions() const
{
	for( auto pEvent : m_Events )
	{
		pEvent->DumpHookedFunctions();
	}
}

static void RegisterScriptCEventManager( asIScriptEngine& engine )
{
	const char* const pszObjectName = "CEventManager";

	engine.RegisterObjectType( pszObjectName, 0, asOBJ_REF | asOBJ_NOCOUNT );

	engine.RegisterObjectMethod(
		pszObjectName, "uint32 GetEventCount() const",
		asMETHOD( CASEventManager, GetEventCount ), asCALL_THISCALL );

	engine.RegisterObjectMethod(
		pszObjectName, "CEvent@ GetEventByIndex(const uint32 uiIndex)",
		asMETHOD( CASEventManager, GetEventByIndex ), asCALL_THISCALL );

	engine.RegisterObjectMethod(
		pszObjectName, "CEvent@ FindEventByName(const " AS_STRING_OBJNAME "& in szName)",
		asMETHOD( CASEventManager, FindEventByName ), asCALL_THISCALL );

	engine.RegisterObjectMethod(
		pszObjectName, "bool HookEvent(const " AS_STRING_OBJNAME "& in szName, ?& in pFunction)",
		asMETHOD( CASEventManager, HookEvent ), asCALL_THISCALL );

	engine.RegisterObjectMethod(
		pszObjectName, "void UnhookEvent(const " AS_STRING_OBJNAME "& in szName, ?& in pFunction)",
		asMETHOD( CASEventManager, UnhookEvent ), asCALL_THISCALL );
}

void RegisterScriptEventAPI( asIScriptEngine& engine )
{
	const asDWORD accessMask = engine.SetDefaultAccessMask( 0xFFFFFFFF );

	RegisterScriptHookReturnCode( engine );
	RegisterScriptCBaseEvent( engine );
	RegisterScriptCEvent( engine );
	RegisterScriptCEventManager( engine );

	engine.SetDefaultAccessMask( accessMask );
}
