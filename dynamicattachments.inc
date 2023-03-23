#if defined __ATTACH_OBJECT_INC__
#endinput
#endif
#define __ATTACH_OBJECT_INC__

// ------------------------------------------------------------
/*

Simple Dynamic Object Attachments by Naseband.

Aimed at working around the buggy YSF functionality and even removing that dependency.

Uses pawnraknet plugin to attach player objects to player objects belonging to the streamer and handles streaming for attached objects.

*/

// ------------------------------------------------------------ Config (redefine before including if necessary)

#if !defined MAX_DYN_ATTACHMENTS
#define MAX_DYN_ATTACHMENTS					250
#endif

// Indexes of the Streamer's data array.
// If this collides, define the following two constants with two unique IDs that work for you.

#if !defined DYN_ATTACHMENTS_CID_BASE
#define DYN_ATTACHMENTS_CID_BASE			E_STREAMER_CUSTOM(101)
#endif

#if !defined DYN_ATTACHMENTS_CID_CHILD
#define DYN_ATTACHMENTS_CID_CHILD			E_STREAMER_CUSTOM(102)
#endif

// Hides all child attachments on player disconnect.
// Makes sure that when they come back no static items remain visible.
// This prevents object spikes when the player enters a virtual world/interior for the first time (ie. first spawn).
// May not be optimal for large amount of attachments.

#if !defined DYN_ATTACHMENTS_SAFE_DESTREAM
#define DYN_ATTACHMENTS_SAFE_DESTREAM		true
#endif

#if !defined DYN_ATTACHMENTS_DEBUG_PREFIX
#define DYN_ATTACHMENTS_DEBUG_PREFIX		"[DYNATT]"
#endif

// #define DYN_ATTACHMENTS_DEBUG_STREAMING	Debug messages for base and child object streaming
// #define DYN_ATTACHMENTS_DEBUG_ALLOC		Debug messages for allocation (internal data)
// #define DYN_ATTACHMENTS_DEBUG_ALLOC_STR	Debug messages for allocation (streamer)

// #define DYN_ATTACHMENTS_NO_CALLBACK		If defined, OnDynObjectAttachmentStreamIn will be completely disabled.

// ------------------------------------------------------------ Constants

// --- from YSF (RPC.h)
#define RPC_CREATE_OBJECT				0x2C // 44

enum (+=1)
{
	ATT_STREAM_MODE_BASE = 0, // (default) Streams the base object in/out like normal, attachments will be static. Causes stream distance of attachments to be ignored.
	ATT_STREAM_MODE_CHILD, // The base object will be static, attachments will be streamed. Unsuitable for far moving objects or objects whose position is changed. A bit smoother streaming.
	ATT_STREAM_MODE_MANUAL, // If you know what you are doing. Does not modify the base or child object's stream distances.

	MAX_DYN_ATTACHMENT_STREAM_MODES
};

// ------------------------------------------------------------ Data

// --- Attachments

static enum E_DYNAMIC_ATTACHMENT
{
	bool:Used,

	StreamMode,
	ObjectID,
	BaseObjectID,

	Float:X,
	Float:Y,
	Float:Z,
	Float:RX,
	Float:RY,
	Float:RZ,

	ModelID,
	bool:SyncRotation,
	Float:DrawDistance,
	bool:NoCameraCol,
	bool:ApplyMaterials,

	#if !defined DYN_ATTACHMENTS_NO_CALLBACK
	bool:EnableCallback,
	#endif

	bool:WasStatic
};

static gDynAttachments[MAX_DYN_ATTACHMENTS][E_DYNAMIC_ATTACHMENT];

// --- Callback

#if !defined DYN_ATTACHMENTS_NO_CALLBACK
static
	gFuncIDX_OnStreamIn = -1, // -1 = init, -2 = does not exist, otherwise ID
	gFuncIDX_OnStreamOut = -1,
	bool:gCallingStreamIn = false; // Used to track availability of ModifyDynAttachmentForPlayer

enum E_DYN_ATT_STREAM_IN_DATA
{
	bool:e_DynAttStreamIn,
	e_DynAttModelID,
	Float:e_DynAttX,
	Float:e_DynAttY,
	Float:e_DynAttZ,
	Float:e_DynAttRX,
	Float:e_DynAttRY,
	Float:e_DynAttRZ,
	bool:e_DynAttSyncRotation,
	Float:e_DynAttDrawDistance,
	bool:e_DynAttNoCameraCol,
	bool:e_DynAttApplyMaterials
};

static gStreamInData[E_DYN_ATT_STREAM_IN_DATA];

#endif

// --- Misc

// Used internally to retrieve a list of attachments from the streamer.
static gDynamicAttachmentsList[MAX_DYN_ATTACHMENTS];

// ------------------------------------------------------------ Forward Declarations

#if !defined DYN_ATTACHMENTS_NO_CALLBACK
forward OnDynAttachmentStreamIn(playerid, att_id, base_player_objectid, att_player_objectid);
forward OnDynAttachmentStreamOut(playerid, att_id);
#endif

// ------------------------------------------------------------ Functions

// Note: Internal ID. Returned by AttachDynObjectToDynaObject.
stock IsValidDynAttachment(att_id)
{
	return (att_id >= 0 && att_id < MAX_DYN_ATTACHMENTS && gDynAttachments[att_id][Used]);
}

