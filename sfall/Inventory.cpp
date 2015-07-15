/*
 *    sfall
 *    Copyright (C) 2011  Timeslip
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "main.h"
#include "Inventory.h"
#include "HookScripts.h"
#include "FalloutEngine.h"
#include "LoadGameHook.h"

#include <stdio.h>

static DWORD mode;
static DWORD MaxItemSize;
static DWORD ReloadWeaponKey = 0;

struct sMessage {
	DWORD number;
	DWORD flags;
	char* audio;
	char* message;
};
static const DWORD message_search=0x484C30;
static const char* MsgSearch(int msgno, DWORD file) {
	if(!file) return 0;
	sMessage msg = { msgno, 0, 0, 0 };
	__asm {
		lea edx, msg;
		mov eax, file;
		call message_search;
	}
	return msg.message;
}

DWORD& GetActiveItemMode() {
	return itemButtonItems[(*itemCurrentItem * 6) + 4];
}

TGameObj* GetActiveItem() {
	return (TGameObj*)itemButtonItems[*itemCurrentItem * 6];
}

void InventoryKeyPressedHook(DWORD dxKey, bool pressed, DWORD vKey) {
	if (pressed && ReloadWeaponKey && dxKey == ReloadWeaponKey && (GetCurrentLoops() & ~(COMBAT | PCOMBAT)) == 0) {
		DWORD maxAmmo, curAmmo;
		TGameObj* item = GetActiveItem();
		__asm {
			mov eax, item;
			call item_w_max_ammo_;
			mov maxAmmo, eax;
			mov eax, item;
			call item_w_cur_ammo_;
			mov curAmmo, eax;
		}
		if (maxAmmo != curAmmo) {
			DWORD &currentMode = GetActiveItemMode();
			DWORD previusMode = currentMode;
			currentMode = 5; // reload mode
			__asm {
				call intface_use_item_;
			}
			if (previusMode != 5) {
				// return to previous active item mode (if it wasn't "reload")
				currentMode = previusMode - 1;
				if (currentMode < 0)
					currentMode = 4;
				__asm {
					call intface_toggle_item_state_;
				}
			}
		}
	}
}

static const DWORD item_total_weight=0x477E98;
static const DWORD item_size=0x477B68;

/*static DWORD _stdcall item_total_size(void* critter) {
	//TODO: Don't really want to be overwriting stuff like this after init. Rewrite properly.
	HookCall(0x477EBD, (void*)0x477B68);
	HookCall(0x477EF6, (void*)0x477B68);
	HookCall(0x477F12, (void*)0x477B68);
	HookCall(0x477F2A, (void*)0x477B68);

	DWORD result;
	__asm {
		mov eax, critter;
		call item_total_weight;
		mov result, eax;
	}

	HookCall(0x477EBD, (void*)0x477B88);
	HookCall(0x477EF6, (void*)0x477B88);
	HookCall(0x477F12, (void*)0x477B88);
	HookCall(0x477F2A, (void*)0x477B88);

	return result;
}*/

