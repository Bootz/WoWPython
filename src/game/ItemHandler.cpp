// Copyright (C) 2004 Team Python
//  
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software 
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#include "ItemHandler.h"
#include "NetworkInterface.h"
#include "Log.h"
#include "Opcodes.h"
#include "GameClient.h"
#include "Character.h"
#include "UpdateMask.h"
#include "WorldServer.h"
#include "Item.h"

#define world WorldServer::getSingleton( )

ItemHandler::ItemHandler( ) {
    debugcounter = 0;
}

ItemHandler::~ItemHandler( ) {

}

void ItemHandler::HandleMsg( wowWData & recv_data, GameClient *pClient ) {
    wowWData data;
    char f[ 256 ];
    sprintf( f, "WORLD: Item Opcode 0x%.4X", recv_data.opcode );
    Log::getSingleton( ).outString( f );
    switch( recv_data.opcode ) {
		case CMSG_SWAP_INV_ITEM:
			{
				//ok i'm gonna do that a looootttt cleaner :/
				uint8 srcslot, destslot;
                recv_data >> srcslot >> destslot;
				int slot = destslot;

				//START OF LINA LVL REQUIREMENT SWAP PATCH
				int8 CharLvl, ItemLvl;
				//printf("ITEM: LVL TEST\n");
                CharLvl=pClient->getCurrentChar()->getLevel();
				ItemLvl=WorldServer::getSingleton().GetItem(pClient->getCurrentChar()->getItemIdBySlot(srcslot))->RequiredLevel;
				//printf("ITEM: CharLvl %d, ItemLvl %d\n", CharLvl, ItemLvl);
				if( CharLvl < ItemLvl)
				{
					ChatHandler * MsgLvlItem = new ChatHandler;
					if(MsgLvlItem  !=NULL)
					{
						//NEED TO PUT SOME CODE TO UNGRAY ITEM
						uint8 buf[256];
						wowWData data;
 						sprintf((char*)buf,"You need the Lvl %d to equip that item.", ItemLvl);
						MsgLvlItem->FillMessageData(&data, 0x09, pClient, buf);
 						pClient->SendMsg( &data );
						delete(MsgLvlItem);
					}
					else printf("ITEM: CMSG_SWAP_INV_ITEM can't send message\n");
					return;
				}
				//END OF LINA LVL REQUIREMENT SWAP PATCH

				//these are the bags slots...ignore it for now
				if ((slot <= 22) && (slot >=19))
					destslot = srcslot; 

				//check to make sure items are not being put in wrong spots
				if (((srcslot > 23) && (destslot < 19)) || ((srcslot < 23) && (destslot > 19))) {
					if ((pClient->getCurrentChar()->getGuidBySlot(destslot) != 0) && (pClient->getCurrentChar()->getGuidBySlot(srcslot) != 0)) {
						Item * tmpitem1 = WorldServer::getSingleton().GetItem(pClient->getCurrentChar()->getItemIdBySlot(destslot));
						Item * tmpitem2 = WorldServer::getSingleton().GetItem(pClient->getCurrentChar()->getItemIdBySlot(srcslot));
						if ((tmpitem1 != NULL) && (tmpitem2 != NULL)) {
							if (tmpitem1->Inventorytype != tmpitem2->Inventorytype) {
								data.Initialise( 18 , SMSG_INVENTORY_CHANGE_FAILURE );
								data << uint8(0x0c) ;
								data << uint32(pClient->getCurrentChar()->getGuidBySlot(destslot));
								data << uint32(0x00000040);
								data << uint32(pClient->getCurrentChar()->getGuidBySlot(srcslot));
								data << uint32(0x00000040);
								data << uint8(0);
								pClient->SendMsg( &data );
								return;		
							}
						}
					}
				}

				//swap items
				pClient->getCurrentChar()->SwapItemInSlot((int)srcslot, (int)destslot);
				UpdateMask updateMask;
                updateMask.setCount( PLAYER_BLOCKS );
				
				//error
				if (srcslot == destslot)
				{
					data.Initialise( 18 , SMSG_INVENTORY_CHANGE_FAILURE );
					data << uint8(0x0c) ;
					data << uint32(pClient->getCurrentChar()->getGuidBySlot(destslot));
					data << uint32(0x00000040);
					data << uint32(pClient->getCurrentChar()->getGuidBySlot(srcslot));
					data << uint32(0x00000040);
					data << uint8(0);
					pClient->SendMsg( &data );
					return;				
				}
				pClient->getCurrentChar()->updateItemStats();
				//send to zone players...they don't need to know about the item if the slot is over 19
				if (destslot < 19) {
					pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (destslot*2), pClient->getCurrentChar()->getGuidBySlot(destslot), updateMask.updateMask );
					pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (destslot*2)+1, pClient->getCurrentChar()->getGuidBySlot(destslot) == 0 ? 0 : 0x00000040, updateMask.updateMask );
				}
				if (srcslot < 19) {
					pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (srcslot*2), pClient->getCurrentChar()->getGuidBySlot(srcslot), updateMask.updateMask );
					pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (srcslot*2)+1, pClient->getCurrentChar()->getGuidBySlot(srcslot) == 0 ? 0 : 0x00000040, updateMask.updateMask );
				}

				pClient->getCurrentChar( )->UpdateObject( &updateMask, &data );