// objectid: Dynamic object to attach
// base_objectid: Dynamic object that objectid is attached to
// If update is true the object will be attached instantly if both objects are visible to players
// Returns the ID of the attachment slot.
// Returns -1 if failed (ie. object is already attached or used as base for another object)
stock AttachDynObjectToDynObject(objectid, base_objectid, Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:rz, bool:sync_rotation = true, bool:no_camera_col = false, stream_mode = ATT_STREAM_MODE_BASE, bool:apply_materials = true, bool:instant_attach = true)
{
	if(!IsValidDynamicObject(objectid) || !IsValidDynamicObject(base_objectid))
		return -1;

	if(Streamer_HasIntData(STREAMER_TYPE_OBJECT, objectid, DYN_ATTACHMENTS_CID_CHILD) || Streamer_HasArrayData(STREAMER_TYPE_OBJECT, objectid, DYN_ATTACHMENTS_CID_BASE))
		return -1;

	new att_id = -1;

	for(new i = 0; i < MAX_DYN_ATTACHMENTS; ++i)
	{
		if(!gDynAttachments[i][Used])
		{
			att_id = i;
			break;
		}
	}

	if(att_id == -1)
		return -1;

	if(stream_mode < 0 || stream_mode >= MAX_DYN_ATTACHMENT_STREAM_MODES)
		stream_mode = ATT_STREAM_MODE_BASE;

	gDynAttachments[att_id][Used] = true;
	gDynAttachments[att_id][StreamMode] = stream_mode;

	gDynAttachments[att_id][ObjectID] = objectid;
	gDynAttachments[att_id][BaseObjectID] = base_objectid;

	gDynAttachments[att_id][X] = x;
	gDynAttachments[att_id][Y] = y;
	gDynAttachments[att_id][Z] = z;
	gDynAttachments[att_id][RX] = rx;
	gDynAttachments[att_id][RY] = ry;
	gDynAttachments[att_id][RZ] = rz;

	gDynAttachments[att_id][SyncRotation] = sync_rotation;
	gDynAttachments[att_id][NoCameraCol] = no_camera_col;
	gDynAttachments[att_id][ApplyMaterials] = apply_materials;

	#if !defined DYN_ATTACHMENTS_NO_CALLBACK
	gDynAttachments[att_id][EnableCallback] = false;
	#endif

	gDynAttachments[att_id][WasStatic] = bool:Streamer_IsToggleItemStatic(STREAMER_TYPE_OBJECT, objectid);

	Streamer_ToggleItemCallbacks(STREAMER_TYPE_OBJECT, objectid, true);
	Streamer_ToggleItemCallbacks(STREAMER_TYPE_OBJECT, base_objectid, true);

	switch(stream_mode)
	{
		case ATT_STREAM_MODE_BASE:
		{
			if(!gDynAttachments[att_id][WasStatic])
				Streamer_ToggleItemStatic(STREAMER_TYPE_OBJECT, objectid, true);

			if(Streamer_IsToggleItemStatic(STREAMER_TYPE_OBJECT, base_objectid))
				Streamer_ToggleItemStatic(STREAMER_TYPE_OBJECT, base_objectid, false);
		}

		case ATT_STREAM_MODE_CHILD:
		{
			if(gDynAttachments[att_id][WasStatic])
				Streamer_ToggleItemStatic(STREAMER_TYPE_OBJECT, objectid, false);

			if(!Streamer_IsToggleItemStatic(STREAMER_TYPE_OBJECT, base_objectid))
				Streamer_ToggleItemStatic(STREAMER_TYPE_OBJECT, base_objectid, true);
		}
	}

	// Set Base CID

	Streamer_SetIntData(STREAMER_TYPE_OBJECT, objectid, DYN_ATTACHMENTS_CID_CHILD, att_id);

	// Set Child CID

	if(Streamer_HasArrayData(STREAMER_TYPE_OBJECT, base_objectid, DYN_ATTACHMENTS_CID_BASE))
	{
		Streamer_AppendArrayData(STREAMER_TYPE_OBJECT, base_objectid, DYN_ATTACHMENTS_CID_BASE, att_id);

		#if defined DYN_ATTACHMENTS_DEBUG_ALLOC_STR
		printf(DYN_ATTACHMENTS_DEBUG_PREFIX " Appended att_id %i Base CID", att_id);
		#endif
	}
	else
	{
		Streamer_SetIntData(STREAMER_TYPE_OBJECT, base_objectid, DYN_ATTACHMENTS_CID_BASE, att_id);

		#if defined DYN_ATTACHMENTS_DEBUG_ALLOC_STR
		printf(DYN_ATTACHMENTS_DEBUG_PREFIX " Created att_id %i Base CID (first attachment)", att_id);
		#endif
	}

	UpdateDynAttachmentData(att_id);
	UpdateDynAttachmentStream(att_id, instant_attach);

	#if defined DYN_ATTACHMENTS_DEBUG_ALLOC
	printf(DYN_ATTACHMENTS_DEBUG_PREFIX " Created att_id %i", att_id);
	#endif

	return att_id;
}

// Updates ModelID, DrawDistance and position for this attachment.
// Needs to be called if either of them is changed.
stock UpdateDynAttachmentData(att_id)
{
	if(!IsValidDynAttachment(att_id))
		return 0;

	gDynAttachments[att_id][ModelID] = Streamer_GetIntData(STREAMER_TYPE_OBJECT, gDynAttachments[att_id][ObjectID], E_STREAMER_MODEL_ID);

	Streamer_GetFloatData(STREAMER_TYPE_OBJECT, gDynAttachments[att_id][ObjectID], E_STREAMER_DRAW_DISTANCE, gDynAttachments[att_id][DrawDistance]);

	switch(gDynAttachments[att_id][StreamMode])
	{
		case ATT_STREAM_MODE_BASE:
		{
			SetDynamicObjectPos(gDynAttachments[att_id][ObjectID], -25000.0, -25000.0, -1000.0);
		}

		case ATT_STREAM_MODE_CHILD:
		{
			new Float:x, Float:y, Float:z;
			GetDynamicObjectPos(gDynAttachments[att_id][BaseObjectID], x, y, z);
			SetDynamicObjectPos(gDynAttachments[att_id][ObjectID], x, y, z);
		}
	}

	return 1;
}