static const DWORD inven_right_hand=0x471B70;
static const DWORD inven_left_hand=0x471BBC;
static const DWORD inven_worn=0x471C08; //TODO: Do we actually want to include this in the limit anyway?
static __declspec(naked) DWORD item_total_size(void* critter) {
	__asm {
		push    ebx;
		push    ecx;
		push    edx;
		push    esi;
		push    edi;
		push    ebp;
		mov     ebp, eax;
		test    eax, eax;
		jz      loc_477F33;
		lea     edi, [eax+2Ch];
		xor     edx, edx;
		mov     ebx, [edi];
		xor     esi, esi;
		test    ebx, ebx;
		jle     loc_477ED3;
		xor     ebx, ebx;
loc_477EB7:
		mov     ecx, [edi+8];
		mov     eax, [ecx+ebx];
		call    item_size;
		imul    eax, [ecx+ebx+4];
		add     ebx, 8;
		inc     edx;
		mov     ecx, [edi];
		add     esi, eax;
		cmp     edx, ecx;
		jl      loc_477EB7;
loc_477ED3:
		mov     eax, [ebp+20h];
		and     eax, 0F000000h;
		sar     eax, 18h;
		cmp     eax, 1;
		jnz     loc_477F31;
		mov     eax, ebp;
		call    inven_right_hand;
		mov     edx, eax;
		test    eax, eax;
		jz      loc_477EFD;
		test    byte ptr [eax+27h], 2;
		jnz     loc_477EFD;
		call    item_size;
		add     esi, eax;
loc_477EFD:
		mov     eax, ebp;
		call    inven_left_hand;
		test    eax, eax;
		jz      loc_477F19;
		cmp     edx, eax;
		jz      loc_477F19;
		test    byte ptr [eax+27h], 1;
		jnz     loc_477F19;
		call    item_size;
		add     esi, eax;
loc_477F19:
		mov     eax, ebp;
		call    inven_worn;
		test    eax, eax;
		jz      loc_477F31;
		test    byte ptr [eax+27h], 4;
		jnz     loc_477F31;
		call    item_size;
		add     esi, eax;
loc_477F31:
		mov     eax, esi;
loc_477F33:
		pop     ebp;
		pop     edi;
		pop     esi;
		pop     edx;
		pop     ecx;
		pop     ebx;
		retn;
	}
}

/*static const DWORD ObjPickupFail=0x49B70D;
static const DWORD ObjPickupEnd=0x49B6F8;
static const DWORD size_limit;
static __declspec(naked) void  ObjPickupHook() {
	__asm {
		cmp edi, [0x6610B8];
		jnz end;
end:
		lea edx, [esp+0x10];
		mov eax, ecx;
		jmp ObjPickupEnd;
	}
}*/

static __declspec(naked) int CritterCheck() {
	__asm {
		push ebx;
		push edx;
		sub esp, 4;
		mov ebx, eax;

		cmp eax, dword ptr ds:[0x6610B8];
		je single;
		test mode, 3;
		jnz run;
		test mode, 2;
		jz fail;
		call isPartyMember_;
		test eax, eax;
		jz end;
run:
		test mode, 8;
		jz single;
		mov edx, esp;
		mov eax, ebx;
		call proto_ptr_;
		mov eax, [esp];
		mov eax, [eax + 0xB0 + 40]; //The unused stat in the extra block
		jmp end;
single:
		mov eax, MaxItemSize;
		jmp end;
fail:
		xor eax, eax;
end:
		add esp, 4;
		pop edx;
		pop ebx;
		retn;
	}
}

static const DWORD IsOverloadedEnd=0x42E68D;
static __declspec(naked) void CritterIsOverloadedHook() {
	__asm {
		and eax, 0xff;
		jnz end;
		mov eax, ebx;
		call CritterCheck;
		test eax, eax;
		jz end;
		xchg eax, ebx;
		call item_total_size;
		cmp eax, ebx;
		setg al;
		and eax, 0xff;
end:
		jmp IsOverloadedEnd;
	}
}

static const DWORD ItemAddMultiRet=0x4772A6;
static const DWORD ItemAddMultiFail=0x4771C7;
static __declspec(naked) void ItemAddMultiHook1() {
	__asm {
		push ebp;
		mov eax, ecx;
		call CritterCheck;
		test eax, eax;
		jz end;
		mov ebp, eax;
		mov eax, esi;
		call item_size;
		mov edx, eax;
		imul edx, ebx;
		mov eax, ecx;
		call item_total_size;
		add edx, eax;
		cmp edx, ebp;
		jle end;
		mov eax, -6; //TODO: Switch this to a lower number, and add custom error messages.
		pop ebp;
		jmp ItemAddMultiFail;
end:
		pop ebp;
		jmp ItemAddMultiRet;
	}
}

static __declspec(naked) void ItemAddMultiHook2() {
	__asm {
		cmp eax, edi;
		jl fail;
		jmp ItemAddMultiHook1;
fail:
		mov eax, -6;
		jmp ItemAddMultiFail;
	}
}