//				WorldServer::getSingleton( ).SendZoneMessage(&data, pClient, 0);
                pClient->getCurrentChar()->SendMessageToSet(&data, false);
				
				//send update to the player
				pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (destslot*2), pClient->getCurrentChar()->getGuidBySlot(destslot), updateMask.updateMask );
				pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (destslot*2)+1, pClient->getCurrentChar()->getGuidBySlot(destslot) == 0 ? 0 : 0x00000040, updateMask.updateMask );
				pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (srcslot*2), pClient->getCurrentChar()->getGuidBySlot(srcslot), updateMask.updateMask );
				pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (srcslot*2)+1, pClient->getCurrentChar()->getGuidBySlot(srcslot) == 0 ? 0 : 0x00000040, updateMask.updateMask );
				pClient->getCurrentChar( )->UpdateObject( &updateMask, &data );
				pClient->SendMsg( &data );

				if ((srcslot < 19) && (destslot < 19))
					return;

				int invcount = srcslot;
				if ((pClient->getCurrentChar()->getGuidBySlot(invcount) != 0) && (srcslot < 19))
				{
					createItemUpdate(&data, pClient, invcount);
//					world.SendZoneMessage(&data, pClient, 0);
                    pClient->getCurrentChar()->SendMessageToSet(&data, false);
				}
				
				invcount = destslot;

				if ((pClient->getCurrentChar()->getGuidBySlot(invcount) != 0) && (destslot < 19))
				{	
					createItemUpdate(&data, pClient, invcount);
//					world.SendZoneMessage(&data, pClient, 0);
                    pClient->getCurrentChar()->SendMessageToSet(&data, false);
				}
			}break;
		case CMSG_DESTROYITEM:
			{
				uint8 srcslot, destslot;
                uint32 itemguid;
				recv_data >> srcslot >> destslot;
				if (WorldServer::getSingleton().GetItem(pClient->getCurrentChar()->getItemIdBySlot(destslot)) != NULL)
					srcslot = WorldServer::getSingleton().GetItem(pClient->getCurrentChar()->getItemIdBySlot(destslot))->Inventorytype && 0xff;
				else
					return;
				if (pClient->getCurrentChar()->getGuidBySlot(destslot) == 0)
					return;
				itemguid = pClient->getCurrentChar()->getGuidBySlot(destslot);
				pClient->getCurrentChar()->AddItemToSlot(destslot,0,0);

				UpdateMask updateMask;
                updateMask.setCount( 0x1b );

				if (destslot < 19) {
					pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (destslot*2), pClient->getCurrentChar()->getGuidBySlot(destslot), updateMask.updateMask );
					pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (destslot*2)+1, pClient->getCurrentChar()->getGuidBySlot(destslot) == 0 ? 0 : 0x00000040, updateMask.updateMask );
				}
				pClient->getCurrentChar( )->UpdateObject( &updateMask, &data );
//				WorldServer::getSingleton( ).SendZoneMessage(&data, pClient, 0);
                pClient->getCurrentChar()->SendMessageToSet(&data, false);
	
				pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (destslot*2), pClient->getCurrentChar()->getGuidBySlot(destslot), updateMask.updateMask );
				pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (destslot*2)+1, pClient->getCurrentChar()->getGuidBySlot(destslot) == 0 ? 0 : 0x00000040, updateMask.updateMask );
				pClient->getCurrentChar( )->UpdateObject( &updateMask, &data );
				pClient->SendMsg( &data );

				data.clear();
				data.Initialise(8 , SMSG_DESTROY_OBJECT);
				data << itemguid << uint32(0x00000040);
