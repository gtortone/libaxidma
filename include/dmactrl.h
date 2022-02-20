/** @file */
#pragma once

#include <map>
#include <string>
#include <cstdint>

/**
 * @defgroup BD_GROUP Block descriptor registers
 *
 * @{
 */

/** Next block descriptor address */
#define NXTDESC                  0x00
/** Memory address for data transfer */
#define BUFFER_ADDRESS           0x08 
/** Control register */
#define CONTROL                  0x18
/** Status register */
#define STATUS                   0x1C      /// unused with 32bit addresses
/** Size of block descriptor */
#define DESC_SIZE                64

/** @} */

#define AXI_DMA_DEPTH            0xFFFF

/**
 * @brief AXI DMA controller
 *
 * Manage AXI DMA controller providing methods for status control (halt, run, reset)
 * and data transfer from/to FPGA
 *
 */

class DMACtrl {
   
public:
   DMACtrl(uint32_t baseaddr);
   ~DMACtrl(void);

   /**
   * @brief DMA channel
   *
   */
   enum Channel {
     MM2S,  ///< Memory Mapped to Stream (PS -> PL)
     S2MM,  ///< Stream to Memory Mapped (PL -> PS) 
     UNKNOWN  ///< default value before initialization
   };

   void setChannel(DMACtrl::Channel ch);
   void setRegister(uint8_t offset, uint32_t value);
   uint32_t getRegister(uint8_t offset);

   void halt(void);
   void reset(void);
   void run(void);

   bool isIdle(void);
   bool isRunning(void);
   bool isSG(void);

   void getStatus(void);
   bool IRQioc(void);
   void clearIRQioc(void);
   
   bool rx(uint32_t timeout = 0);

   /* Direct DMA methods */
   void initDirect(uint32_t blocksize, uint32_t addr);

   /* Scatter Gather DMA methods */
   void initSG(uint32_t baseaddr, uint8_t n, uint32_t blocksize, uint32_t tgtaddr);
   void incSGDescTable(uint8_t index);
   void dumpSGDescTable(void);
   void dumpSGDescAllStatus(void);
   void clearSGDescAllStatus(void);
   uint32_t getSGDescBufferAddress(uint8_t desc);

   uint32_t getBlockOffset(void);
   uint32_t getBlockSize(void);

private:

   std::map<std::string, uint8_t> mm2sRegs = { 
      {"DMACR", 0x00}, 
      {"DMASR", 0x04},
      {"START_ADDRESS", 0x18},
      {"LENGTH", 0x28},
      {"CURDESC", 0x08},
      {"TAILDESC", 0x10} };

   std::map<std::string, uint8_t> s2mmRegs = { 
      {"DMACR", 0x30}, 
      {"DMASR", 0x34},
      {"DESTINATION_ADDRESS", 0x48},
      {"LENGTH", 0x58},
      {"CURDESC", 0x38},
      {"TAILDESC", 0x40} };

   std::map<std::string, uint8_t> regs;

   DMACtrl::Channel channel = DMACtrl::Channel::UNKNOWN;

   int dh;
   volatile uint32_t* mem;      // AXI-DMA controller
   volatile uint32_t* bdmem;    // block descriptors memory (SG)
   uint32_t size;
   uint32_t descaddr;
   uint32_t targetaddr;
   uint8_t ndesc;
   uint32_t blockOffset, blockSize;
   uint8_t bdStartIndex, bdStopIndex;
   uint32_t minWait, maxWait, curWait;
   uint16_t minLoop, maxLoop;
   uint16_t lastIrqThreshold;
   bool initsg;
   bool blockTransfer, bufferTransfer;

   void setMem(volatile uint32_t *mem_address, uint32_t offset, uint32_t value);
   uint32_t getMem(volatile uint32_t *mem_address, uint32_t offset);
   void initSGDescriptors(void);
   void calibrateWaitTime(uint16_t count);
   uint32_t getBufferAddress(uint8_t desc);

   /* Direct DMA methods */
   void runDirect(void);
   bool directRx(uint32_t timeout = 0);

   /* Scatter Gather DMA methods */
   void runSG(void);
   bool blockRx(uint32_t timeout = 0);
   bool bufferRx(uint32_t timeout = 0);
};

