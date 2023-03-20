#if defined __ATTACH_OBJECT_INC__
#endinput
#endif
#define __ATTACH_OBJECT_INC__

// ------------------------------------------------------------
/*

Simple Dynamic Object Attachments by Naseband.

Aimed at working around the buggy YSF functionality and even removing that dependency.

Uses pawnraknet plugin to attach player objects to player objects belonging to the streamer and handles streaming for attached objects.

Currently the only way to attach dynamic objects to other dynamic objects is AttachDynamicObjectToObject which requires YSF.
However, YSF has quite a few problems ranging from bugs to crashes (ie attached objects often attach to the wrong object if they restream).

This include works for 0.3.7 & 0.3DL Servers as well as samp-compat and all client versions (incl. the client side 0.3DL compat ASI plugin).

Please note:

Most of the streaming logic is handled in PAWN. It's quite straight forward thanks to E_STREAMER_CUSTOM but still not optimal if you have a huge amount.

When a dynamic object is attached to another dynamic object the base object priority should be higher than the attached object.
Not doing so might result in an endless loop of restreaming objects, as attachments might take away an object slot from the base item (which frees up slots etc etc).

There are currently no checks whether the base object and attached objects share a virtual world/interior. They will never stream if they don't.

It's currently not possible to chain up multiple attachments (ie attach A to B and B to C).

*/

// ------------------------------------------------------------ Config (redefine before including if necessary)

#if !defined MAX_DYN_ATTACHMENTS
#define MAX_DYN_ATTACHMENTS					250
#endif

// Indexes of the Streamer's E_STREAMER_CUSTOM(...) array.
// If this collides, define the following two constants with two unique IDs that work for you.

#if !defined DYN_ATTACHMENTS_CID_BASE
#define DYN_ATTACHMENTS_CID_BASE			101
#endif

#if !defined DYN_ATTACHMENTS_CID_CHILD
#define DYN_ATTACHMENTS_CID_CHILD			102
#endif

// Hides all child attachments on player disconnect.
// Makes sure that when they come back no static items remain visible.
// This prevents a small object spike when the player enters a virtual world/interior for the first time.
// May not be optimal for large amount of attachments!

#if !defined DYN_ATTACHMENTS_SAFE_DESTREAM
#define DYN_ATTACHMENTS_SAFE_DESTREAM		true
#endif

#if !defined DYN_ATTACHMENTS_DEBUG_PREFIX
#define DYN_ATTACHMENTS_DEBUG_PREFIX		"[DYNATT]"
#endif

// #define DYN_ATTACHMENTS_DEBUG_STREAMING	Debug messages for base and child object streaming
// #define DYN_ATTACHMENTS_DEBUG_ALLOC		Debug messages for allocation (internal data)
// #define DYN_ATTACHMENTS_DEBUG_ALLOC_STR	Debug messages for allocation (streamer)

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

static enum E_DYNAMIC_ATTACHMENT
{
	bool:Used,

	ObjectID,
	BaseObjectID,

	Float:X,
	Float:Y,
	Float:Z,
	Float:RX,
	Float:RY,
	Float:RZ,

	bool:SyncRotation,
	bool:NoCameraCol,
	StreamMode,

	ModelID,
	Float:DrawDistance,

	bool:WasStatic
};

static gDynamicAttachments[MAX_DYN_ATTACHMENTS][E_DYNAMIC_ATTACHMENT];

// Used internally to retrieve a list of attachments from the streamer.
static gDynamicAttachmentsList[MAX_DYN_ATTACHMENTS];

// ------------------------------------------------------------ Functions

// Note: Internal ID. Returned by AttachDynObjectToDynaObject.
stock IsValidDynObjectAttachment(att_id)
{
	return (att_id >= 0 && att_id < MAX_DYN_ATTACHMENTS && gDynamicAttachments[att_id][Used]);
}