static const DWORD BarterAttemptTransactionHook1Fail=0x474C81;
static const DWORD BarterAttemptTransactionHook1End=0x474CA8;
static __declspec(naked) void BarterAttemptTransactionHook1() {
	__asm {
		cmp eax, edx;
		jg fail;
		mov eax, edi;
		call CritterCheck;
		test eax, eax;
		jz end;
		mov edx, eax;
		mov eax, edi;
		call item_total_size;
		sub edx, eax;
		mov eax, ebp;
		call item_total_size;
		cmp eax, edx;
		jle end;
fail:
		mov esi, 0x1f;
		jmp BarterAttemptTransactionHook1Fail;
end:
		jmp BarterAttemptTransactionHook1End;
	}
}

static const DWORD BarterAttemptTransactionHook2Fail=0x474CD8;
static const DWORD BarterAttemptTransactionHook2End=0x474D01;
static __declspec(naked) void BarterAttemptTransactionHook2() {
	__asm {
		cmp eax, edx;
		jg fail;
		mov eax, ebx;
		call CritterCheck;
		test eax, eax;
		jz end;
		mov edx, eax;
		mov eax, ebx;
		call item_total_size;
		sub edx, eax;
		mov eax, esi;
		call item_total_size;
		cmp eax, edx;
		jle end;
fail:
		mov ecx, 0x20;
		jmp BarterAttemptTransactionHook2Fail;
end:
		jmp BarterAttemptTransactionHook2End;
	}
}

static char SizeStr[16];
static char InvenFmt[32];
static const char* InvenFmt1="%s %d/%d  %s %d/%d";
static const char* InvenFmt2="%s %d/%d";

static const char* _stdcall GetInvenMsg() {
	const char* tmp=MsgSearch(35, 0x59E814);
	if(!tmp) return "S:";
	else return tmp;
}

static void _stdcall strcpy_wrapper(char* buf, const char* str) {
	strcpy(buf, str);
}

static const DWORD DisplayStatsEnd=0x4725E5;
static __declspec(naked) void DisplayStatsHook() {
	__asm {
		call CritterCheck;
		jz nolimit;
		push eax;
		mov eax, ds:[0x59E86C];
		push ecx;
		push InvenFmt1;
		push offset InvenFmt;
		call strcpy_wrapper;
		pop ecx;
		mov eax, ds:[0x59E86C];
		call item_total_size;
		push eax;
		push ecx;
		call GetInvenMsg;
		pop ecx;
		push eax;
		jmp end;
nolimit:
		push ecx;
		push InvenFmt2;
		push offset InvenFmt;
		call strcpy_wrapper;
		pop ecx;
		push eax;
		push eax;
		push eax;
end:
		mov eax, ds:[0x59E86C];
		mov edx, 0xc;
		jmp DisplayStatsEnd;
	}
}

static char SizeMsgBuf[32];
static const char* _stdcall FmtSizeMsg(int size) {
	if(size==1) {
		const char* tmp=MsgSearch(543, 0x6647FC);
		if(!tmp) strcpy(SizeMsgBuf, "It occupies 1 unit.");
		else sprintf(SizeMsgBuf, tmp, size);
	} else {
		const char* tmp=MsgSearch(542, 0x6647FC);
		if(!tmp) sprintf(SizeMsgBuf, "It occupies %d units.", size);
		else sprintf(SizeMsgBuf, tmp, size);
	}
	return SizeMsgBuf;
}

static const DWORD InvenDisplayMsg=0x472D24;
static __declspec(naked) void InvenObjExamineFuncHook() {
	__asm {
		call InvenDisplayMsg;
		push edx;
		push ecx;
		mov eax, esi;
		call item_size;
		push eax;
		call FmtSizeMsg;
		pop ecx;
		pop edx;
		call InvenDisplayMsg;
		retn;
	}
}

