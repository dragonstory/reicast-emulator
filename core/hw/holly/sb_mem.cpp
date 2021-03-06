#include "types.h"
#include "hw/sh4/sh4_mem.h"

#include "sb_mem.h"
#include "sb.h"
#include "hw/pvr/pvr_regs.h"
#include "hw/pvr/pvr_mem.h"
#include "hw/gdrom/gdrom_if.h"
#include "hw/aica/aica.h"
//#include "hw/aica/aica_if.h"
#include "hw/naomi/naomi.h"

#include "hw/flashrom/flashrom.h"
#include "reios/reios.h"


static HollyInterruptID dmatmp1;
static HollyInterruptID dmatmp2;
static HollyInterruptID OldDmaId;

/*
	Dreamcast 'area 0' emulation
	Pretty much all peripheral registers are mapped here

	Routing is mostly handled here, as well as flash/SRAM emulation
*/

RomChip sys_rom;
SRamChip sys_nvmem_sram;
DCFlashChip sys_nvmem_flash;

extern char nvmem_file[PATH_MAX];

static const char *get_rom_prefix(void)
{
   switch (settings.System)
   {
      case DC_PLATFORM_DREAMCAST:
         return "dc_";
      case DC_PLATFORM_DEV_UNIT:
         return "hkt_";
      case DC_PLATFORM_NAOMI:
         return "naomi_";
      case DC_PLATFORM_NAOMI2:
         return "n2_";
      case DC_PLATFORM_ATOMISWAVE:
         return "aw_";
   }
   return "";
}

static const char *get_rom_names(void)
{
   switch (settings.System)
   {
      case DC_PLATFORM_DREAMCAST:
      case DC_PLATFORM_DEV_UNIT:
         return "%boot.bin;%boot.bin.bin;%bios.bin;%bios.bin.bin";
      case DC_PLATFORM_NAOMI:
         return "%boot.bin;%boot.bin.bin;%bios.bin;%bios.bin.bin;epr-21576d.bin";
      case DC_PLATFORM_NAOMI2:
         return "%boot.bin;%boot.bin.bin;%bios.bin;%bios.bin.bin";
      case DC_PLATFORM_ATOMISWAVE:
         return "%boot.bin;%boot.bin.bin;%bios.bin;%bios.bin.bin;bios.ic23_l";
   }

   return NULL; 
}

static bool nvmem_load(const string& root,
      const string& s1, const char *s2)
{
   switch (settings.System)
   {
      case DC_PLATFORM_DREAMCAST:
      case DC_PLATFORM_DEV_UNIT:
      case DC_PLATFORM_ATOMISWAVE:
         return sys_nvmem_flash.Load(root, get_rom_prefix(), s1.c_str(), s2);
      case DC_PLATFORM_NAOMI:
      case DC_PLATFORM_NAOMI2:
         return sys_nvmem_sram.Load(nvmem_file);
   }

   return false;
}

static bool nvr_is_optional(void)
{
   switch (settings.System)
   {
      case DC_PLATFORM_ATOMISWAVE:
      case DC_PLATFORM_NAOMI:
      case DC_PLATFORM_NAOMI2:
         return true;
      case DC_PLATFORM_DREAMCAST:
      case DC_PLATFORM_DEV_UNIT:
         break;
   }

   return false;
}


bool LoadRomFiles(const string& root)
{
	if (!sys_rom.Load(root, get_rom_prefix(), get_rom_names(), "bootrom"))
	{
		msgboxf("Unable to find bios in \n%s\nExiting...", MBX_ICONERROR, root.c_str());
		return false;
	}
   if (!nvmem_load(root, "%nvmem.bin;%flash_wb.bin;%flash.bin;%flash.bin.bin", "nvram"))
	{
		if (nvr_is_optional())
		{
			printf("flash/nvmem is missing, will create new file...");
		}
		else
		{
			msgboxf("Unable to find flash/nvmem in \n%s\nExiting...", MBX_ICONERROR, root.c_str());
			return false;
		}
	}

	return true;
}