// Updates attachment visibility to players depending on base object state.
// Might be necessary to call if you change virtual worlds or similar and want to avoid some glitches.
// If instant_attach is true the item will be reattached instantly (if possible)
stock UpdateDynAttachmentStream(att_id, bool:instant_attach = false)
{
	if(!IsValidDynAttachment(att_id))
		return 0;

	for(new playerid = 0; playerid < MAX_PLAYERS; ++playerid)
	{
		// Update attachment visibility

		if(Streamer_IsItemVisible(playerid, STREAMER_TYPE_OBJECT, gDynAttachments[att_id][BaseObjectID]))
		{
			Streamer_ToggleItem(playerid, STREAMER_TYPE_OBJECT, gDynAttachments[att_id][ObjectID], true);

			// Attach if already visible

			if(instant_attach && !IsPlayerNPC(playerid))
			{
				DynAtt_StreamIn(playerid, att_id);
			}
		}
		else
		{
			Streamer_ToggleItem(playerid, STREAMER_TYPE_OBJECT, gDynAttachments[att_id][ObjectID], false);
		}
	}

	return 1;
}

// If respawn is false the object will not be physically detached for players who see it (until they restream).
// Can lead to objects being attached to the wrong objects.
// To avoid some overhead you can set destroy_child_object to true to destroy the dynamic object here.
stock DetachDynamicObjectByInternalID(att_id, bool:respawn = true, bool:destroy_child_object = false)
{
	if(!IsValidDynAttachment(att_id))
		return 0;

	gDynAttachments[att_id][Used] = false;

	#if defined DYN_ATTACHMENTS_DEBUG_ALLOC
	printf(DYN_ATTACHMENTS_DEBUG_PREFIX " Destroyed att_id %i", att_id);
	#endif

	// Child

	if(destroy_child_object)
	{
		DestroyDynamicObject(gDynAttachments[att_id][ObjectID]);

		#if defined DYN_ATTACHMENTS_DEBUG_ALLOC_STR
		printf(DYN_ATTACHMENTS_DEBUG_PREFIX " Destroyed att_id %i Child Object (destroy_child_object)", att_id);
		#endif
	}
	else
	{
		Streamer_ToggleItemCallbacks(STREAMER_TYPE_OBJECT, gDynAttachments[att_id][ObjectID], false);
		Streamer_ToggleItemStatic(STREAMER_TYPE_OBJECT, gDynAttachments[att_id][ObjectID], gDynAttachments[att_id][WasStatic]);

		if(Streamer_HasIntData(STREAMER_TYPE_OBJECT, gDynAttachments[att_id][ObjectID], DYN_ATTACHMENTS_CID_CHILD))
		{
			#if defined DYN_ATTACHMENTS_DEBUG_ALLOC_STR
			printf(DYN_ATTACHMENTS_DEBUG_PREFIX " Deleted att_id %i Child CID", att_id);
			#endif

			Streamer_RemoveIntData(STREAMER_TYPE_OBJECT, gDynAttachments[att_id][ObjectID], DYN_ATTACHMENTS_CID_CHILD);
		}

		if(respawn)
			Streamer_SetIntData(STREAMER_TYPE_OBJECT, gDynAttachments[att_id][ObjectID], E_STREAMER_MODEL_ID, Streamer_GetIntData(STREAMER_TYPE_OBJECT, gDynAttachments[att_id][ObjectID], E_STREAMER_MODEL_ID));
	}

	// Base

	if(Streamer_HasArrayData(STREAMER_TYPE_OBJECT, gDynAttachments[att_id][BaseObjectID], DYN_ATTACHMENTS_CID_BASE))
	{
		Streamer_RemoveArrayData(STREAMER_TYPE_OBJECT, gDynAttachments[att_id][BaseObjectID], DYN_ATTACHMENTS_CID_BASE, att_id);

		// Streamer_RemoveArrayData does not actually delete the container, its length will be -1 instead.
		// To delete the container, use Streamer_RemoveIntData.

		if(Streamer_GetArrayDataLength(STREAMER_TYPE_OBJECT, gDynAttachments[att_id][BaseObjectID], DYN_ATTACHMENTS_CID_BASE) <= 0)
		{
			Streamer_RemoveIntData(STREAMER_TYPE_OBJECT, gDynAttachments[att_id][BaseObjectID], DYN_ATTACHMENTS_CID_BASE);

			#if defined DYN_ATTACHMENTS_DEBUG_ALLOC_STR
			printf(DYN_ATTACHMENTS_DEBUG_PREFIX " Deleted att_id %i Base CID (no attachments remaining)", att_id);
			#endif
		}
	}

	return 1;
}

stock DetachDynamicObject(objectid, bool:respawn = true)
{
	if(!IsValidDynamicObject(objectid))
		return 0;

	if(!Streamer_HasIntData(STREAMER_TYPE_OBJECT, objectid, DYN_ATTACHMENTS_CID_CHILD))
		return 0;

	new att_id = Streamer_GetIntData(STREAMER_TYPE_OBJECT, objectid, DYN_ATTACHMENTS_CID_CHILD);

	if(!IsValidDynAttachment(att_id) || gDynAttachments[att_id][ObjectID] != objectid)
		return 0;

	return DetachDynamicObjectByInternalID(att_id, respawn);
}

// objectid = base objectid of any existing attachments
stock DetachAllDynObjectsFromObject(objectid, bool:respawn = true, bool:destroy_child_objects = false)
{
	if(!IsValidDynamicObject(objectid))
		return 0;

	if(Streamer_HasArrayData(STREAMER_TYPE_OBJECT, objectid, DYN_ATTACHMENTS_CID_BASE))
	{
		new length = Streamer_GetArrayDataLength(STREAMER_TYPE_OBJECT, objectid, DYN_ATTACHMENTS_CID_BASE);

		if(length > 0 && Streamer_GetArrayData(STREAMER_TYPE_OBJECT, objectid, DYN_ATTACHMENTS_CID_BASE, gDynamicAttachmentsList))
		{
			if(length > MAX_DYN_ATTACHMENTS)
				length = MAX_DYN_ATTACHMENTS;

			for(new i = 0; i < length; ++i)
			{
				new att_id = gDynamicAttachmentsList[i];

				if(IsValidDynAttachment(att_id) && objectid == gDynAttachments[att_id][BaseObjectID])
				{
					DetachDynamicObjectByInternalID(att_id, respawn, destroy_child_objects);
				}
			}

			return length;
		}
	}

	return 0;
}