// objectid: Dynamic object to attach
// base_objectid: Dynamic object that objectid is attached to
// If update is true the object will be attached instantly if both objects are visible to players
// Returns the ID of the attachment slot.
// Returns -1 if failed (ie. object is already attached or used as base for another object)
stock AttachDynObjectToDynObject(objectid, base_objectid, Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:rz, bool:sync_rotation = true, bool:no_camera_col = false, stream_mode = ATT_STREAM_MODE_BASE, bool:instant_attach = true)
{
	if(!IsValidDynamicObject(objectid) || !IsValidDynamicObject(base_objectid))
		return -1;

	if(Streamer_HasIntData(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_CHILD)) || Streamer_HasArrayData(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_BASE)))
		return -1;

	new att_id = -1;

	for(new i = 0; i < MAX_DYN_ATTACHMENTS; ++i)
	{
		if(!gDynamicAttachments[i][Used])
		{
			att_id = i;
			break;
		}
	}

	if(att_id == -1)
		return -1;

	if(stream_mode < 0 || stream_mode >= MAX_DYN_ATTACHMENT_STREAM_MODES)
		stream_mode = ATT_STREAM_MODE_BASE;

	gDynamicAttachments[att_id][Used] = true;

	gDynamicAttachments[att_id][ObjectID] = objectid;
	gDynamicAttachments[att_id][BaseObjectID] = base_objectid;

	gDynamicAttachments[att_id][X] = x;
	gDynamicAttachments[att_id][Y] = y;
	gDynamicAttachments[att_id][Z] = z;
	gDynamicAttachments[att_id][RX] = rx;
	gDynamicAttachments[att_id][RY] = ry;
	gDynamicAttachments[att_id][RZ] = rz;

	gDynamicAttachments[att_id][SyncRotation] = sync_rotation;
	gDynamicAttachments[att_id][NoCameraCol] = no_camera_col;
	gDynamicAttachments[att_id][StreamMode] = stream_mode;

	gDynamicAttachments[att_id][WasStatic] = bool:Streamer_IsToggleItemStatic(STREAMER_TYPE_OBJECT, objectid);

	Streamer_ToggleItemCallbacks(STREAMER_TYPE_OBJECT, objectid, true);
	Streamer_ToggleItemCallbacks(STREAMER_TYPE_OBJECT, base_objectid, true);

	switch(stream_mode)
	{
		case ATT_STREAM_MODE_BASE:
		{
			if(!gDynamicAttachments[att_id][WasStatic])
				Streamer_ToggleItemStatic(STREAMER_TYPE_OBJECT, objectid, true);

			if(Streamer_IsToggleItemStatic(STREAMER_TYPE_OBJECT, base_objectid))
				Streamer_ToggleItemStatic(STREAMER_TYPE_OBJECT, base_objectid, false);
		}

		case ATT_STREAM_MODE_CHILD:
		{
			if(gDynamicAttachments[att_id][WasStatic])
				Streamer_ToggleItemStatic(STREAMER_TYPE_OBJECT, objectid, false);

			if(!Streamer_IsToggleItemStatic(STREAMER_TYPE_OBJECT, base_objectid))
				Streamer_ToggleItemStatic(STREAMER_TYPE_OBJECT, base_objectid, true);
		}
	}

	UpdateDynObjectAttachmentData(att_id);
	UpdateDynObjectAttachmentStream(att_id, instant_attach);

	#if defined DYN_ATTACHMENTS_DEBUG_ALLOC
	printf(DYN_ATTACHMENTS_DEBUG_PREFIX " Created att_id %i", att_id);
	#endif

	// Set Base CID

	Streamer_SetIntData(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_CHILD), att_id);

	// Set Child CID

	if(Streamer_HasArrayData(STREAMER_TYPE_OBJECT, base_objectid, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_BASE)))
	{
		Streamer_AppendArrayData(STREAMER_TYPE_OBJECT, base_objectid, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_BASE), att_id);

		#if defined DYN_ATTACHMENTS_DEBUG_ALLOC_STR
		printf(DYN_ATTACHMENTS_DEBUG_PREFIX " Appended att_id %i Base CID", att_id);
		#endif
	}
	else
	{
		Streamer_SetIntData(STREAMER_TYPE_OBJECT, base_objectid, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_BASE), att_id);

		#if defined DYN_ATTACHMENTS_DEBUG_ALLOC_STR
		printf(DYN_ATTACHMENTS_DEBUG_PREFIX " Created att_id %i Base CID (first attachment)", att_id);
		#endif
	}

	return att_id;
}