void SaveRomFiles(const string& root)
{
   switch (settings.System)
   {
      case DC_PLATFORM_DREAMCAST:
      case DC_PLATFORM_DEV_UNIT:
      case DC_PLATFORM_ATOMISWAVE:
         sys_nvmem_flash.Save(root, get_rom_prefix(), "nvmem.bin", "nvmem");
         break;
      case DC_PLATFORM_NAOMI:
      case DC_PLATFORM_NAOMI2:
         sys_nvmem_sram.Save(nvmem_file);
         break;
   }
}

u8 *get_nvmem_data(void)
{
   switch (settings.System)
   {
      case DC_PLATFORM_DREAMCAST:
      case DC_PLATFORM_DEV_UNIT:
      case DC_PLATFORM_ATOMISWAVE:
         return sys_nvmem_flash.data;
      case DC_PLATFORM_NAOMI:
      case DC_PLATFORM_NAOMI2:
         return sys_nvmem_sram.data;
   }

   return NULL;
}

bool LoadHle(const string& root) {
	if (!nvmem_load(root, "%nvmem.bin;%flash_wb.bin;%flash.bin;%flash.bin.bin", "nvram")) {
		printf("No nvmem loaded\n");
	}

	return reios_init(sys_rom.data, get_nvmem_data());
}

u32 ReadFlash(u32 addr,u32 sz)
{
   switch (settings.System)
   {
      case DC_PLATFORM_DREAMCAST:
      case DC_PLATFORM_DEV_UNIT:
         return sys_nvmem_flash.Read(addr,sz);
      case DC_PLATFORM_NAOMI:
      case DC_PLATFORM_NAOMI2:
         return sys_nvmem_sram.Read(addr,sz);
      case DC_PLATFORM_ATOMISWAVE:
#ifndef NDEBUG
         EMUERROR3("Read from [Flash ROM] is not possible, addr=%x,size=%d",addr,sz);
#endif
         break;
   }

   return 0;
}

void WriteFlash(u32 addr,u32 data,u32 sz)
{
   switch (settings.System)
   {
      case DC_PLATFORM_DREAMCAST:
      case DC_PLATFORM_DEV_UNIT:
         sys_nvmem_flash.Write(addr,data,sz);
         break;
      case DC_PLATFORM_NAOMI:
      case DC_PLATFORM_NAOMI2:
         sys_nvmem_sram.Write(addr,data,sz);
         break;
      case DC_PLATFORM_ATOMISWAVE:
#ifndef NDEBUG
         EMUERROR4("Write to [Flash ROM] is not possible, addr=%x,data=%x,size=%d",addr,data,sz);
#endif
         break;
   }
}

u32 ReadBios(u32 addr,u32 sz)
{
   switch (settings.System)
   {
      case DC_PLATFORM_DREAMCAST:
      case DC_PLATFORM_DEV_UNIT:
      case DC_PLATFORM_NAOMI:
      case DC_PLATFORM_NAOMI2:
         return sys_rom.Read(addr,sz);
      case DC_PLATFORM_ATOMISWAVE:
         if (!(addr&0x10000)) //upper 64 kb is flashrom
            return sys_rom.Read(addr,sz);
         return ReadFlash(addr,sz);
   }
   return 0;
}

void WriteBios(u32 addr,u32 data,u32 sz)
{
   switch (settings.System)
   {
      case DC_PLATFORM_DREAMCAST:
      case DC_PLATFORM_DEV_UNIT:
      case DC_PLATFORM_NAOMI:
      case DC_PLATFORM_NAOMI2:
#ifndef NDEBUG
         EMUERROR4("Write to [Boot ROM] is not possible, addr=%x,data=%x,size=%d",addr,data,sz);
#endif
         break;
      case DC_PLATFORM_ATOMISWAVE:
         if (!(addr&0x10000)) //upper 64 kb is flashrom
         {
#ifndef NDEBUG
            EMUERROR4("Write to  [Boot ROM] is not possible, addr=%x,data=%x,size=%d",addr,data,sz);
#endif
         }
         else
            WriteFlash(addr,data,sz);
         break;
   }
}