// Returns the internal attachment id if this is either a base or child item.
// Returns -1 if it's neither.
// Pass a variable to is_base to check whether it is a base object or child object.
// If it is a base object, the returned ID might only be the first of many attachments for which it is used as base.
stock GetDynamicObjectAttachmentIndex(objectid, &bool:is_base = false)
{
	if(!IsValidDynamicObject(objectid))
		return 0;

	if(Streamer_HasArrayData(STREAMER_TYPE_OBJECT, objectid, DYN_ATTACHMENTS_CID_BASE))
	{
		new length = Streamer_GetArrayDataLength(STREAMER_TYPE_OBJECT, objectid, DYN_ATTACHMENTS_CID_BASE);

		if(length > 0 && Streamer_GetArrayData(STREAMER_TYPE_OBJECT, objectid, DYN_ATTACHMENTS_CID_BASE, gDynamicAttachmentsList))
		{
			if(length > MAX_DYN_ATTACHMENTS)
				length = MAX_DYN_ATTACHMENTS;

			for(new i = 0; i < length; ++i)
			{
				if(IsValidDynAttachment(gDynamicAttachmentsList[i]) && objectid == gDynAttachments[gDynamicAttachmentsList[i]][BaseObjectID])
				{
					is_base = true;

					return gDynamicAttachmentsList[i];
				}
			}
		}
	}

	if(Streamer_HasIntData(STREAMER_TYPE_OBJECT, objectid, DYN_ATTACHMENTS_CID_CHILD))
	{
		new att_id = Streamer_GetIntData(STREAMER_TYPE_OBJECT, objectid, DYN_ATTACHMENTS_CID_CHILD);

		if(IsValidDynAttachment(att_id) && gDynAttachments[att_id][ObjectID] == objectid)
		{
			is_base = false;

			return att_id;
		}
	}

	return 0;
}

// Gets all attached item internal ids from a base object.
// Returns the amount of indexes found, 0 if none were found.
stock GetAllDynAttachmentsForObject(objectid, array[], size = sizeof array)
{
	if(!IsValidDynamicObject(objectid))
		return 0;

	if(Streamer_HasArrayData(STREAMER_TYPE_OBJECT, objectid, DYN_ATTACHMENTS_CID_BASE))
	{
		new length = Streamer_GetArrayDataLength(STREAMER_TYPE_OBJECT, objectid, DYN_ATTACHMENTS_CID_BASE);

		if(length > 0 && Streamer_GetArrayData(STREAMER_TYPE_OBJECT, objectid, DYN_ATTACHMENTS_CID_BASE, gDynamicAttachmentsList))
		{
			if(length > MAX_DYN_ATTACHMENTS)
				length = MAX_DYN_ATTACHMENTS;

			new count = 0;

			for(new i = 0; i < length; ++i)
			{
				if(IsValidDynAttachment(gDynamicAttachmentsList[i]) && objectid == gDynAttachments[att_id][BaseObjectID])
				{
					array[count++] = gDynamicAttachmentsList[i];

					if(count == size)
						break;
				}
			}

			return count;
		}
	}

	return 0;
}

#if !defined DYN_ATTACHMENTS_NO_CALLBACK
stock GetDynAttachmentStreamInData(stream_in_data[E_DYN_ATT_STREAM_IN_DATA])
{
	if(!gCallingStreamIn)
	{
		printf("GetDynAttachmentStreamInData(): This function can only be used in OnDynAttachmentStreamIn");
		return 0;
	}

	stream_in_data = gStreamInData;
	return 1;
}

stock SetDynAttachmentStreamInData(stream_in_data[E_DYN_ATT_STREAM_IN_DATA])
{
	if(!gCallingStreamIn)
	{
		printf("SetDynAttachmentStreamInData(): This function can only be used in OnDynAttachmentStreamIn");
		return 0;
	}

	gStreamInData = stream_in_data;
	return 1;
}
#endif

// ------------------------------------------------------------ Set/Get For Attachment Data

stock GetDynAttachmentBaseObjectID(att_id)
{
	if(!IsValidDynAttachment(att_id))
		return INVALID_STREAMER_ID;

	return gDynAttachments[att_id][BaseObjectID];
}

stock GetDynAttachmentChildObjectID(att_id)
{
	if(!IsValidDynAttachment(att_id))
		return INVALID_STREAMER_ID;

	return gDynAttachments[att_id][ObjectID];
}

stock SetDynAttachmentOffsetPos(att_id, Float:x, Float:y, Float:z)
{
	if(!IsValidDynAttachment(att_id))
		return 0;

	gDynAttachments[att_id][X] = x;
	gDynAttachments[att_id][Y] = y;
	gDynAttachments[att_id][Z] = z;

	return 1;
}

stock GetDynAttachmentOffsetPos(att_id, &Float:x, &Float:y, &Float:z)
{
	if(!IsValidDynAttachment(att_id))
		return 0;

	x = gDynAttachments[att_id][X];
	y = gDynAttachments[att_id][Y];
	z = gDynAttachments[att_id][Z];

	return 1;
}

stock SetDynAttachmentOffsetRot(att_id, Float:rx, Float:ry, Float:rz)
{
	if(!IsValidDynAttachment(att_id))
		return 0;

	gDynAttachments[att_id][RX] = rx;
	gDynAttachments[att_id][RY] = ry;
	gDynAttachments[att_id][RZ] = rz;

	return 1;
}

