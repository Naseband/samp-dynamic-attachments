# Dynamic Object Attachments

This include is aimed at working around the buggy YSF functionality and even removing that dependency. Instead, the pawnraknet plugin is used to attach the player objects created by the streamer via the CreateObject RPC.

The streamer object states are slightly modified to make streaming work. Ie. either the base items or attached items are set to be static. This can be configured on a per-attachment basis.

Works for 0.3.7 & 0.3DL Servers as well as samp-compat and all client versions (incl. the client side 0.3DL compat ASI plugin).

# Important

Most of the streaming logic is handled in PAWN. It's quite straight forward thanks to E_STREAMER_CUSTOM but still not optimal if you have a huge amount.

When a dynamic object is attached to another dynamic object the base object priority should be higher than the attached object.
Not doing so might result in an endless loop of restreaming objects, as attachments might take away an object slot from the base item (which frees up slots because attachments destream, etc etc).

It's currently not possible to chain up multiple attachments (ie attach A to B and B to C).

# Known issues

- Does not work well for objects that are attached to vehicles (ie attaching A to B and B to a vehicle). There are some streaming issues regarding static objects attached to vehicles.

- Currently attachments do not support material texts. You have to implement that yourself with a text buffer and GetDynamicObjectMaterialText.

- Memory usage is a problem when using a lot of attachments as E_STREAMER_CUSTOM uses a data container for each single attachment/base relation. I might switch to a single container for all attachments if deemed necessary.

# Usage

Similar to YSF, you can simply call AttachDynObjectToDynObject (named differently to avoid conflicts) and one object will be attached to the other.

AttachDynObjectToDynObject returns an index to the stored attachment data, or -1 if attaching failed.

This index can be used to detach the objects, but doesn't necessarily have to be kept as a reference.

*When the base object or child object is destroyed it is not detached automatically. You have to make sure to do so.*

To detach an object from its base you can use DetachDynamicObjectByInternalID(att_id) or DetachDynamicObject(objectid).

To detach all child objects from a base object use DetachAllDynamicObjectsFromObject(objectid).

All of these three functions clean up both the base and child object as well as the internal data. They can be destroyed or used otherwise afterwards.

You might need to restore the items to non-static and set the child items to be visible to all players though.

# Streaming Modes

There are currently two supported modes:

- ATT_STREAM_MODE_BASE

The attached object will be static and the base object will be streamed. This means that whenever the base object streams in for a player, all its attached objects will be forced to stream in aswell.

This is most suitable in cases where there is enough headroom for all attachments to stream and if the base object is meant to be moved around the map.

- ATT_STREAM_MODE_CHILD

The opposite of ATT_STREAM_MODE_BASE. The attached object will be streamed based on its position. The base object can be toggled static by the user to avoid streaming glitches, but it is not done by default.

Make sure to make use of priorities and set correct stream distances of the base and child objects.

This mode is unsuitable for moving the base object around over longer distances. It can be moved, but it should remain within the stream distance of its attachments. Not doing so might result in attachments to not appear when they need to.

# Functions

``` IsValidDynObjectAttachment(att_id) ```

Returns 1 if the specified attachment index exists. Returns 0 otherwise.

``` AttachDynObjectToDynObject(objectid, base_objectid, Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:rz, bool:sync_rotation = true, bool:no_camera_col = false, stream_mode = ATT_STREAM_MODE_BASE, bool:instant_attach = true) ```

Attaches a dynamic streamer object to another dynamic streamer object.

Returns the internal attachment id (att_id) or -1 if it fails.

``` UpdateDynObjectAttachmentData(att_id) ```

Updates the draw distance and model id of the child object. Needs to be called if either of them is changed via Streamer_SetIntData/Streamer_SetFloatData.

If the stream mode is ATT_STREAM_MODE_CHILD, the child object's position will be adjusted to the base object. This is to keep them in streaming range.
Useful if the base object is moved far away.

``` UpdateDynObjectAttachmentStream(att_id, bool:instant_attach = false) ```

Updates the child item's visibility to players.

If instant_attach is true, the child player objects will be attached to the base item if they are already visible to players.

Note: The visibility must be updated for both connected and non-connected players.

Use this function if you call any streamer function on child objects that might cause it to respawn (such as changing model, toggling it, changing virtual world/interior, etc).

``` DetachDynamicObjectByInternalID(att_id, bool:respawn = true) ```

Detaches an object from its base item. Clears up data on the objects and in the script.

If respawn is true, the child objects will be detached for all players that currently see it.

When detaching, objects might be in positions where they are not visible to any players. You may need to move it afterwards.

``` DetachDynamicObject(objectid, bool:respawn = true) ```

Same as above, but instead uses the streamer objectid of the child object.

``` DetachAllDynObjectsFromObject(objectid, bool:respawn = true) ```

Same as the two functions above, but detaches all child objects from the given base object (streamer objectid).

``` GetDynamicObjectAttachmentIndex(objectid, &bool:is_base = false) ```

Gets the ID of the internal attachment id (att_id) for this dynamic object.

The object can be a base or a child object. The variable passed to is_base will be set to true/false accordingly.

If the object is a base object, the returned index is the first attachment only. There might be more!

Returns -1 if it is neither a base or child object for any attachments.

``` GetAllDynamicObjectAttachments(objectid, array[], size = sizeof array) ```

Gets all internal attachment ids for this dynamic object.

The passed objectid is expected to be a base object for at least one attachment.

Returns the amount of attachments found, or 0 if none are found. Stops counting at max size of the passed array.

``` AttachPlayerObjToPlayerObj(playerid, objectid, base_objectid, model, Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:rz, bool:sync_rotation = true, Float:draw_distance = 300.0, bool:no_camera_col = false) ```

Internal function used to attach player objects to player objects. Provided in case you need it.

Could be replaced by a different kind of function if you need to for some reason.

This function does not do error checking. 

Because of the way objects were implemented objectid does not need to exist for this function to work.
However, if it does not the next player object created by the server that takes its slot will override this attachment on the client side.

To work around this you can create a dummy player object and use its ID to then create an attachment. That way the server knows that slot is used and functions such as SetPlayerObjectMaterial and DestroyPlayerObject actually work.

# Credits

IllidanS4, YSF and whoever added the original AttachPlayerObjectToObject function to it. I got the RPC ID and RPC format from YSF.

Incognito for the streamer.