static char SuperStimMsg[128];
static int _stdcall SuperStimFix2(DWORD* item, DWORD* target) {
	if(!item || !target) return 0;
	DWORD itm_pid=item[0x64/4], target_pid=target[0x64/4];
	if((target_pid&0xff000000) != 0x01000000) return 0;
	if((itm_pid&0xff000000) != 0) return 0;
	if((itm_pid&0xffffff) != 144) return 0;
	static const DWORD _stat_level=0x4AEF48;
	DWORD curr_hp, max_hp;
	__asm {
		mov eax, target;
		mov edx, 35;
		call _stat_level;
		mov curr_hp, eax;
		mov eax, target;
		mov edx, 7;
		call _stat_level;
		mov max_hp, eax;
	}
	if(curr_hp<max_hp) return 0;
	DisplayConsoleMessage(SuperStimMsg);
	return 1;
}

static const DWORD UseItemHookRet=0x49C3D3;
static void __declspec(naked) SuperStimFix() {
	__asm {
		push eax;
		push ecx;
		push edx;

		push edx;
		push ebx;
		call SuperStimFix2;
		pop edx;
		pop ecx;
		test eax, eax;
		jz end;
		pop eax;
		xor eax, eax;
		retn;
end:
		pop eax;
		push ecx;
		push esi;
		push edi;
		push ebp;
		sub esp, 0x14;
		jmp UseItemHookRet;
	}
}

static int invenapcost;
static char invenapqpreduction;
void _stdcall SetInvenApCost(int a) { invenapcost=a; }
static const DWORD inven_ap_cost_hook_ret=0x46E816;
static void __declspec(naked) inven_ap_cost_hook() {
	_asm {
		movzx ebx, byte ptr invenapqpreduction;
		mul bl;
		mov edx, invenapcost;
		sub edx, eax;
		mov eax, edx;
		jmp inven_ap_cost_hook_ret;
	}
}

static const DWORD item_w_compute_ammo_cost_ = 0x4790AC; // signed int aWeapon<eax>, int *aRoundsSpent<edx>
static const DWORD add_check_for_item_ammo_cost_back = 0x4266EE;
// adds check for weapons which require more than 1 ammo for single shot (super cattle prod & mega power fist)
static void __declspec(naked) add_check_for_item_ammo_cost() {
	__asm {
		push    edx
		push    ebx
		sub     esp, 4
		call    item_w_cur_ammo_
		mov     ebx, eax
		mov     eax, ecx // weapon
		mov     edx, esp
		mov     dword ptr [esp], 1
		pushad
		push    1 // hook type
		call    AmmoCostHookWrapper
		add		esp, 4
		popad
		mov     eax, [esp]
		cmp     eax, ebx
		jle     enoughammo
		xor     eax, eax // this will force "Out of ammo"
		jmp     end
enoughammo:
		mov     eax, 1 // this will force success
end:
		add     esp, 4
		pop     ebx
		pop     edx
		jmp     add_check_for_item_ammo_cost_back; // jump back
	}
}

static const DWORD divide_burst_rounds_by_ammo_cost_back = 0x4234B9;
static void __declspec(naked) divide_burst_rounds_by_ammo_cost() {
	__asm {
		// ecx - current ammo, eax - burst rounds; need to set ebp
		push edx
		sub     esp, 4
		mov     ebp, eax
		mov     eax, edx // weapon
		mov     dword ptr [esp], 1
		mov     edx, esp // *rounds
		pushad
		push    2
		call    AmmoCostHookWrapper
		add		esp, 4
		popad
		mov     edx, 0
		mov     eax, ebp // rounds in burst
		imul    dword ptr [esp] // so much ammo is required for this burst
		cmp     eax, ecx
		jle     skip
		mov     eax, ecx // if more than current ammo, set it to current
skip:
		idiv    dword ptr [esp] // divide back to get proper number of rounds for damage calculations
		mov     ebp, eax
		add     esp, 4
		pop edx
		// end overwriten code
		jmp     divide_burst_rounds_by_ammo_cost_back; // jump back
	}
}