stock GetDynAttachmentOffsetRot(att_id, &Float:rx, &Float:ry, &Float:rz)
{
	if(!IsValidDynAttachment(att_id))
		return 0;

	rx = gDynAttachments[att_id][RX];
	ry = gDynAttachments[att_id][RY];
	rz = gDynAttachments[att_id][RZ];

	return 1;
}

stock SetDynAttachementModelID(att_id, model_id)
{
	if(!IsValidDynAttachement(att_id))
		return 0;

	gDynAttachements[att_id][ModelID] = model_id;

	return 1;
}

stock GetDynAttachementModelID(att_id, &model_id)
{
	if(!IsValidDynAttachement(att_id))
		return 0;

	model_id = gDynAttachements[att_id][ModelID];

	return 1;
}

stock SetDynAttachmentSyncRotation(att_id, bool:sync_rotation)
{
	if(!IsValidDynAttachment(att_id))
		return 0;

	gDynAttachments[att_id][SyncRotation] = sync_rotation;

	return 1;
}

stock GetDynAttachmentSyncRotation(att_id, &bool:sync_rotation)
{
	if(!IsValidDynAttachment(att_id))
		return 0;

	sync_rotation = gDynAttachments[att_id][SyncRotation];

	return 1;
}

stock SetDynAttachmentDrawDistance(att_id, Float:draw_distance)
{
	if(!IsValidDynAttachment(att_id))
		return 0;

	gDynAttachments[att_id][DrawDistance] = draw_distance;

	return 1;
}

stock GetDynAttachmentDrawDistance(att_id, &Float:draw_distance)
{
	if(!IsValidDynAttachment(att_id))
		return 0;

	draw_distance = gDynAttachments[att_id][DrawDistance];

	return 1;
}

stock SetDynAttachmentNoCameraCol(att_id, bool:no_camera_col)
{
	if(!IsValidDynAttachment(att_id))
		return 0;

	gDynAttachments[att_id][NoCameraCol] = no_camera_col;

	return 1;
}

stock GetDynAttachmentNoCameraCol(att_id, &bool:no_camera_col)
{
	if(!IsValidDynAttachment(att_id))
		return 0;

	no_camera_col = gDynAttachments[att_id][NoCameraCol];

	return 1;
}

stock SetDynAttachmentApplyMaterials(att_id, bool:apply_materials)
{
	if(!IsValidDynAttachment(att_id))
		return 0;

	gDynAttachments[att_id][ApplyMaterials] = apply_materials;

	return 1;
}

stock GetDynAttachmentApplyMaterials(att_id, &bool:apply_materials)
{
	if(!IsValidDynAttachment(att_id))
		return 0;

	apply_materials = gDynAttachments[att_id][ApplyMaterials];

	return 1;
}

stock SetDynAttachmentEnableCallback(att_id, bool:enable_callback)
{
	if(!IsValidDynAttachment(att_id))
		return 0;

	gDynAttachments[att_id][EnableCallback] = enable_callback;

	return 1;
}

stock GetDynAttachmentEnableCallback(att_id, &bool:enable_callback)
{
	if(!IsValidDynAttachment(att_id))
		return 0;

	enable_callback = gDynAttachments[att_id][EnableCallback];

	return 1;
}

// ------------------------------------------------------------ Internal Functions

// --- from YSF (objects.cpp @ 549)
// bs.Write((WORD)objectid);
// bs.Write(iModelID);
// bs.Write(vecPos);
// bs.Write(vecRot);
// bs.Write(fDrawDistance); // 159
// bs.Write(byteNoCameraCol);
// bs.Write((WORD)-1); // attached vehicle
// bs.Write((WORD)attachtoid); // attached object
// bs.Write(pAddon->vecOffset);
// bs.Write(pAddon->vecRot);
// bs.Write(byteSyncRot);
// ---

// Technically the player object (objectid) does not need to exist, as attaching playerobjects also creates objects (and overwrites the player object client-side if one exists).
// However, this will cause the server to not know about the object, causing it be overridden again client-side if that slot gets used later on.
// To avoid that, create a dummy player object first, then create the attachment.
// You can of course use an arbitrary objectid that would normally not get used by the server, but again this can give allocation problems.
stock AttachPlayerObjToPlayerObj(playerid, objectid, base_objectid, model, Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:rz, bool:sync_rotation = true, Float:draw_distance = 300.0, bool:no_camera_col = false)
{
	new BitStream:bs_out = BS_New();

	BS_WriteValue(bs_out,
		PR_INT16, objectid,
		PR_INT32, model,
		PR_FLOAT, 0.0, PR_FLOAT, 0.0, PR_FLOAT, 0.0,
		PR_FLOAT, 0.0, PR_FLOAT, 0.0, PR_FLOAT, 0.0,
		PR_FLOAT, draw_distance,
		PR_UINT8, no_camera_col,
		PR_INT16, -1,
		PR_INT16, base_objectid,
		PR_FLOAT, x, PR_FLOAT, y, PR_FLOAT, z,
		PR_FLOAT, rx, PR_FLOAT, ry, PR_FLOAT, rz,
		PR_UINT8, sync_rotation);

	PR_SendRPC(bs_out, playerid, RPC_CREATE_OBJECT);
	BS_Delete(bs_out);

	return 1;
}

// Debug RPCs - Requires interception of Outgoing RPC

// public OnOutgoingRPC(playerid, rpcid, BitStream:bs)
// {
// 	if(rpcid == RPC_CREATE_OBJECT)
// 	{
// 		new objectid,
// 			model,
// 			Float:x, Float:y, Float:z,
// 			Float:rx, Float:ry, Float:rz,
// 			Float:draw_distance,
// 			bool:no_camera_col,
// 			attached_vehicle,
// 			attached_object,
// 			Float:offx, Float:offy, Float:offz,
// 			Float:offrx, Float:offry, Float:offrz,
// 			bool:sync_rotation;