stock GetDynAttBaseObjectID(att_id)
{
	if(!IsValidDynObjectAttachment(att_id))
		return INVALID_STREAMER_ID;

	return gDynamicAttachments[att_id][BaseObjectID];
}

stock GetDynAttChildObjectID(att_id)
{
	if(!IsValidDynObjectAttachment(att_id))
		return INVALID_STREAMER_ID;

	return gDynamicAttachments[att_id][ObjectID];
}

// Updates ModelID, DrawDistance and position for this attachment.
// Needs to be called if either of them is changed.
stock UpdateDynObjectAttachmentData(att_id)
{
	if(!IsValidDynObjectAttachment(att_id))
		return 0;

	gDynamicAttachments[att_id][ModelID] = Streamer_GetIntData(STREAMER_TYPE_OBJECT, gDynamicAttachments[att_id][ObjectID], E_STREAMER_MODEL_ID);

	Streamer_GetFloatData(STREAMER_TYPE_OBJECT, gDynamicAttachments[att_id][ObjectID], E_STREAMER_DRAW_DISTANCE, gDynamicAttachments[att_id][DrawDistance]);

	switch(gDynamicAttachments[att_id][StreamMode])
	{
		case ATT_STREAM_MODE_BASE:
		{
			SetDynamicObjectPos(gDynamicAttachments[att_id][ObjectID], -25000.0, -25000.0, -1000.0);
		}

		case ATT_STREAM_MODE_CHILD:
		{
			new Float:x, Float:y, Float:z;
			GetDynamicObjectPos(gDynamicAttachments[att_id][BaseObjectID], x, y, z);
			SetDynamicObjectPos(gDynamicAttachments[att_id][ObjectID], x, y, z);
		}
	}

	return 1;
}

// Updates attachment visibility to players depending on base object state.
// Might be necessary to call if you change virtual worlds or similar and want to avoid some glitches.
// If instant_attach is true the item will be reattached instantly (if possible)
stock UpdateDynObjectAttachmentStream(att_id, bool:instant_attach = false)
{
	if(!IsValidDynObjectAttachment(att_id))
		return 0;

	for(new playerid = 0; playerid < MAX_PLAYERS; ++playerid)
	{
		// Update attachment visibility

		if(Streamer_IsItemVisible(playerid, STREAMER_TYPE_OBJECT, gDynamicAttachments[att_id][BaseObjectID]))
		{
			Streamer_ToggleItem(playerid, STREAMER_TYPE_OBJECT, gDynamicAttachments[att_id][ObjectID], true);

			// Attach if already visible

			if(instant_attach && !IsPlayerNPC(playerid))
			{
				new base_player_objectid = Streamer_GetItemInternalID(playerid, STREAMER_TYPE_OBJECT, gDynamicAttachments[att_id][BaseObjectID]);

				if(base_player_objectid != INVALID_OBJECT_ID)
				{
					new att_player_objectid = Streamer_GetItemInternalID(playerid, STREAMER_TYPE_OBJECT, gDynamicAttachments[att_id][ObjectID]);

					if(att_player_objectid != INVALID_OBJECT_ID)
					{
						AttachPlayerObjToPlayerObj(playerid, att_player_objectid, base_player_objectid, gDynamicAttachments[att_id][ModelID], gDynamicAttachments[att_id][X], gDynamicAttachments[att_id][Y], gDynamicAttachments[att_id][Z], gDynamicAttachments[att_id][RX], gDynamicAttachments[att_id][RY], gDynamicAttachments[att_id][RZ], gDynamicAttachments[att_id][SyncRotation], gDynamicAttachments[att_id][DrawDistance], gDynamicAttachments[att_id][NoCameraCol]);

						new modelid, txdname[32], texturename[32], matcolor;

						for(new i = 0; i < 16; ++i)
						{
							if(!IsDynamicObjectMaterialUsed(gDynamicAttachments[att_id][ObjectID], i))
								continue;

							GetDynamicObjectMaterial(gDynamicAttachments[att_id][ObjectID], i, modelid, txdname, texturename, matcolor);
							SetPlayerObjectMaterial(playerid, att_player_objectid, i, modelid, txdname, texturename, matcolor);
						}
					}
				}
			}
		}
		else
		{
			Streamer_ToggleItem(playerid, STREAMER_TYPE_OBJECT, gDynamicAttachments[att_id][ObjectID], false);
		}
	}

	return 1;
}