//				WorldServer::getSingleton().SendZoneMessage(&data, pClient, 1);
                pClient->getCurrentChar()->SendMessageToSet(&data, true);

			}break;
		case CMSG_AUTOEQUIP_ITEM:
			{

				uint8 srcslot, destslot;
                recv_data >> srcslot >> destslot;

				int8 CharLvl, ItemLvl;
				//START OF LINA LVL REQUIREMENT AUTOEQUIP PATCH
                CharLvl=pClient->getCurrentChar()->getLevel();
				ItemLvl=WorldServer::getSingleton().GetItem(pClient->getCurrentChar()->getItemIdBySlot(destslot))->RequiredLevel;
				printf("ITEM: CharLvl %d, ItemLvl %d\n", CharLvl, ItemLvl);
				if( CharLvl < ItemLvl)
				{
					ChatHandler * MsgLvlItem = new ChatHandler;
					if( MsgLvlItem != NULL)
					{
						//NEED TO PUT SOME CODE TO UNGRAY ITEM
						uint8 buf[256];
						wowWData data;
 						sprintf((char*)buf,"You need the Lvl %d to equip that item.", ItemLvl);
						MsgLvlItem->FillMessageData(&data, 0x09, pClient, buf);
 						pClient->SendMsg( &data );
						delete(MsgLvlItem);
					}
					else printf("ITEM: CMSG_AUTOEQUIP_ITEM can't send message\n");
					return;
				}
				//END OF LINA LVL REQUIREMENT AUTOEQUIP PATCH

				if (WorldServer::getSingleton().GetItem(pClient->getCurrentChar()->getItemIdBySlot(destslot)) != NULL)
					srcslot = uint8(WorldServer::getSingleton().GetItem(pClient->getCurrentChar()->getItemIdBySlot(destslot))->Inventorytype);
				else
					return;
				
				if (WorldServer::getSingleton().GetItem(pClient->getCurrentChar()->getItemIdBySlot(destslot))->Class == 4) {
					if (srcslot < 11)
						srcslot--;
					else if (srcslot == 11) {
						if (pClient->getCurrentChar()->getGuidBySlot(10) == 0)
						{
							srcslot = 10;
						}
						else if (pClient->getCurrentChar()->getGuidBySlot(11) == 0) {

							srcslot = 11;
						}
						else {
							srcslot = destslot;
						}

					}
					else if (srcslot == 14)
						srcslot += 2;
					else if (srcslot == 13)
						srcslot += 2;
					else
						srcslot = 4;
				}
				else if (WorldServer::getSingleton().GetItem(pClient->getCurrentChar()->getItemIdBySlot(destslot))->Class == 2) {
					switch (WorldServer::getSingleton().GetItem(pClient->getCurrentChar()->getItemIdBySlot(destslot))->SubClass)
					{
						case 2: case 3: case 16: case 19:
							{
								srcslot = 17;
							}break;
					
						default:
							{
								srcslot = 15;
							}break;
					}
				}
				else {
					srcslot = destslot;
				}
				
				if (srcslot == destslot)
				{
					data.Initialise( 18 , SMSG_INVENTORY_CHANGE_FAILURE );
					data << uint8(0x0c) ;
					data << uint32(pClient->getCurrentChar()->getGuidBySlot(destslot));
					data << uint32(0x00000040);
					data << uint32(0) << uint32(0) << uint8(0);
					pClient->SendMsg( &data );
					return;
				}

				pClient->getCurrentChar()->SwapItemInSlot((int)srcslot, (int)destslot);
				UpdateMask updateMask;
                updateMask.setCount( 0x1b );
				if (destslot < 19) {
					pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (destslot*2), pClient->getCurrentChar()->getGuidBySlot(destslot), updateMask.updateMask );
					pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (destslot*2)+1, pClient->getCurrentChar()->getGuidBySlot(destslot) == 0 ? 0 : 0x00000040, updateMask.updateMask );
				}
				if (srcslot < 19) {
					pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (srcslot*2), pClient->getCurrentChar()->getGuidBySlot(srcslot), updateMask.updateMask );
					pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (srcslot*2)+1, pClient->getCurrentChar()->getGuidBySlot(srcslot) == 0 ? 0 : 0x00000040, updateMask.updateMask );
				}
				pClient->getCurrentChar( )->UpdateObject( &updateMask, &data );