// 		BS_ReadValue(bs,
// 			PR_INT16, objectid,
// 			PR_INT32, model,
// 			PR_FLOAT, x, PR_FLOAT, y, PR_FLOAT, z,
// 			PR_FLOAT, rx, PR_FLOAT, ry, PR_FLOAT, rz,
// 			PR_FLOAT, draw_distance,
// 			PR_UINT8, no_camera_col,
// 			PR_INT16, attached_vehicle,
// 			PR_INT16, attached_object,
// 			PR_FLOAT, offx, PR_FLOAT, offy, PR_FLOAT, offz,
// 			PR_FLOAT, offrx, PR_FLOAT, offry, PR_FLOAT, offrz,
// 			PR_UINT8, sync_rotation);

// 		print("----------------");
// 		printf("objectid: %i", objectid);
// 		printf("model: %i", model);
// 		printf("x, y, z: %f, %f, %f", x, y, z);
// 		printf("rx, ry, rz: %f, %f, %f", rx, ry, rz);
// 		printf("draw_distance: %f", draw_distance);
// 		printf("no_camera_col: %i", no_camera_col);
// 		printf("attached_vehicle: %i", attached_vehicle);
// 		printf("attached_object: %i", attached_object);
// 		printf("offx, offy, offz: %f, %f, %f", offx, offy, offz);
// 		printf("offrx, offry, offrz: %f, %f, %f", offrx, offry, offrz);
// 		printf("sync_rotation: %i", sync_rotation);
// 		print("----------------");

// 		return 1;
// 	}
// 	return 1;
// }

// Loops through all streamer objects to find attachment base and child objects.
// Used for debugging.
// Note that this will also count attachments of other scripts.
stock CountDynObjectAttachments_D(&count_base, &count_child)
{
	count_base = 0;
	count_child = 0;

	new max_object_id = Streamer_GetUpperBound(STREAMER_TYPE_OBJECT);

	for(new objectid = 0; objectid <= max_object_id; ++objectid)
	{
		if(!IsValidDynamicObject(objectid))
			continue;

		if(Streamer_HasIntData(STREAMER_TYPE_OBJECT, objectid, DYN_ATTACHMENTS_CID_BASE))
			++count_base;

		if(Streamer_HasIntData(STREAMER_TYPE_OBJECT, objectid, DYN_ATTACHMENTS_CID_CHILD))
			++count_child;
	}

	return 1;
}