// If respawn is false the object will not be physically detached for players who see it (until they restream).
// Can lead to objects being attached to the wrong objects.
// Set to false if you want to destroy the object anyway.
stock DetachDynamicObjectByInternalID(att_id, bool:respawn = true)
{
	if(!IsValidDynObjectAttachment(att_id))
		return 0;

	gDynamicAttachments[att_id][Used] = false;

	#if defined DYN_ATTACHMENTS_DEBUG_ALLOC
	printf(DYN_ATTACHMENTS_DEBUG_PREFIX " Destroyed att_id %i", att_id);
	#endif

	Streamer_ToggleItemCallbacks(STREAMER_TYPE_OBJECT, gDynamicAttachments[att_id][ObjectID], false);
	Streamer_ToggleItemStatic(STREAMER_TYPE_OBJECT, gDynamicAttachments[att_id][ObjectID], gDynamicAttachments[att_id][WasStatic]);

	Streamer_RemoveIntData(STREAMER_TYPE_OBJECT, gDynamicAttachments[att_id][ObjectID], E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_CHILD));

	if(Streamer_HasArrayData(STREAMER_TYPE_OBJECT, gDynamicAttachments[att_id][BaseObjectID], E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_BASE)))
	{
		Streamer_RemoveArrayData(STREAMER_TYPE_OBJECT, gDynamicAttachments[att_id][BaseObjectID], E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_BASE), att_id);

		// Streamer_RemoveArrayData does not actually delete the container, its length will be -1 instead.
		// To delete the container, use Streamer_RemoveIntData.

		if(Streamer_GetArrayDataLength(STREAMER_TYPE_OBJECT, gDynamicAttachments[att_id][BaseObjectID], E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_BASE)) <= 0)
		{
			Streamer_RemoveIntData(STREAMER_TYPE_OBJECT, gDynamicAttachments[att_id][BaseObjectID], E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_BASE));

			#if defined DYN_ATTACHMENTS_DEBUG_ALLOC_STR
			printf(DYN_ATTACHMENTS_DEBUG_PREFIX " Deleted att_id %i Base CID (no attachments remaining)", att_id);
			#endif
		}
	}

	if(respawn)
		Streamer_SetIntData(STREAMER_TYPE_OBJECT, gDynamicAttachments[att_id][ObjectID], E_STREAMER_MODEL_ID, Streamer_GetIntData(STREAMER_TYPE_OBJECT, gDynamicAttachments[att_id][ObjectID], E_STREAMER_MODEL_ID));

	return 1;
}

stock DetachDynamicObject(objectid, bool:respawn = true)
{
	if(!IsValidDynamicObject(objectid))
		return 0;

	if(!Streamer_HasIntData(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_CHILD)))
		return 0;

	new att_id = Streamer_GetIntData(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_CHILD));

	if(!IsValidDynObjectAttachment(att_id) || gDynamicAttachments[att_id][ObjectID] != objectid)
		return 0;

	return DetachDynamicObjectByInternalID(att_id, respawn);
}

// objectid = base objectid of any existing attachments
stock DetachAllDynObjectsFromObject(objectid, bool:respawn = true)
{
	if(!IsValidDynamicObject(objectid))
		return 0;

	if(Streamer_HasArrayData(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_BASE)))
	{
		new length = Streamer_GetArrayDataLength(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_BASE));

		if(length > 0 && Streamer_GetArrayData(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_BASE), gDynamicAttachmentsList))
		{
			if(length > MAX_DYN_ATTACHMENTS)
				length = MAX_DYN_ATTACHMENTS;

			for(new i = 0; i < length; ++i)
			{
				new att_id = gDynamicAttachmentsList[i];

				if(IsValidDynObjectAttachment(att_id) && objectid == gDynamicAttachments[att_id][BaseObjectID])
				{
					DetachDynamicObjectByInternalID(att_id, respawn);
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
stock GetDynObjectAttachmentIndex(objectid, &bool:is_base = false)
{
	if(!IsValidDynamicObject(objectid))
		return 0;

	if(Streamer_HasArrayData(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_BASE)))
	{
		new length = Streamer_GetArrayDataLength(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_BASE));

		if(length > 0 && Streamer_GetArrayData(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_BASE), gDynamicAttachmentsList))
		{
			if(length > MAX_DYN_ATTACHMENTS)
				length = MAX_DYN_ATTACHMENTS;

			for(new i = 0; i < length; ++i)
			{
				if(IsValidDynObjectAttachment(gDynamicAttachmentsList[i]) && objectid == gDynamicAttachments[gDynamicAttachmentsList[i]][BaseObjectID])
				{
					is_base = true;

					return gDynamicAttachmentsList[i];
				}
			}
		}
	}

	if(Streamer_HasIntData(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_CHILD)))
	{
		new att_id = Streamer_GetIntData(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_CHILD));

		if(IsValidDynObjectAttachment(att_id) && gDynamicAttachments[att_id][ObjectID] == objectid)
		{
			is_base = false;

			return att_id;
		}
	}

	return 0;
}

