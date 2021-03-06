#include "z_item_inbox.h"

#define FLAGS 0x00000009

#define THIS ((ItemInbox*)thisx)

void ItemInbox_Init(Actor* thisx, GlobalContext* globalCtx);
void ItemInbox_Destroy(Actor* thisx, GlobalContext* globalCtx);
void ItemInbox_Update(Actor* thisx, GlobalContext* globalCtx);
void ItemInbox_Draw(Actor* thisx, GlobalContext* globalCtx);

/*
const ActorInit Item_Inbox_InitVars = {
    ACTOR_ITEM_INBOX,
    ACTORCAT_NPC,
    FLAGS,
    GAMEPLAY_KEEP,
    sizeof(ItemInbox),
    (ActorFunc)ItemInbox_Init,
    (ActorFunc)ItemInbox_Destroy,
    (ActorFunc)ItemInbox_Update,
    (ActorFunc)ItemInbox_Draw
};
*/

#pragma GLOBAL_ASM("./asm/non_matchings/overlays/ovl_Item_Inbox_0x809454F0/ItemInbox_Init.asm")

#pragma GLOBAL_ASM("./asm/non_matchings/overlays/ovl_Item_Inbox_0x809454F0/ItemInbox_Destroy.asm")

#pragma GLOBAL_ASM("./asm/non_matchings/overlays/ovl_Item_Inbox_0x809454F0/func_80945534.asm")

#pragma GLOBAL_ASM("./asm/non_matchings/overlays/ovl_Item_Inbox_0x809454F0/ItemInbox_Update.asm")

#pragma GLOBAL_ASM("./asm/non_matchings/overlays/ovl_Item_Inbox_0x809454F0/ItemInbox_Draw.asm")