static stock DynAtt_StreamIn(playerid, att_id)
{
	new base_player_objectid = Streamer_GetItemInternalID(playerid, STREAMER_TYPE_OBJECT, gDynAttachments[att_id][BaseObjectID]);

	if(base_player_objectid != INVALID_OBJECT_ID)
	{
		new att_player_objectid = Streamer_GetItemInternalID(playerid, STREAMER_TYPE_OBJECT, gDynAttachments[att_id][ObjectID]);

		if(att_player_objectid != INVALID_OBJECT_ID)
		{
			#if !defined DYN_ATTACHMENTS_NO_CALLBACK

			if(!gDynAttachments[att_id][EnableCallback] || gFuncIDX_OnStreamIn == -2) // Stream in, callback is disabled or doesn't exist (skip checks)
			{
				#if defined DYN_ATTACHMENTS_DEBUG_STREAMING
				printf(DYN_ATTACHMENTS_DEBUG_PREFIX " Child %i streamed in (1)", gDynAttachments[att_id][ObjectID]);
				#endif

				AttachPlayerObjToPlayerObj(playerid, att_player_objectid, base_player_objectid, gDynAttachments[att_id][ModelID], gDynAttachments[att_id][X], gDynAttachments[att_id][Y], gDynAttachments[att_id][Z], gDynAttachments[att_id][RX], gDynAttachments[att_id][RY], gDynAttachments[att_id][RZ], gDynAttachments[att_id][SyncRotation], gDynAttachments[att_id][DrawDistance], gDynAttachments[att_id][NoCameraCol]);

				if(gDynAttachments[att_id][ApplyMaterials])
				{
					new modelid, txdname[32], texturename[32], matcolor;

					for(new i = 0; i < 16; ++i)
					{
						if(!IsDynamicObjectMaterialUsed(gDynAttachments[att_id][ObjectID], i))
							continue;

						GetDynamicObjectMaterial(gDynAttachments[att_id][ObjectID], i, modelid, txdname, texturename, matcolor);
						SetPlayerObjectMaterial(playerid, att_player_objectid, i, modelid, txdname, texturename, matcolor);
					}
				}

				return 1;
			}

			if(gFuncIDX_OnStreamIn == -1) // Check if it exists
			{
				gFuncIDX_OnStreamIn = funcidx("OnDynAttachmentStreamIn");

				if(gFuncIDX_OnStreamIn == -1)
					gFuncIDX_OnStreamIn = -2;
			}

			if(gFuncIDX_OnStreamIn > -1) // Callback exists
			{
				gStreamInData[e_DynAttStreamIn] = true,
				gStreamInData[e_DynAttModelID] = gDynAttachments[att_id][ModelID];
				gStreamInData[e_DynAttX] = gDynAttachments[att_id][X];
				gStreamInData[e_DynAttY] = gDynAttachments[att_id][Y];
				gStreamInData[e_DynAttZ] = gDynAttachments[att_id][Z];
				gStreamInData[e_DynAttRX] = gDynAttachments[att_id][RX];
				gStreamInData[e_DynAttRY] = gDynAttachments[att_id][RY];
				gStreamInData[e_DynAttRZ] = gDynAttachments[att_id][RZ];
				gStreamInData[e_DynAttSyncRotation] = gDynAttachments[att_id][SyncRotation];
				gStreamInData[e_DynAttDrawDistance] = gDynAttachments[att_id][DrawDistance];
				gStreamInData[e_DynAttNoCameraCol] = gDynAttachments[att_id][NoCameraCol];
				gStreamInData[e_DynAttApplyMaterials] = gDynAttachments[att_id][ApplyMaterials];

				gCallingStreamIn = true;

				// OnDynAttachmentStreamIn(playerid, att_id, base_player_objectid, att_player_objectid);
				CallLocalFunction("OnDynAttachmentStreamIn", "dddd", playerid, att_id, base_player_objectid, att_player_objectid);

				gCallingStreamIn = false;

				if(gStreamInData[e_DynAttStreamIn])
				{
					#if defined DYN_ATTACHMENTS_DEBUG_STREAMING
					printf(DYN_ATTACHMENTS_DEBUG_PREFIX " Child %i streamed in (2) (Callback)", gDynAttachments[att_id][ObjectID]);
					#endif

					AttachPlayerObjToPlayerObj(playerid, att_player_objectid, base_player_objectid, gStreamInData[e_DynAttModelID], gStreamInData[e_DynAttX], gStreamInData[e_DynAttY], gStreamInData[e_DynAttZ], gStreamInData[e_DynAttRX], gStreamInData[e_DynAttRY], gStreamInData[e_DynAttRZ], gStreamInData[e_DynAttSyncRotation], gStreamInData[e_DynAttDrawDistance], gStreamInData[e_DynAttNoCameraCol]);

					if(gStreamInData[e_DynAttApplyMaterials])
					{
						new modelid, txdname[32], texturename[32], matcolor;

						for(new i = 0; i < 16; ++i)
						{
							if(!IsDynamicObjectMaterialUsed(gDynAttachments[att_id][ObjectID], i))
								continue;

							GetDynamicObjectMaterial(gDynAttachments[att_id][ObjectID], i, modelid, txdname, texturename, matcolor);
							SetPlayerObjectMaterial(playerid, att_player_objectid, i, modelid, txdname, texturename, matcolor);
						}
					}
				}
				else
				{
					#if defined DYN_ATTACHMENTS_DEBUG_STREAMING
					printf(DYN_ATTACHMENTS_DEBUG_PREFIX " Child %i was prevented from streaming in (Callback)", gDynAttachments[att_id][ObjectID]);
					#endif

					Streamer_ToggleItem(playerid, STREAMER_TYPE_OBJECT, gDynAttachments[att_id][ObjectID], false);
				}

				return 1;
			}

			// If the callback doesn't exist on the very first call of this function, the default code below will be called. Otherwise it is not reached.

			#endif

			#if defined DYN_ATTACHMENTS_DEBUG_STREAMING
			printf(DYN_ATTACHMENTS_DEBUG_PREFIX " Child %i streamed in (3)", gDynAttachments[att_id][ObjectID]);
			#endif

			AttachPlayerObjToPlayerObj(playerid, att_player_objectid, base_player_objectid, gDynAttachments[att_id][ModelID], gDynAttachments[att_id][X], gDynAttachments[att_id][Y], gDynAttachments[att_id][Z], gDynAttachments[att_id][RX], gDynAttachments[att_id][RY], gDynAttachments[att_id][RZ], gDynAttachments[att_id][SyncRotation], gDynAttachments[att_id][DrawDistance], gDynAttachments[att_id][NoCameraCol]);

			if(gDynAttachments[att_id][ApplyMaterials])
			{
				new modelid, txdname[32], texturename[32], matcolor;

				for(new i = 0; i < 16; ++i)
				{
					if(!IsDynamicObjectMaterialUsed(gDynAttachments[att_id][ObjectID], i))
						continue;

					GetDynamicObjectMaterial(gDynAttachments[att_id][ObjectID], i, modelid, txdname, texturename, matcolor);
					SetPlayerObjectMaterial(playerid, att_player_objectid, i, modelid, txdname, texturename, matcolor);
				}
			}

			return 1;
		}
	}

	#if defined DYN_ATTACHMENTS_DEBUG_STREAMING
	printf(DYN_ATTACHMENTS_DEBUG_PREFIX " Child %i failed to stream in", gDynAttachments[att_id][ObjectID]);
	#endif

	return 0;
}

static stock DynAtt_StreamOut(playerid, att_id)
{
	#if defined DYN_ATTACHMENTS_DEBUG_STREAMING
	printf(DYN_ATTACHMENTS_DEBUG_PREFIX " Child %i streamed out", gDynAttachments[att_id][ObjectID]);
	#endif

	if(!gDynAttachments[att_id][EnableCallback])
		return 1;

	if(gFuncIDX_OnStreamOut == -1) // Check if it exists
	{
		gFuncIDX_OnStreamOut = funcidx("OnDynAttachmentStreamOut");

		if(gFuncIDX_OnStreamOut == -1)
			gFuncIDX_OnStreamOut = -2;
	}

	if(gFuncIDX_OnStreamOut > -1)
	{
		// OnDynAttachmentStreamOut(playerid, att_id)
		CallLocalFunction("OnDynAttachmentStreamOut", "dd", playerid, att_id);
	}

	return 1;
}

// ------------------------------------------------------------ Callback Hooks

#if DYN_ATTACHMENTS_SAFE_DESTREAM

public OnPlayerDisconnect(playerid, reason)
{
	if(!IsPlayerNPC(playerid))
	{
		for(new i = 0; i < MAX_DYN_ATTACHMENTS; ++i)
		{
			if(IsValidDynAttachment(i) && IsValidDynamicObject(gDynAttachments[i][ObjectID]))
			{
				Streamer_ToggleItem(playerid, STREAMER_TYPE_OBJECT, gDynAttachments[i][ObjectID], false);
			}
		}
	}

	// ---

	#if defined OATT_OnPlayerDisconnect
		return OATT_OnPlayerDisconnect(playerid, reason);
	#else
		return 1;
	#endif
}

#endif

