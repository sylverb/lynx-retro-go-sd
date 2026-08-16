//
// Copyright (c) 2004 K. Wilkins
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from
// the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//

//////////////////////////////////////////////////////////////////////////////
//                       Handy - An Atari Lynx Emulator                     //
//                          Copyright (c) 1996,1997                         //
//                                 K. Wilkins                               //
//////////////////////////////////////////////////////////////////////////////
// RAM object header file                                                   //
//////////////////////////////////////////////////////////////////////////////
//                                                                          //
// This header file provides the interface definition for the RAM class     //
// that emulates the Handy system RAM (64K)                                 //
//                                                                          //
//    K. Wilkins                                                            //
// August 1997                                                              //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////
// Revision History:                                                        //
// -----------------                                                        //
//                                                                          //
// 01Aug1997 KW Document header added & class documented.                   //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////

#ifndef RAM_H
#define RAM_H

#include "gw_malloc.h"

#ifdef TARGET_GNW
extern "C" void wdog_refresh(void);
#endif

#define RAM_SIZE				   65536

typedef struct
{
   UWORD   jump;
   UWORD   load_address;
   UWORD   size;
   UBYTE   magic[4];
}HOME_HEADER;

class CRam : public CLynxBase
{
   // Function members

   public:

      CRam(const UBYTE *filedata, ULONG filesize)
      {
         /* 64 KiB system RAM is the hottest data path — keep it in DTCM
          * (zero-wait). ITCM is reserved for packed hot *code* only. */
         mRamData = (UBYTE *)dtc_calloc(1, RAM_SIZE);
         if (!mRamData)
            mRamData = (UBYTE *)ram_calloc(1, RAM_SIZE);
         if (!mRamData)
            mRamData = (UBYTE *)ahb_calloc(1, RAM_SIZE);

         if (filedata && filesize > 64 && memcmp(filedata + 6, "BS93", 4) == 0) {
            /* GNW FIX: BS93 header fields (load addr @2-3, size @4-5) are
             * BIG-ENDIAN. The old "#ifndef MSB_FIRST" path read them little-
             * endian on our LE host/device -> wrong addr (Lode Runner got
             * 0x0002) which underflowed at "addr-=10" and OOB-wrote the 64K
             * RAM (ASan-confirmed SEGV in CRam::Reset). Read big-endian. */
            mHomebrewAddr = (filedata[2] << 8) | filedata[3];
            mHomebrewSize = (filedata[4] << 8) | filedata[5];
            mHomebrewSize = filesize > mHomebrewSize ? mHomebrewSize : filesize;
            mHomebrewAddr -= 10;
            /* defence-in-depth: never let a malformed BS93 header push the RAM
             * load out of the 64K buffer (clamp instead of OOB-writing). */
            if (mHomebrewAddr >= RAM_SIZE) mHomebrewAddr = 0;
            if (mHomebrewSize > RAM_SIZE - mHomebrewAddr)
               mHomebrewSize = RAM_SIZE - mHomebrewAddr;
            mHomebrewData = new UBYTE[mHomebrewSize];
            memcpy(mHomebrewData, filedata, mHomebrewSize);
            log_printf("Homebrew found: size=%u, addr=0x%04X\n", (unsigned int)mHomebrewSize, (unsigned int)mHomebrewAddr);
         } else {
            mHomebrewData = NULL;
            mHomebrewAddr = 0;
            mHomebrewSize = 0;
         }
         Reset();
      }

      ~CRam()
      {
         /* DTCM/RAM_EMU bumps have no free — only release homebrew scratch. */
         if (mHomebrewData) {
            delete[] mHomebrewData;
            mHomebrewData=NULL;
         }
         mRamData = NULL;
      }

      void Reset(void)
      {
         if (!mRamData) return;
         if (mHomebrewData) {
            // Load the cart into RAM
            for (ULONG off = 0; off < RAM_SIZE; off += 4096) {
               ULONG n = RAM_SIZE - off;
               if (n > 4096) n = 4096;
               memset(mRamData + off, 0x00, n);
#ifdef TARGET_GNW
               wdog_refresh();
#endif
            }
            memcpy(mRamData+mHomebrewAddr, mHomebrewData, mHomebrewSize);
            gCPUBootAddress = mHomebrewAddr;
         } else {
            for (ULONG off = 0; off < RAM_SIZE; off += 4096) {
               ULONG n = RAM_SIZE - off;
               if (n > 4096) n = 4096;
               memset(mRamData + off, 0xFF, n);
#ifdef TARGET_GNW
               wdog_refresh();
#endif
            }
         }
      }

      void Clear(void)
      {
         if (mRamData) memset(mRamData, 0, RAM_SIZE);
      }

      bool ContextSave(LSS_FILE *fp)
      {
         if(!mRamData) return 0;
         if(!lss_printf(fp,"CRam::ContextSave")) return 0;
         if(!lss_write(mRamData,sizeof(UBYTE),RAM_SIZE,fp)) return 0;
         return 1;
      }

      bool ContextLoad(LSS_FILE *fp)
      {
         char teststr[32]="XXXXXXXXXXXXXXXXX";
         if(!mRamData) return 0;
         if(!lss_read(teststr,sizeof(char),17,fp)) return 0;
         if(strcmp(teststr,"CRam::ContextSave")!=0) return 0;
         if(!lss_read(mRamData,sizeof(UBYTE),RAM_SIZE,fp)) return 0;
         return 1;
      }

      void   Poke(ULONG addr, UBYTE data) {mRamData[addr] = data;};
      UBYTE  Peek(ULONG addr) {return mRamData[addr];};
      ULONG  ObjectSize(void) {return RAM_SIZE;};
      UBYTE* GetRamPointer(void) {return mRamData;};

   // Data members

   private:
      UBYTE	*mRamData;
      UBYTE	*mHomebrewData;
      ULONG	mHomebrewSize;
      ULONG mHomebrewAddr;
};

#endif