//				WorldServer::getSingleton( ).SendZoneMessage(&data, pClient, 0);
                pClient->getCurrentChar()->SendMessageToSet(&data, false);

				pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (destslot*2), pClient->getCurrentChar()->getGuidBySlot(destslot), updateMask.updateMask );
				pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (destslot*2)+1, pClient->getCurrentChar()->getGuidBySlot(destslot) == 0 ? 0 : 0x00000040, updateMask.updateMask );
				pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (srcslot*2), pClient->getCurrentChar()->getGuidBySlot(srcslot), updateMask.updateMask );
				pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (srcslot*2)+1, pClient->getCurrentChar()->getGuidBySlot(srcslot) == 0 ? 0 : 0x00000040, updateMask.updateMask );
				pClient->getCurrentChar( )->UpdateObject( &updateMask, &data );
				pClient->SendMsg( &data );

				Item *tempitem;
				UpdateMask invUpdateMask;
				invUpdateMask.clear();
				int invcount = srcslot;
				invUpdateMask.setCount(0x02);
				tempitem = new Item;
				if ((pClient->getCurrentChar()->getGuidBySlot(invcount) != 0) && (srcslot < 19))
				{
					createItemUpdate(&data, pClient, invcount);
//					world.SendZoneMessage(&data, pClient, 0);
                    pClient->getCurrentChar()->SendMessageToSet(&data, false);
					delete tempitem;
				}
				else
					delete tempitem;
				invUpdateMask.clear();
				invcount = destslot;
				invUpdateMask.setCount(0x02);
				tempitem = new Item;
				if (srcslot == destslot)
					return;
				if ((pClient->getCurrentChar()->getGuidBySlot(invcount) != 0) && (destslot < 19))
				{
					createItemUpdate(&data, pClient, invcount);
//					world.SendZoneMessage(&data, pClient, 0);
                    pClient->getCurrentChar()->SendMessageToSet(&data, false);

				}
				Log::getSingleton( ).outString( "WORLD: Sent Updated Item slot Masks" );
			}break;        
	
		case CMSG_ITEM_QUERY_SINGLE:
            {
                
				int i;
				uint32 itemid, guid1, guid2;
                recv_data >> itemid >> guid1 >> guid2; // guid is the guid of the ITEM OWNER - NO ITS NOT
                if (WorldServer::getSingleton( ).GetItem(itemid) == NULL)
					return;
				sprintf( curopcodebuf, "WORLD: Recvd CMSG_ITEM_QUERY_SINGLE for item id 0x%.8X, guid 0x%.8X 0x%.8X", itemid, guid1, guid2 );
                Log::getSingleton( ).outString( curopcodebuf );
				
				Item *tempItem = WorldServer::getSingleton( ).GetItem(itemid);
				
                data.Initialise( 413 + tempItem->name1.length() + tempItem->name2.length()  + tempItem->name3.length()  + tempItem->name4.length()  + tempItem->Description.length() , SMSG_ITEM_QUERY_SINGLE_RESPONSE );
				//printf("%d\n", data.length);
				//printf("%s\n%s\n", tempItem->name1.c_str() , tempItem->Description.c_str());
				data << itemid;
				data << tempItem->Class;
				data << tempItem->SubClass;
				data << tempItem->name1.c_str();
				data << tempItem->name2.c_str();
				data << tempItem->name3.c_str();
				data << tempItem->name4.c_str();
				data << tempItem->DisplayInfoID; 
				data << tempItem->OverallQualityID; 
				data << tempItem->Flags; 
				data << tempItem->Buyprice; 
				data << tempItem->Sellprice; 
				data << tempItem->Inventorytype; 
				data << tempItem->AllowableClass; 
				data << tempItem->AllowableRace; 
				data << tempItem->ItemLevel; 
				data << tempItem->RequiredLevel; 
				data << tempItem->RequiredSkill; 
				data << tempItem->RequiredSkillRank; 
				data << tempItem->MaxCount; 
				data << tempItem->Stackable; 
				data << tempItem->ContainerSlots;
				for(i = 0; i<10; i++) 
				{
					data << tempItem->BonusStat[i];
					data << tempItem->BonusAmount[i];
				}
				for(i = 0; i<5; i++) 
				{
					data << tempItem->MinimumDamage[i]; 
					data << tempItem->MaximumDamage[i];
					data << tempItem->DamageType[i];
				}
 				for(i = 0; i<6; i++) 
				{
					data << tempItem->Resistances[i];
				}
				data << tempItem->Delay; 
				data << tempItem->AmmunitionType; 
				data << tempItem->MaxDurability; 
 				for(i = 0; i<5; i++) {
					data << tempItem->SpellID[i];
					data << tempItem->SpellTrigger[i]; 
					data << tempItem->SpellCharges[i];
					data << tempItem->SpellCooldown[i];
					data << tempItem->SpellCategory[i];
					data << tempItem->SpellCategoryCooldown[i]; 
				}
				data << tempItem->Bonding;  
				if (tempItem->Description.c_str()[0] != 48)
					data << tempItem->Description.c_str();  
				else
					data << uint32(0);
				data << tempItem->Pagetext;  
				data << tempItem->LanguageID;  
				data << tempItem->PageMaterial;  
				data << tempItem->StartQuestID;  
				data << tempItem->LockID;  
				data << tempItem->Material;  
				data << tempItem->Sheathetype;
				data << tempItem->Unknown1;
				data << tempItem->Unknown2;

                pClient->SendMsg( &data );

            }break;
		case CMSG_SELL_ITEM:
            {
                Log::getSingleton( ).outString( "World: Recieved CMSG_SELL_ITEM" );
                uint32 srcguid1, srcguid2, itemguid1, itemguid2,newmoney;
                uint8 amount;
                recv_data >> srcguid1 >> srcguid2;
                recv_data >> itemguid1 >> itemguid2;
                recv_data >> amount;
				
				if (itemguid1 == 0) {
					data.clear();
					data.Initialise(17 , SMSG_SELL_ITEM);
					data << srcguid1 << srcguid2 << itemguid1 << itemguid2 << uint8(0x01);
					pClient->SendMsg( &data );	
					return;
				}
				int itemindex = -1,i,check = 0;
				Unit *tempunit;
				tempunit = WorldServer::getSingleton( ).GetCreature(srcguid1);
				if (tempunit == NULL)
					return;

				for(i = 0; i< 39;i++)
				{
					if (pClient->getCurrentChar()->getGuidBySlot(i) == itemguid1) {
						itemindex = i;
						break;
					}
				}
				if (itemindex == -1) {
					data.clear();
					data.Initialise(17 , SMSG_SELL_ITEM);
					data << srcguid1 << srcguid2 << itemguid1 << itemguid2 << uint8(0x01);
					pClient->SendMsg( &data );	
					return; //our player doesn't have this item
				}

				
				/*****************************************************
				 *   i will need to put some stack count check here  *
                 *****************************************************/
				if (amount == 0)
					amount = 1;

				//adding this item to the vendor's item list
				for(i=0; i<tempunit->getItemCount();i++)
				{
					if (tempunit->getItemId(i) == pClient->getCurrentChar()->getItemIdBySlot(itemindex)) {
						tempunit->setItemAmount(i, tempunit->getItemAmount(i) + amount);
						check = 1;
					}
				}
				if (check == 0) {
					if (tempunit->getItemCount() > 100) {
						data.clear();
						data.Initialise(17 , SMSG_SELL_ITEM);
						data << srcguid1 << srcguid2 << itemguid1 << itemguid2 << uint8(0x02);
						pClient->SendMsg( &data );					
						return;
					}
					else
						tempunit->addItem(pClient->getCurrentChar()->getItemIdBySlot(itemindex), amount);
				}
				newmoney = WorldServer::getSingleton().GetItem(pClient->getCurrentChar()->getItemIdBySlot(itemindex))->Sellprice + pClient->getCurrentChar()->getUpdateValue(PLAYER_FIELD_COINAGE );

				//removing the item from the char's inventory
				pClient->getCurrentChar()->AddItemToSlot(itemindex,0,0);
				//sending a player update
				UpdateMask updateMask;
				updateMask.setCount( 0x1b );

				if (itemindex < 19) {
					pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (itemindex*2), pClient->getCurrentChar()->getGuidBySlot(itemindex), updateMask.updateMask );
					pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (itemindex*2)+1, pClient->getCurrentChar()->getGuidBySlot(itemindex) == 0 ? 0 : 0x00000040, updateMask.updateMask );
				}
				pClient->getCurrentChar( )->UpdateObject( &updateMask, &data );