public Streamer_OnItemStreamIn(type, STREAMER_ALL_TAGS:id, forplayerid)
{
	if(type == STREAMER_TYPE_OBJECT && !IsPlayerNPC(forplayerid))
	{
		// Child Item.

		if(Streamer_HasIntData(STREAMER_TYPE_OBJECT, id, DYN_ATTACHMENTS_CID_CHILD))
		{
			new att_id = Streamer_GetIntData(STREAMER_TYPE_OBJECT, id, DYN_ATTACHMENTS_CID_CHILD);

			if(IsValidDynAttachment(att_id) && _:id == gDynAttachments[att_id][ObjectID])
			{
				DynAtt_StreamIn(forplayerid, att_id);
			}
		}

		// Base Item. Get all attachments and toggle them for this player.

		if(Streamer_HasArrayData(STREAMER_TYPE_OBJECT, id, DYN_ATTACHMENTS_CID_BASE))
		{
			#if defined DYN_ATTACHMENTS_DEBUG_STREAMING
			new bool:notified = false;
			#endif

			new length = Streamer_GetArrayDataLength(STREAMER_TYPE_OBJECT, id, DYN_ATTACHMENTS_CID_BASE);

			if(length > 0 && Streamer_GetArrayData(STREAMER_TYPE_OBJECT, id, DYN_ATTACHMENTS_CID_BASE, gDynamicAttachmentsList))
			{
				if(length > MAX_DYN_ATTACHMENTS)
					length = MAX_DYN_ATTACHMENTS;

				for(new i = 0; i < length; ++i)
				{
					new att_id = gDynamicAttachmentsList[i];

					if(IsValidDynAttachment(att_id) && _:id == gDynAttachments[att_id][BaseObjectID])
					{
						#if defined DYN_ATTACHMENTS_DEBUG_STREAMING
						if(!notified)
						{
							printf(DYN_ATTACHMENTS_DEBUG_PREFIX " Base %i streamed in", _:id);
							notified = true;
						}
						#endif

						Streamer_ToggleItem(forplayerid, STREAMER_TYPE_OBJECT, gDynAttachments[att_id][ObjectID], true);
					}
				}
			}
		}
	}

	// ---

	#if defined OATT_Streamer_OnItemStreamIn
		return OATT_Streamer_OnItemStreamIn(type, id, forplayerid);
	#else
		return 1;
	#endif
}

public Streamer_OnItemStreamOut(type, STREAMER_ALL_TAGS:id, forplayerid)
{
	if(type == STREAMER_TYPE_OBJECT && !IsPlayerNPC(forplayerid))
	{
		// Child Item. Not needed if callback disabled.

		#if !defined DYN_ATTACHMENTS_NO_CALLBACK

		if(Streamer_HasIntData(STREAMER_TYPE_OBJECT, id, DYN_ATTACHMENTS_CID_CHILD))
		{
			new att_id = Streamer_GetIntData(STREAMER_TYPE_OBJECT, id, DYN_ATTACHMENTS_CID_CHILD);

			if(IsValidDynAttachment(att_id) && _:id == gDynAttachments[att_id][ObjectID])
			{
				DynAtt_StreamOut(forplayerid, att_id);
			}
		}

		#endif

		// Base Item. Get all attachments and toggle them for this player.

		if(Streamer_HasArrayData(STREAMER_TYPE_OBJECT, id, DYN_ATTACHMENTS_CID_BASE))
		{
			#if defined DYN_ATTACHMENTS_DEBUG_STREAMING
			new bool:notified = false;
			#endif

			new length = Streamer_GetArrayDataLength(STREAMER_TYPE_OBJECT, id, DYN_ATTACHMENTS_CID_BASE);

			if(length > 0 && Streamer_GetArrayData(STREAMER_TYPE_OBJECT, id, DYN_ATTACHMENTS_CID_BASE, gDynamicAttachmentsList))
			{
				if(length > MAX_DYN_ATTACHMENTS)
					length = MAX_DYN_ATTACHMENTS;

				for(new i = 0; i < length; ++i)
				{
					new att_id = gDynamicAttachmentsList[i];

					if(IsValidDynAttachment(att_id) && _:id == gDynAttachments[att_id][BaseObjectID])
					{
						#if defined DYN_ATTACHMENTS_DEBUG_STREAMING
						if(!notified)
						{
							printf(DYN_ATTACHMENTS_DEBUG_PREFIX " Base %i streamed out", _:id);
							notified = true;
						}
						#endif

						Streamer_ToggleItem(forplayerid, STREAMER_TYPE_OBJECT, gDynAttachments[att_id][ObjectID], false);
					}
				}
			}
		}
	}

	// ---

	#if defined OATT_Streamer_OnItemStreamOut
		return OATT_Streamer_OnItemStreamOut(type, id, forplayerid);
	#else
		return 1;
	#endif
}

// ------------------------------------------------------------ Hook Stuff

#if DYN_ATTACHMENTS_SAFE_DESTREAM

#if defined _ALS_OnPlayerDisconnect
	#undef OnPlayerDisconnect
#else
	#define _ALS_OnPlayerDisconnect
#endif
#define OnPlayerDisconnect OATT_OnPlayerDisconnect
#if defined OATT_OnPlayerDisconnect
	forward OATT_OnPlayerDisconnect(playerid, reason);
#endif

#endif

// ---

#if defined _ALS_Streamer_OnItemStreamIn
	#undef Streamer_OnItemStreamIn
#else
	#define _ALS_Streamer_OnItemStreamIn
#endif
#define Streamer_OnItemStreamIn OATT_Streamer_OnItemStreamIn
#if defined OATT_Streamer_OnItemStreamIn
	forward OATT_Streamer_OnItemStreamIn(type, STREAMER_ALL_TAGS:id, forplayerid);
#endif

// ---

#if defined _ALS_Streamer_OnItemStreamOut
	#undef Streamer_OnItemStreamOut
#else
	#define _ALS_Streamer_OnItemStreamOut
#endif
#define Streamer_OnItemStreamOut OATT_Streamer_OnItemStreamOut
#if defined OATT_Streamer_OnItemStreamOut
	forward OATT_Streamer_OnItemStreamOut(type, STREAMER_ALL_TAGS:id, forplayerid);
#endif

// ------------------------------------------------------------ EOF