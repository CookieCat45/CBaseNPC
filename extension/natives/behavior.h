#ifndef NATIVE_BEHAVIOR_H_
#define NATIVE_BEHAVIOR_H_

#include "cbasenpc_behavior.h"

#define ACTIONFACTORYNATIVE(name) \
cell_t NextBotActionFactory_##name(IPluginContext *pContext, const cell_t *params) \
{ \
	HandleSecurity security; \
	security.pOwner = NULL; \
	security.pIdentity = myself->GetIdentity(); \
	Handle_t hndlObject = static_cast<Handle_t>(params[1]); \
	CBaseNPCPluginActionFactory *pFactory = nullptr; \
	READHANDLE(hndlObject, BaseNPCPluginActionFactory, pFactory) \

#define ACTIONNATIVE(name) \
cell_t NextBotAction_##name(IPluginContext *pContext, const cell_t *params) \
{ \
	CBaseNPCPluginAction *pAction = (CBaseNPCPluginAction*)params[1]; \
	if (!pAction) return pContext->ThrowNativeError("CBaseNPCPluginAction is NULL"); \
	CBaseNPCPluginActionFactory *pFactory = pAction->GetFactory(); \
	if (!pFactory) return pContext->ThrowNativeError("pFactory is NULL");

#define ACTIONDATANATIVE(name) \
	ACTIONNATIVE(name) \
	void* pData = pAction->GetData(); \
	if (!pData) return pContext->ThrowNativeError("Action has no user data"); \
	datamap_t* pDataMap = pAction->GetDataDescMap(); \
	if (!pDataMap) return pContext->ThrowNativeError("Action has no datamap"); \
	char* prop; pContext->LocalToString(params[2], &prop); \
	char error[256];

cell_t NextBotActionFactory_NextBotActionFactory(IPluginContext *pContext, const cell_t *params)
{
	char* name;
	pContext->LocalToString(params[1], &name);

	if (!name || !strlen(name))
		return pContext->ThrowNativeError("Action must have a name");
	
	CBaseNPCPluginActionFactory* pFactory = new CBaseNPCPluginActionFactory(name);

	Handle_t handle = CREATEHANDLE(BaseNPCPluginActionFactory, pFactory);
	pFactory->m_Handle = handle;

	return handle;
}

ACTIONFACTORYNATIVE(SetCallback)
	IPluginFunction* pCallback = pContext->GetFunctionById( params[3] );
	pFactory->SetCallback( (CBaseNPCPluginActionFactory::CallbackType)params[2], pCallback );
	return 0;
}

ACTIONFACTORYNATIVE(SetQueryCallback)
	IPluginFunction* pCallback = pContext->GetFunctionById( params[3] );
	pFactory->SetQueryCallback( (CBaseNPCPluginActionFactory::QueryCallbackType)params[2], pCallback );
	return 0;
}

ACTIONFACTORYNATIVE(SetEventCallback)
	IPluginFunction* pCallback = pContext->GetFunctionById( params[3] );
	pFactory->SetEventCallback( (CBaseNPCPluginActionFactory::EventResponderCallbackType)params[2], pCallback );
	return 0;
}

ACTIONFACTORYNATIVE(Create)
	return (cell_t)pFactory->Create();
}

ACTIONFACTORYNATIVE(BeginDataMapDesc)
	pFactory->BeginDataDesc();
	return params[1];
}

ACTIONFACTORYNATIVE(DefineIntField)
	char* fieldName;
	pContext->LocalToString(params[2], &fieldName);
	pFactory->DefineField(fieldName, FIELD_INTEGER, params[3], 0, nullptr, 0);
	return params[1];
}

ACTIONFACTORYNATIVE(DefineFloatField)
	char* fieldName;
	pContext->LocalToString(params[2], &fieldName);
	pFactory->DefineField(fieldName, FIELD_FLOAT, params[3], 0, nullptr, 0);
	return params[1];
}

ACTIONFACTORYNATIVE(DefineCharField)
	char* fieldName;
	pContext->LocalToString(params[2], &fieldName);
	pFactory->DefineField(fieldName, FIELD_CHARACTER, params[3], 0, nullptr, 0);
	return params[1];
}

ACTIONFACTORYNATIVE(DefineBoolField)
	char* fieldName;
	pContext->LocalToString(params[2], &fieldName);
	pFactory->DefineField(fieldName, FIELD_BOOLEAN, params[3], 0, nullptr, 0);
	return params[1];
}

ACTIONFACTORYNATIVE(DefineVectorField)
	char* fieldName;
	pContext->LocalToString(params[2], &fieldName);
	pFactory->DefineField(fieldName, FIELD_VECTOR, params[3], 0, nullptr, 0);
	return params[1];
}

ACTIONFACTORYNATIVE(DefineStringField)
	char* fieldName;
	pContext->LocalToString(params[2], &fieldName);
	pFactory->DefineField(fieldName, FIELD_STRING, params[3], 0, nullptr, 0);
	return params[1];
}

ACTIONFACTORYNATIVE(EndDataMapDesc)
	pFactory->EndDataDesc();
	return 0;
}

ACTIONNATIVE(GetName)
	cell_t buffer = params[2];
	size_t bufferSize = params[3];

	return pContext->StringToLocal(buffer, bufferSize, pAction->GetName());
}

ACTIONNATIVE(GetFullName)

	cell_t buffer = params[2];
	size_t bufferSize = params[3];

	return pContext->StringToLocal(buffer, bufferSize, pAction->GetFullName());
}

ACTIONNATIVE(GetActor)
	return gamehelpers->EntityToBCompatRef( pAction->GetActor() );
}