//				WorldServer::getSingleton( ).SendZoneMessage(&data, pClient, 0);
                pClient->getCurrentChar()->SendMessageToSet(&data, false);
				
                pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_COINAGE , newmoney, updateMask.updateMask);			
				pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (itemindex*2), pClient->getCurrentChar()->getGuidBySlot(itemindex), updateMask.updateMask );
				pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (itemindex*2)+1, pClient->getCurrentChar()->getGuidBySlot(itemindex) == 0 ? 0 : 0x00000040, updateMask.updateMask );
				pClient->getCurrentChar( )->UpdateObject( &updateMask, &data );
				pClient->SendMsg( &data );

				//send an SMSG_SELL_ITEM
				data.clear();
				data.Initialise(17 , SMSG_SELL_ITEM);
				data << srcguid1 << srcguid2 << itemguid1 << itemguid2 << uint8(0x05);
				pClient->SendMsg( &data );

				//send an object destroy packet
				data.clear();
				data.Initialise(8 , SMSG_DESTROY_OBJECT);
				data << itemguid1 << uint32(0x00000040);
//				WorldServer::getSingleton().SendZoneMessage(&data, pClient, 1);
                pClient->getCurrentChar()->SendMessageToSet(&data, true);

			}break;

        case CMSG_BUY_ITEM_IN_SLOT:
            {
                Log::getSingleton( ).outString( "World: Recieved CMSG_BUY_ITEM_IN_SLOT" );
                uint32 srcguid1, srcguid2, itemid, destguid1, destguid2;
                uint8 slot, amount;
                recv_data >> srcguid1 >> srcguid2 >> itemid;
                recv_data >> destguid1 >> destguid2;
                recv_data >> slot;
                recv_data >> amount;
				int itemindex,i,varify = 0;
				Unit *tempunit;
				tempunit = WorldServer::getSingleton( ).GetCreature(srcguid1);
				if (tempunit == NULL)
					return;

				if (slot > 38)
					return;
				if (slot < 19)
					return;
				if ((slot <= 22) && (slot >=19))
					return; //these are the bags slots...i'm not sure exactly how to use them
				if (pClient->getCurrentChar()->getGuidBySlot(slot) != 0)
					return; //slot is not empty...i'll have to make code to check for other slots
				for(i = 0; i< tempunit->getItemCount();i++)
				{
					if (tempunit->getItemId(i) == itemid) 
					{
						varify = 1;
						break;
					}
				}
				if (varify == 0)
					return; //our vendor doesn't have this item
				itemindex = i;
				if (amount > tempunit->getItemAmount(i))
					return; //our vendor doesn't have the required amount of that item
				tempunit->setItemAmountById(itemid,tempunit->getItemAmount(i) - amount);
				
				//START OF LINA BUY PATCH
				printf("ARGENT: %d , COUT: %d\n", (pClient->getCurrentChar()->getUpdateValue(PLAYER_FIELD_COINAGE)), (WorldServer::getSingleton().GetItem(tempunit->getItemId(itemindex))->Buyprice)); //INFO
				int32 newmoney; 
				newmoney = ((pClient->getCurrentChar()->getUpdateValue(PLAYER_FIELD_COINAGE)) - (WorldServer::getSingleton().GetItem(tempunit->getItemId(itemindex))->Buyprice)); //LINA
				printf("DIF: %d\n",newmoney); //INFO
				if(newmoney < 0 )
				{
					ChatHandler * MsgGold = new ChatHandler;
					if( MsgGold != NULL )
					{
						uint8 buf[256];
						wowWData data;
 						sprintf((char*)buf,"You need %i	to buy this item.", abs(newmoney) );
						MsgGold->FillMessageData(&data, 0x09, pClient, buf);
 						pClient->SendMsg( &data );
						delete(MsgGold);
					}
					else printf("ITEM: CMSG_BUY_ITEM_IN_SLOT can't send message\n");
					return;
				}
				//END OF LINA BUY PATCH

				world.m_hiItemGuid++;
				std::string templog;
                char tempiid[10];
				sprintf(tempiid,"%d",world.m_hiItemGuid);
				templog = "Created Item. Guid: ";
				templog+= tempiid;

				Log::getSingleton( ).outString( templog.c_str() );

				pClient->getCurrentChar()->AddItemToSlot(slot,world.m_hiItemGuid,itemid);

				data.clear();
				data.Initialise(16, SMSG_BUY_ITEM);
				data << uint32(srcguid1) << uint32(srcguid2);
				data << uint32(itemid) << uint32(amount);
				pClient->SendMsg( &data );
				
				Item *tempitem;
				
				UpdateMask invUpdateMask;
				
				invUpdateMask.clear();
				int invcount = slot;
				invUpdateMask.setCount(0x02);
				tempitem = new Item;
	
				createItemUpdate(&data, pClient, invcount);

				if (slot > 23)
					pClient->SendMsg( &data );
				else {
//					world.SendZoneMessage(&data, pClient, 0);
                    pClient->getCurrentChar()->SendMessageToSet(&data, true);
//					pClient->SendMsg( &data );
				}
				delete tempitem;

				UpdateMask updateMask;
				updateMask.setCount( 0x1b );
				pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_COINAGE , newmoney, updateMask.updateMask);	//LINA BUY PATCH NEXT		
				pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (slot*2), pClient->getCurrentChar()->getGuidBySlot(slot), updateMask.updateMask );
				pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (slot*2)+1, pClient->getCurrentChar()->getGuidBySlot(slot) == 0 ? 0 : 0x00000040, updateMask.updateMask );
				pClient->getCurrentChar( )->UpdateObject( &updateMask, &data );
				if (slot > 23)
					pClient->SendMsg( &data );
				else {
//					world.SendZoneMessage(&data, pClient, 0);
                    pClient->getCurrentChar()->SendMessageToSet(&data, true);
//					pClient->SendMsg( &data );
				}
			}break;
        case CMSG_BUY_ITEM: //right click
            {
                Log::getSingleton( ).outString( "World: Recieved CMSG_BUY_ITEM" );
				uint32 srcguid1, srcguid2, itemid;
                uint8 slot, amount;
                recv_data >> srcguid1 >> srcguid2 >> itemid;
                recv_data >> amount >> slot;
				int itemindex,i,varify = 0;
				Unit *tempunit;
				tempunit = WorldServer::getSingleton( ).GetCreature(srcguid1);
				if (tempunit == NULL)
					return;
				slot = 0;
				for(i = 23; i <= 38; i++) 
				{
					if (pClient->getCurrentChar()->getGuidBySlot(i) == 0) {
						slot = i;
						break;
					}
				}
				if (slot == 0)
					return;
				for(i = 0; i< tempunit->getItemCount();i++)
				{
					if (tempunit->getItemId(i) == itemid) {
						varify = 1;
						break;
					}
				}
				if (varify == 0)
					return; //our vendor doesn't have this item
				itemindex = i;
				if (amount > tempunit->getItemAmount(i))
					return; //our vendor doesn't have the required amount of that item
				tempunit->setItemAmountById(itemid,tempunit->getItemAmount(i) - amount);

				//START OF LINA BUY PATCH
				printf("ARGENT: %d , COUT: %d\n", (pClient->getCurrentChar()->getUpdateValue(PLAYER_FIELD_COINAGE)), (WorldServer::getSingleton().GetItem(tempunit->getItemId(itemindex))->Buyprice)); //INFO
				int32 newmoney; 
				newmoney = ((pClient->getCurrentChar()->getUpdateValue(PLAYER_FIELD_COINAGE)) - (WorldServer::getSingleton().GetItem(tempunit->getItemId(itemindex))->Buyprice)); //LINA
				printf("DIF: %d\n",newmoney); //INFO
				if(newmoney < 0 )
				{
					//NEED TO PUT SOME CODE TO UNGRAY ITEM AND SEND A MESSAGE TO PLAYER
					ChatHandler * MsgGold = new ChatHandler;
					if( MsgGold != NULL )
					{
						uint8 buf[256];
						wowWData data;
 						sprintf((char*)buf,"You need %i	to buy this item.", abs(newmoney) );
						MsgGold->FillMessageData(&data, 0x09, pClient, buf);
 						pClient->SendMsg( &data );
						delete(MsgGold);
					}
					else printf("ITEM: CMSG_BUY_ITEM can't send message\n");
					return;
				}
				//END OF LINA BUY PATCH

				world.m_hiItemGuid++;
				std::string templog;
                char tempiid[10];
				sprintf(tempiid,"%d",world.m_hiItemGuid);
				templog = "Created Item. Guid: ";
				templog+= tempiid;

				Log::getSingleton( ).outString( templog.c_str() );

				pClient->getCurrentChar()->AddItemToSlot(slot,world.m_hiItemGuid,itemid);
				
				
				UpdateMask invUpdateMask;
				
				invUpdateMask.clear();
				int invcount = slot;
				invUpdateMask.setCount(0x02);
	
				createItemUpdate(&data, pClient, invcount);

				if (slot > 23)
					pClient->SendMsg( &data );
				else {
//					world.SendZoneMessage(&data, pClient, 0);
                    pClient->getCurrentChar()->SendMessageToSet(&data, true);
//					pClient->SendMsg( &data );
				}

				UpdateMask updateMask;
				updateMask.setCount( 0x1b );
				pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_COINAGE , newmoney, updateMask.updateMask);	//LINA BUY PATCH NEXT		
				pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (slot*2), pClient->getCurrentChar()->getGuidBySlot(slot), updateMask.updateMask );
				pClient->getCurrentChar( )->setUpdateValue( PLAYER_FIELD_INV_SLOT_HEAD  + (slot*2)+1, pClient->getCurrentChar()->getGuidBySlot(slot) == 0 ? 0 : 0x00000040, updateMask.updateMask );
				pClient->getCurrentChar( )->UpdateObject( &updateMask, &data );
				if (slot > 23)
					pClient->SendMsg( &data );
				else {
//					world.SendZoneMessage(&data, pClient, 0);
                    pClient->getCurrentChar()->SendMessageToSet(&data, false);
//					pClient->SendMsg( &data );
				}
			}break;
			
        case CMSG_LIST_INVENTORY:
            {
                Log::getSingleton( ).outString( "WORLD: Recvd CMSG_LIST_INVENTORY Message" );
                uint32 guid1, guid2;
				//guid1+guid2 = npc's full uint64 guid
				
                recv_data >> guid1 >> guid2;
				Unit *tempunit;
				tempunit = WorldServer::getSingleton( ).GetCreature(guid1);
				if (tempunit == NULL)
					return;
                uint8 numitems = (uint8)tempunit->getItemCount();

				if (numitems == 0)
					return;

                data.Initialise( 8 + 1 + numitems * 7 * 4, SMSG_LIST_INVENTORY );
                data << guid1 << guid2;
                data << uint8( numitems ); // num items

                // each item has seven uint32's
                Item * curItem;
                for(uint8 itemcount = 0; itemcount < numitems; itemcount ++ ) {
                    curItem = WorldServer::getSingleton( ).GetItem(tempunit->getItemId(itemcount));
                    if( !curItem ) {
                        Log::getSingleton( ).outError( "Unit %i has nonexistant item %i!", guid1, tempunit->getItemId(itemcount) );
                        Log::getSingleton( ).outString( "WORLD: DID NOT Send SMSG_LIST_INVENTORY Message" );
                        for( int a = 0; a < 7; a ++ ) data << uint32( 0 );
                    } else {
                        data << uint32( itemcount + 1 ); // index ? doesn't seem to affect anything
                        data << uint32( tempunit->getItemId(itemcount) ); // item id
                        data << uint32( curItem->DisplayInfoID ); // item icon
                        data << uint32( tempunit->getItemAmount(itemcount) ); // number of items available, -1 works for infinity, although maybe just 'cause it's really big
                        data << uint32( curItem->Buyprice ); // price
                        data << uint32( 0 ); // ?
                        data << uint32( 0 ); // ?
                    }
                }

                //data.writeData( tdata, sizeof( tdata ) );
                pClient->SendMsg( &data );
                Log::getSingleton( ).outString( "WORLD: Sent SMSG_LIST_INVENTORY Message" );
            }break;
    }
}