//Area 0 mem map
//0x00000000- 0x001FFFFF	:MPX	System/Boot ROM
//0x00200000- 0x0021FFFF	:Flash Memory
//0x00400000- 0x005F67FF	:Unassigned
//0x005F6800- 0x005F69FF	:System Control Reg.
//0x005F6C00- 0x005F6CFF	:Maple i/f Control Reg.
//0x005F7000- 0x005F70FF	:GD-ROM / NAOMI BD Reg.
//0x005F7400- 0x005F74FF	:G1 i/f Control Reg.
//0x005F7800- 0x005F78FF	:G2 i/f Control Reg.
//0x005F7C00- 0x005F7CFF	:PVR i/f Control Reg.
//0x005F8000- 0x005F9FFF	:TA / PVR Core Reg.
//0x00600000- 0x006007FF	:MODEM
//0x00600800- 0x006FFFFF	:G2 (Reserved)
//0x00700000- 0x00707FFF	:AICA- Sound Cntr. Reg.
//0x00710000- 0x0071000B	:AICA- RTC Cntr. Reg.
//0x00800000- 0x00FFFFFF	:AICA- Wave Memory
//0x01000000- 0x01FFFFFF	:Ext. Device
//0x02000000- 0x03FFFFFF*	:Image Area*	2MB

//use unified size handler for registers
//it really makes no sense to use different size handlers on em -> especially when we can use templates :p
template<u32 sz, class T>
T DYNACALL ReadMem_area0(u32 addr)
{
	addr &= 0x01FFFFFF;//to get rid of non needed bits
	const u32 base=(addr>>16);
	//map 0x0000 to 0x01FF to Default handler
	//mirror 0x0200 to 0x03FF , from 0x0000 to 0x03FFF
	//map 0x0000 to 0x001F
	if (base<=0x001F)//	:MPX	System/Boot ROM
	{
		return ReadBios(addr,sz);
	}
	//map 0x0020 to 0x0021
	else if ((base>= 0x0020) && (base<= 0x0021)) // :Flash Memory
	{
		return ReadFlash(addr&0x1FFFF,sz);
	}
	//map 0x005F to 0x005F
	else if (likely(base==0x005F))
	{
		if ( /*&& (addr>= 0x00400000)*/ (addr<= 0x005F67FF)) // :Unassigned
		{
#ifndef NDEBUG
			EMUERROR2("Read from area0_32 not implemented [Unassigned], addr=%x",addr);
#endif
		}
		else if ((addr>= 0x005F7000) && (addr<= 0x005F70FF)) // GD-ROM
		{
			//EMUERROR3("Read from area0_32 not implemented [GD-ROM], addr=%x,size=%d",addr,sz);
         if (settings.System == DC_PLATFORM_NAOMI)
            return (T)ReadMem_naomi(addr,sz);
         return (T)ReadMem_gdrom(addr,sz);
		}
		else if (likely((addr>= 0x005F6800) && (addr<=0x005F7CFF))) //	/*:PVR i/f Control Reg.*/ -> ALL SB registers now
		{
			//EMUERROR2("Read from area0_32 not implemented [PVR i/f Control Reg], addr=%x",addr);
			return (T)sb_ReadMem(addr,sz);
		}
		else if (likely((addr>= 0x005F8000) && (addr<=0x005F9FFF))) //	:TA / PVR Core Reg.
		{
			//EMUERROR2("Read from area0_32 not implemented [TA / PVR Core Reg], addr=%x",addr);
			//verify(sz==4); //HOTD2 fails this check
			return (T)PvrReg(addr, u32);
		}
	}
	//map 0x0060 to 0x0060
	else if ((base ==0x0060) /*&& (addr>= 0x00600000)*/ && (addr<= 0x006007FF)) //	:MODEM
	{
		return (T)libExtDevice_ReadMem_A0_006(addr,sz);
		//EMUERROR2("Read from area0_32 not implemented [MODEM], addr=%x",addr);
	}
	//map 0x0060 to 0x006F
	else if ((base >=0x0060) && (base <=0x006F) && (addr>= 0x00600800) && (addr<= 0x006FFFFF)) //	:G2 (Reserved)
	{
		EMUERROR2("Read from area0_32 not implemented [G2 (Reserved)], addr=%x",addr);
	}
	//map 0x0070 to 0x0070
	else if ((base ==0x0070) /*&& (addr>= 0x00700000)*/ && (addr<=0x00707FFF)) //	:AICA- Sound Cntr. Reg.
	{
		//EMUERROR2("Read from area0_32 not implemented [AICA- Sound Cntr. Reg], addr=%x",addr);
		return (T) ReadMem_aica_reg(addr,sz);//libAICA_ReadReg(addr,sz);
	}
	//map 0x0071 to 0x0071
	else if ((base ==0x0071) /*&& (addr>= 0x00710000)*/ && (addr<= 0x0071000B)) //	:AICA- RTC Cntr. Reg.
	{
		//EMUERROR2("Read from area0_32 not implemented [AICA- RTC Cntr. Reg], addr=%x",addr);
      return (T)ReadMem_aica_rtc(addr,sz);
	}
	//map 0x0080 to 0x00FF
	else if ((base >=0x0080) && (base <=0x00FF) /*&& (addr>= 0x00800000) && (addr<=0x00FFFFFF)*/) //	:AICA- Wave Memory
	{
		//EMUERROR2("Read from area0_32 not implemented [AICA- Wave Memory], addr=%x",addr);
		//return (T)libAICA_ReadMem_aica_ram(addr,sz);
      switch (sz)
      {
         case 1:
            return aica_ram.data[addr & ARAM_MASK];
         case 2:
            return *(u16*)&aica_ram.data[addr & ARAM_MASK];
         case 4:
            return *(u32*)&aica_ram.data[addr & ARAM_MASK];
      }
	}
	//map 0x0100 to 0x01FF
	else if ((base >=0x0100) && (base <=0x01FF) /*&& (addr>= 0x01000000) && (addr<= 0x01FFFFFF)*/) //	:Ext. Device
	{
	//	EMUERROR2("Read from area0_32 not implemented [Ext. Device], addr=%x",addr);
		return (T)libExtDevice_ReadMem_A0_010(addr,sz);
	}
	return 0;
}