static void __declspec(naked) SetDefaultAmmo() {
	__asm {
		push    eax
		push    ebx
		push    edx
		xchg    eax, edx
		mov     ebx, eax
		call    item_get_type_
		cmp     eax, 3 // is it item_type_weapon?
		jne     end // no
		cmp     dword ptr [ebx+0x3C], 0 // is there any ammo in the weapon?
		jne     end // yes
		sub     esp, 4
		mov     edx, esp
		mov     eax, [ebx+0x64] // eax = weapon pid
		call    proto_ptr_
		mov     edx, [esp]
		mov     eax, [edx+0x5C] // eax = default ammo pid
		mov     [ebx+0x40], eax // set current ammo proto
		add     esp, 4
end:
		pop     edx
		pop     ebx
		pop     eax
		retn
	}
}

static const DWORD inven_action_cursor_hook_End = 0x4736CB;
static void __declspec(naked) inven_action_cursor_hook() {
	__asm {
		mov     edx, [esp+0x1C]
		call    SetDefaultAmmo
		cmp     dword ptr [esp+0x18], 0
		jmp     inven_action_cursor_hook_End
	}
}

static const DWORD item_add_force_ = 0x4772B8;
static void __declspec(naked) item_add_mult_hook() {
	__asm {
		call    SetDefaultAmmo
		call    item_add_force_
		retn
	}
}

void InventoryInit() {
	mode=GetPrivateProfileInt("Misc", "CritterInvSizeLimitMode", 0, ini);
	invenapcost=GetPrivateProfileInt("Misc", "InventoryApCost", 4, ini);
	invenapqpreduction=GetPrivateProfileInt("Misc", "QuickPocketsApCostReduction", 2, ini);
	MakeCall(0x46E80B, inven_ap_cost_hook, true);
	if(mode>7) mode=0;
	if(mode>=4) {
		mode-=4;
		SafeWrite8(0x477EB3, 0xeb);
	}
	if(mode) {
		MaxItemSize=GetPrivateProfileInt("Misc", "CritterInvSizeLimit", 100, ini);

		//Check item_add_multi (picking stuff from the floor, etc.)
		HookCall(0x4771BD, &ItemAddMultiHook1);
		MakeCall(0x47726D, &ItemAddMultiHook2, true);
		MakeCall(0x42E688, &CritterIsOverloadedHook, true);

		//Check capacity of player and barteree when bartering
		MakeCall(0x474C78, &BarterAttemptTransactionHook1, true);
		MakeCall(0x474CCF, &BarterAttemptTransactionHook2, true);

		//Display total weight on the inventory screen
		SafeWrite32(0x4725FF, (DWORD)&InvenFmt);
		MakeCall(0x4725E0, &DisplayStatsHook, true);
		SafeWrite8(0x47260F, 0x20);
		SafeWrite32(0x4725F9, 0x9c+0xc);
		SafeWrite8(0x472606, 0x10+0xc);
		SafeWrite32(0x472632, 150);
		SafeWrite8(0x472638, 0);

		//Display item weight when examening
		HookCall(0x472FFE, &InvenObjExamineFuncHook);
	}

	if(GetPrivateProfileInt("Misc", "SuperStimExploitFix", 0, ini)) {
		GetPrivateProfileString("sfall", "SuperStimExploitMsg", "You cannot use a super stim on someone who is not injured!", SuperStimMsg, 128, translationIni);
		MakeCall(0x49C3CC, SuperStimFix, true);
	}

	if(GetPrivateProfileInt("Misc", "CheckWeaponAmmoCost", 0, ini)) {
		MakeCall(0x4266E9, &add_check_for_item_ammo_cost, true);
		MakeCall(0x4234B3, &divide_burst_rounds_by_ammo_cost, true);
	}

	ReloadWeaponKey = GetPrivateProfileInt("Input", "ReloadWeaponKey", 0, ini);

	if(GetPrivateProfileIntA("Misc", "StackEmptyWeapons", 0, ini)) {
		MakeCall(0x4736C6, &inven_action_cursor_hook, true);
		HookCall(0x4772AA, &item_add_mult_hook);
	}
}
void InventoryReset() {
	invenapcost=GetPrivateProfileInt("Misc", "InventoryApCost", 4, ini);
}