ACTIONNATIVE(GetParent)
	return (cell_t)pAction->GetParentAction();
}

ACTIONNATIVE(GetActiveChild)
	return (cell_t)pAction->GetActiveChildAction();
}

ACTIONNATIVE(IsSuspended)
	return pAction->IsSuspended();
}

ACTIONDATANATIVE(GetData)
	
	cell_t value;
	int element = params[3];
	if (!pFactory->GetObjectData( pData, prop, value, element, error, sizeof(error)))
		return pContext->ThrowNativeError( error );

	return value;
}

ACTIONDATANATIVE(SetData)
	
	cell_t value = params[3];
	int element = params[4];

	if (!pFactory->SetObjectData( pData, prop, value, element, error, sizeof(error)))
		return pContext->ThrowNativeError( error );

	return 0;
}

ACTIONDATANATIVE(GetDataFloat)
	
	float value;
	int element = params[3];
	if (!pFactory->GetObjectDataFloat( pData, prop, value, element, error, sizeof(error)))
		return pContext->ThrowNativeError( error );

	return sp_ftoc(value);
}

ACTIONDATANATIVE(SetDataFloat)
	
	float value = sp_ctof( params[3] );
	int element = params[4];
	if (!pFactory->SetObjectDataFloat( pData, prop, value, element, error, sizeof(error)))
		return pContext->ThrowNativeError( error );

	return 0;
}

ACTIONDATANATIVE(GetDataVector)
	
	cell_t* vec;
	pContext->LocalToPhysAddr(params[3], &vec);
	int element = params[4];

	float value[3];
	if (!pFactory->GetObjectDataVector( pData, prop, value, element, error, sizeof(error)))
		return pContext->ThrowNativeError( error );

	vec[0] = sp_ftoc(value[0]); vec[1] = sp_ftoc(value[1]); vec[2] = sp_ftoc(value[2]); 
	return 0;
}

ACTIONDATANATIVE(SetDataVector)
	
	cell_t* vec;
	pContext->LocalToPhysAddr(params[3], &vec);
	int element = params[4];

	float value[3];
	value[0] = sp_ctof(vec[0]); value[1] = sp_ctof(vec[1]); value[2] = sp_ctof(vec[2]); 

	if (!pFactory->SetObjectDataVector( pData, prop, value, element, error, sizeof(error)))
		return pContext->ThrowNativeError( error );

	return 0;
}

ACTIONDATANATIVE(GetDataString)

	cell_t dest = params[3];
	size_t bufferSize = params[4];
	int element = params[5];

	char * value = new char[ bufferSize ];
	bool result = pFactory->GetObjectDataString( pData, prop, value, bufferSize, element, error, sizeof(error) );
	if (result)
	{
		size_t len;
		pContext->StringToLocalUTF8(dest, bufferSize, value, &len);
	}
	else
	{
		pContext->StringToLocal(dest, bufferSize, "");
	}

	delete[] value;

	return result ? 0 : pContext->ThrowNativeError( error );
}

ACTIONDATANATIVE(SetDataString)

	cell_t src = params[3];
	int element = params[4];

	char* value;
	pContext->LocalToStringNULL(src, &value);
	if (!value) value = "";

	if (!pFactory->SetObjectDataString( pData, prop, value, element, error, sizeof(error)))
		return pContext->ThrowNativeError( error );

	return 0;
}

ACTIONNATIVE(Continue)
	pAction->PluginContinue();
	return 0;
}

ACTIONNATIVE(ChangeTo)
	char* reason = nullptr;
	pContext->LocalToStringNULL( params[3], &reason );
	pAction->PluginChangeTo( (CBaseNPCPluginAction*)params[2], reason );
	return 0;
}

ACTIONNATIVE(SuspendFor)
	char* reason = nullptr;
	pContext->LocalToStringNULL( params[3], &reason );
	pAction->PluginSuspendFor( (CBaseNPCPluginAction*)params[2], reason );
	return 0;
}

ACTIONNATIVE(Done)
	char* reason = nullptr;
	pContext->LocalToStringNULL( params[2], &reason );
	pAction->PluginDone(reason);
	return 0;
}

ACTIONNATIVE(TryContinue)
	pAction->PluginTryContinue( (EventResultPriorityType)params[2] );
	return 0;
}

ACTIONNATIVE(TryChangeTo)
	CBaseNPCPluginAction * pOtherAction = (CBaseNPCPluginAction*)params[2];
	if (!pOtherAction) return pContext->ThrowNativeError("action is NULL");

	char* reason = nullptr;
	pContext->LocalToStringNULL( params[4], &reason );

	pAction->PluginTryChangeTo( pOtherAction, (EventResultPriorityType)params[3], reason );
	return 0;
}

ACTIONNATIVE(TrySuspendFor)
	CBaseNPCPluginAction * pOtherAction = (CBaseNPCPluginAction*)params[2];
	if (!pOtherAction) return pContext->ThrowNativeError("action is NULL");

	char* reason = nullptr;
	pContext->LocalToStringNULL( params[4], &reason );

	pAction->PluginTrySuspendFor( pOtherAction, (EventResultPriorityType)params[3], reason );
	return 0;
}

ACTIONNATIVE(TryDone)
	char* reason;
	pContext->LocalToStringNULL( params[3], &reason );

	pAction->PluginTryDone( (EventResultPriorityType)params[2], reason );
	return 0;
}

ACTIONNATIVE(TryToSustain)
	char* reason;
	pContext->LocalToStringNULL( params[3], &reason );

	pAction->PluginTryToSustain( (EventResultPriorityType)params[2], reason );
	return 0;
}

#endif