void ItemHandler::createItemUpdate( wowWData *data, GameClient *pClient, int invcount)
{
				UpdateMask invUpdateMask;
				
				invUpdateMask.clear();				
				invUpdateMask.setCount(0x02);
				Item *tempitem = new Item;
	
				invUpdateMask.setBit( OBJECT_FIELD_GUID );
				invUpdateMask.setBit( OBJECT_FIELD_GUID+1 );
				invUpdateMask.setBit( OBJECT_FIELD_TYPE );
				invUpdateMask.setBit( OBJECT_FIELD_ENTRY );
				invUpdateMask.setBit( OBJECT_FIELD_SCALE_X );
				invUpdateMask.setBit( OBJECT_FIELD_PADDING );
				invUpdateMask.setBit( ITEM_FIELD_OWNER );
				invUpdateMask.setBit( ITEM_FIELD_CONTAINED );
				invUpdateMask.setBit( ITEM_FIELD_OWNER +1 );
				invUpdateMask.setBit( ITEM_FIELD_CONTAINED +1 );
				invUpdateMask.setBit( ITEM_FIELD_STACK_COUNT );
				tempitem->Create(pClient->getCurrentChar()->getGuidBySlot(invcount),pClient->getCurrentChar()->getItemIdBySlot(invcount));
				tempitem->setUpdateValue( OBJECT_FIELD_GUID , pClient->getCurrentChar()->getGuidBySlot(invcount) ,invUpdateMask.updateMask);
				tempitem->setUpdateValue( OBJECT_FIELD_GUID+1 , 0x00000040 ,invUpdateMask.updateMask);
				tempitem->setUpdateValue( OBJECT_FIELD_TYPE , 0x00000003 ,invUpdateMask.updateMask);
				tempitem->setUpdateValue( OBJECT_FIELD_ENTRY , pClient->getCurrentChar()->getItemIdBySlot(invcount) ,invUpdateMask.updateMask);
				tempitem->setUpdateFloatValue( OBJECT_FIELD_SCALE_X , 1.0f ,invUpdateMask.updateMask);
				tempitem->setUpdateValue( OBJECT_FIELD_PADDING , 0xeeeeeeee ,invUpdateMask.updateMask);
				tempitem->setUpdateValue( ITEM_FIELD_OWNER , pClient->getCurrentChar()->getGUID() ,invUpdateMask.updateMask);
				tempitem->setUpdateValue( ITEM_FIELD_CONTAINED , pClient->getCurrentChar()->getGUID() ,invUpdateMask.updateMask);
				tempitem->setUpdateValue( ITEM_FIELD_OWNER +1 , 0 ,invUpdateMask.updateMask);
				tempitem->setUpdateValue( ITEM_FIELD_CONTAINED +1 , 0 ,invUpdateMask.updateMask);
				tempitem->setUpdateValue( ITEM_FIELD_STACK_COUNT , 1 ,invUpdateMask.updateMask);
				
				tempitem->CreateObject(&invUpdateMask, data, 0);
}