// Gets all attached item internal ids from a base object.
// Returns the amount of indexes found, 0 if none were found.
stock GetAllDynObjectAttachments(objectid, array[], size = sizeof array)
{
	if(!IsValidDynamicObject(objectid))
		return 0;

	if(Streamer_HasArrayData(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_BASE)))
	{
		new length = Streamer_GetArrayDataLength(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_BASE));

		if(length > 0 && Streamer_GetArrayData(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_BASE), gDynamicAttachmentsList))
		{
			if(length > MAX_DYN_ATTACHMENTS)
				length = MAX_DYN_ATTACHMENTS;

			new count = 0;

			for(new i = 0; i < length; ++i)
			{
				if(IsValidDynObjectAttachment(gDynamicAttachmentsList[i]) && objectid == gDynamicAttachments[att_id][BaseObjectID])
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
AttachPlayerObjToPlayerObj(playerid, objectid, base_objectid, model, Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:rz, bool:sync_rotation = true, Float:draw_distance = 300.0, bool:no_camera_col = false)
{
	new BitStream:bs_out = BS_New();

	BS_WriteValue(bs_out,
		PR_INT16, objectid,
		PR_INT32, model,
		PR_FLOAT, x, PR_FLOAT, y, PR_FLOAT, z,
		PR_FLOAT, rx, PR_FLOAT, ry, PR_FLOAT, rz,
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
// Note that this will also count dynamic of other scripts.
stock CountDynObjectAttachments_D(&count_base, &count_child)
{
	count_base = 0;
	count_child = 0;

	new max_object_id = Streamer_GetUpperBound(STREAMER_TYPE_OBJECT);

	for(new objectid = 0; objectid <= max_object_id; ++objectid)
	{
		if(!IsValidDynamicObject(objectid))
			continue;

		if(Streamer_HasIntData(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_BASE)))
			++count_base;

		if(Streamer_HasIntData(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_CHILD)))
			++count_child;
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
			if(IsValidDynObjectAttachment(i) && IsValidDynamicObject(gDynamicAttachments[i][ObjectID]))
			{
				Streamer_ToggleItem(playerid, STREAMER_TYPE_OBJECT, gDynamicAttachments[i][ObjectID], false);
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
		// If the custom ID is set, it's (most definitely) an attachment.
		// From there get the player object ids of the base and attachment, then replace the player object as attachment via AttachPlayerObjToPlayerObj (RPC)

		if(Streamer_HasIntData(STREAMER_TYPE_OBJECT, id, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_CHILD)))
		{
			new att_id = Streamer_GetIntData(STREAMER_TYPE_OBJECT, id, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_CHILD));

			if(IsValidDynObjectAttachment(att_id) && _:id == gDynamicAttachments[att_id][ObjectID])
			{
				#if defined DYN_ATTACHMENTS_DEBUG_STREAMING
				printf(DYN_ATTACHMENTS_DEBUG_PREFIX " Child %i streamed in", _:id);
				#endif

				new base_player_objectid = Streamer_GetItemInternalID(forplayerid, STREAMER_TYPE_OBJECT, gDynamicAttachments[att_id][BaseObjectID]);

				if(base_player_objectid == INVALID_OBJECT_ID)
				{
					Streamer_ToggleItem(forplayerid, STREAMER_TYPE_OBJECT, id, false);

					#if defined DYN_ATTACHMENTS_DEBUG_STREAMING
					printf(DYN_ATTACHMENTS_DEBUG_PREFIX " Streamed out child %i because base is not visible", _:id);
					#endif
				}
				else
				{
					new att_player_objectid = Streamer_GetItemInternalID(forplayerid, STREAMER_TYPE_OBJECT, id);

					if(att_player_objectid != INVALID_OBJECT_ID)
					{
						AttachPlayerObjToPlayerObj(forplayerid, att_player_objectid, base_player_objectid, gDynamicAttachments[att_id][ModelID], gDynamicAttachments[att_id][X], gDynamicAttachments[att_id][Y], gDynamicAttachments[att_id][Z], gDynamicAttachments[att_id][RX], gDynamicAttachments[att_id][RY], gDynamicAttachments[att_id][RZ], gDynamicAttachments[att_id][SyncRotation], gDynamicAttachments[att_id][DrawDistance], gDynamicAttachments[att_id][NoCameraCol]);

						new modelid, txdname[32], texturename[32], matcolor;

						for(new i = 0; i < 16; ++i)
						{
							if(!IsDynamicObjectMaterialUsed(_:id, i))
								continue;

							GetDynamicObjectMaterial(_:id, i, modelid, txdname, texturename, matcolor);
							SetPlayerObjectMaterial(forplayerid, att_player_objectid, i, modelid, txdname, texturename, matcolor);
						}
					}
				}
			}
		}

		// This is a base item. Get all attachments of this base item and show them for this player.

		if(Streamer_HasArrayData(STREAMER_TYPE_OBJECT, id, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_BASE)))
		{
			#if defined DYN_ATTACHMENTS_DEBUG_STREAMING
			new bool:notified = false;
			#endif

			new length = Streamer_GetArrayDataLength(STREAMER_TYPE_OBJECT, id, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_BASE));

			if(length > 0 && Streamer_GetArrayData(STREAMER_TYPE_OBJECT, id, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_BASE), gDynamicAttachmentsList))
			{
				if(length > MAX_DYN_ATTACHMENTS)
					length = MAX_DYN_ATTACHMENTS;

				for(new i = 0; i < length; ++i)
				{
					new att_id = gDynamicAttachmentsList[i];

					if(IsValidDynObjectAttachment(att_id) && _:id == gDynamicAttachments[att_id][BaseObjectID])
					{
						#if defined DYN_ATTACHMENTS_DEBUG_STREAMING
						if(!notified)
						{
							printf(DYN_ATTACHMENTS_DEBUG_PREFIX " Base %i streamed in", _:id);
							notified = true;
						}
						#endif

						Streamer_ToggleItem(forplayerid, STREAMER_TYPE_OBJECT, gDynamicAttachments[att_id][ObjectID], true);
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
		// This is a base item. Get all attachments of this base item and hide them for this player.

		if(Streamer_HasArrayData(STREAMER_TYPE_OBJECT, id, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_BASE)))
		{
			#if defined DYN_ATTACHMENTS_DEBUG_STREAMING
			new bool:notified = false;
			#endif

			new length = Streamer_GetArrayDataLength(STREAMER_TYPE_OBJECT, id, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_BASE));

			if(length > 0 && Streamer_GetArrayData(STREAMER_TYPE_OBJECT, id, E_STREAMER_CUSTOM(DYN_ATTACHMENTS_CID_BASE), gDynamicAttachmentsList))
			{
				if(length > MAX_DYN_ATTACHMENTS)
					length = MAX_DYN_ATTACHMENTS;

				for(new i = 0; i < length; ++i)
				{
					new att_id = gDynamicAttachmentsList[i];

					if(IsValidDynObjectAttachment(att_id) && _:id == gDynamicAttachments[att_id][BaseObjectID])
					{
						#if defined DYN_ATTACHMENTS_DEBUG_STREAMING
						if(!notified)
						{
							printf(DYN_ATTACHMENTS_DEBUG_PREFIX " Base %i streamed out", _:id);
							notified = true;
						}
						#endif

						Streamer_ToggleItem(forplayerid, STREAMER_TYPE_OBJECT, gDynamicAttachments[att_id][ObjectID], false);
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