template<u32 sz, class T>
void  DYNACALL WriteMem_area0(u32 addr,T data)
{
	addr &= 0x01FFFFFF;//to get rid of non needed bits

	const u32 base=(addr>>16);

	//map 0x0000 to 0x001F
	if ((base <=0x001F) /*&& (addr<=0x001FFFFF)*/)// :MPX System/Boot ROM
	{
		//EMUERROR4("Write to  [MPX	System/Boot ROM] is not possible, addr=%x,data=%x,size=%d",addr,data,sz);
		WriteBios(addr,data,sz);
	}
	//map 0x0020 to 0x0021
	else if ((base >=0x0020) && (base <=0x0021) /*&& (addr>= 0x00200000) && (addr<= 0x0021FFFF)*/) // Flash Memory
	{
		//EMUERROR4("Write to [Flash Memory] , sz?!, addr=%x,data=%x,size=%d",addr,data,sz);
		WriteFlash(addr,data,sz);
	}
	//map 0x0040 to 0x005F -> actually, I'll only map 0x005F to 0x005F, b/c the rest of it is unspammed (left to default handler)
	//map 0x005F to 0x005F
	else if ( likely(base==0x005F) )
	{
		if (/*&& (addr>= 0x00400000) */ (addr<= 0x005F67FF)) // Unassigned
		{
			EMUERROR4("Write to area0_32 not implemented [Unassigned], addr=%x,data=%x,size=%d",addr,data,sz);
		}
		else if ((addr>= 0x005F7000) && (addr<= 0x005F70FF)) // GD-ROM
		{
			//EMUERROR4("Write to area0_32 not implemented [GD-ROM], addr=%x,data=%x,size=%d",addr,data,sz);
         if   (settings.System == DC_PLATFORM_NAOMI ||
               settings.System == DC_PLATFORM_ATOMISWAVE)
            WriteMem_naomi(addr,data,sz);
         else
            WriteMem_gdrom(addr,data,sz);
		}
		else if ( likely((addr>= 0x005F6800) && (addr<=0x005F7CFF)) ) // /*:PVR i/f Control Reg.*/ -> ALL SB registers
		{
			//EMUERROR4("Write to area0_32 not implemented [PVR i/f Control Reg], addr=%x,data=%x,size=%d",addr,data,sz);
			sb_WriteMem(addr,data,sz);
		}
		else if ( likely((addr>= 0x005F8000) && (addr<=0x005F9FFF)) ) // TA / PVR Core Reg.
		{
			//EMUERROR4("Write to area0_32 not implemented [TA / PVR Core Reg], addr=%x,data=%x,size=%d",addr,data,sz);
			verify(sz==4);
			pvr_WriteReg(addr,data);
		}
	}
	//map 0x0060 to 0x0060
	else if ((base ==0x0060) /*&& (addr>= 0x00600000)*/ && (addr<= 0x006007FF)) // MODEM
	{
		//EMUERROR4("Write to area0_32 not implemented [MODEM], addr=%x,data=%x,size=%d",addr,data,sz);
		libExtDevice_WriteMem_A0_006(addr,data,sz);
	}
	//map 0x0060 to 0x006F
	else if ((base >=0x0060) && (base <=0x006F) && (addr>= 0x00600800) && (addr<= 0x006FFFFF)) // G2 (Reserved)
	{
		EMUERROR4("Write to area0_32 not implemented [G2 (Reserved)], addr=%x,data=%x,size=%d",addr,data,sz);
	}
	//map 0x0070 to 0x0070
	else if ((base >=0x0070) && (base <=0x0070) /*&& (addr>= 0x00700000)*/ && (addr<=0x00707FFF)) // AICA- Sound Cntr. Reg.
	{
		//EMUERROR4("Write to area0_32 not implemented [AICA- Sound Cntr. Reg], addr=%x,data=%x,size=%d",addr,data,sz);
		WriteMem_aica_reg(addr,data,sz);
		return;
	}
	//map 0x0071 to 0x0071
	else if ((base >=0x0071) && (base <=0x0071) /*&& (addr>= 0x00710000)*/ && (addr<= 0x0071000B)) // AICA- RTC Cntr. Reg.
	{
		//EMUERROR4("Write to area0_32 not implemented [AICA- RTC Cntr. Reg], addr=%x,data=%x,size=%d",addr,data,sz);
      WriteMem_aica_rtc(addr,data,sz);
		return;
	}
	//map 0x0080 to 0x00FF
	else if ((base >=0x0080) && (base <=0x00FF) /*&& (addr>= 0x00800000) && (addr<=0x00FFFFFF)*/) // AICA- Wave Memory
	{
		//EMUERROR4("Write to area0_32 not implemented [AICA- Wave Memory], addr=%x,data=%x,size=%d",addr,data,sz);
		//aica_writeram(addr,data,sz);

      switch (sz)
      {
         case 1:
            aica_ram.data[addr & ARAM_MASK]         = (u8)data;
            break;
         case 2:
            *(u16*)&aica_ram.data[addr & ARAM_MASK] = (u16)data;
            break;
         case 4:
            *(u32*)&aica_ram.data[addr & ARAM_MASK] = data;
            break;
      }
		return;
	}
	//map 0x0100 to 0x01FF
	else if ((base >=0x0100) && (base <=0x01FF) /*&& (addr>= 0x01000000) && (addr<= 0x01FFFFFF)*/) // Ext. Device
	{
		//EMUERROR4("Write to area0_32 not implemented [Ext. Device], addr=%x,data=%x,size=%d",addr,data,sz);
		libExtDevice_WriteMem_A0_010(addr,data,sz);
	}
	return;
}

//Init/Res/Term
void sh4_area0_Init(void)
{
	sb_Init();
}

void sh4_area0_Reset(bool Manual)
{
	sb_Reset(Manual);
}

void sh4_area0_Term(void)
{
	sb_Term();
}


//AREA 0
_vmem_handler area0_handler;


void map_area0_init()
{

	area0_handler = _vmem_register_handler_Template(ReadMem_area0,WriteMem_area0);
}
void map_area0(u32 base)
{
	verify(base<0xE0);

	_vmem_map_handler(area0_handler,0x00|base,0x01|base);

	//0x0240 to 0x03FF mirrors 0x0040 to 0x01FF (no flashrom or bios)
	//0x0200 to 0x023F are unused
	_vmem_mirror_mapping(0x02|base,0x00|base,0x02);
}
