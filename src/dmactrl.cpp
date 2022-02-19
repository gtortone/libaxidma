#include <iostream>
#include <string>
#include <thread>
#include <fcntl.h>
#include <unistd.h>  // usleep
#include <stdexcept>
#include <sys/mman.h>
#include <fmt/core.h>

#include "dmactrl.h"

//#define DEBUG

/**
 * @brief DMACtrl constructor
 *
 * Create a memory mapped area for AXI DMA device
 *
 * @param baseaddr AXI DMA base address
 */
DMACtrl::DMACtrl(unsigned int baseaddr) {

   dh = open("/dev/mem", O_RDWR | O_SYNC);
   // check return value

   mem = (unsigned int *) mmap(NULL, AXI_DMA_DEPTH, PROT_READ | PROT_WRITE, MAP_SHARED, dh, baseaddr);
   // check return value

   minLoop = 5;
   maxLoop = 10;

   minWait = 100;    // 100 us
   maxWait = 10000;  //  10 ms
   curWait = (maxWait - minWait) / 2;

   bdStartIndex = 0;
   bdStopIndex = 0;

   initsg = false;
   blockTransfer = false;
   bufferTransfer = false;
}

/**
 * @brief DMACtrl destructor
 *
 * Unmap memory mapped area for AXI DMA device
 *
 */
DMACtrl::~DMACtrl(void) {
   munmap((unsigned int *) mem, AXI_DMA_DEPTH);
}

/**
 * @brief Set transfer channel
 *
 * @param ch channel
 */
void DMACtrl::setChannel(DMACtrl::Channel ch) {

   channel = ch;
   if(channel == MM2S)
      regs = mm2sRegs;
   else if(channel == S2MM)
      regs = s2mmRegs;
}

/**
 * @brief Set register of DMA controller with value
 *
 * @param offset address
 * @param value value
 */
void DMACtrl::setRegister(int offset, unsigned int value) {
   mem[offset>>2] = value;
}

/**
 * @brief Get register value of DMA controller
 *
 * @param offset address
 */
unsigned int DMACtrl::getRegister(int offset) {
   return (mem[offset>>2]);
}

/**
 * @brief Halt AXI DMA controller
 *
 */
void DMACtrl::halt(void) {

   if(channel == UNKNOWN)
      throw std::runtime_error(std::string(__func__) + ": DMA channel not set");

   setRegister(regs["DMACR"], 0);
}

/**
 * @brief Reset AXI DMA controller
 *
 */
void DMACtrl::reset(void) {

   if(channel == UNKNOWN)
      throw std::runtime_error(std::string(__func__) + ": DMA channel not set");

   setRegister(regs["DMACR"], 4);
}

/**
 * @brief Start DMA transfer
 *
 * - in scatter-gather mode start DMA controller and set TAILDESC register
 * - in direct mode start DMA controller and set LENGTH register
 */
void DMACtrl::run(void) {

   if(isSG()) runSG();
   else runDirect();
}

/**
 * @brief Get idle status of DMA channel (DMASR register)
 *
 * @return true: DMA is idle
 * @return false: DMA is not idle
 *
 * @note After a successful DMA transfer idle flag reports end of transfer
 */
bool DMACtrl::isIdle(void) {

   if(channel == UNKNOWN)
      throw std::runtime_error(std::string(__func__) + ": DMA channel not set");

   return( getRegister(regs["DMASR"]) & 0x0002 );
}

/**
 * @brief Get running state of DMA channel (DMASR register)
 *
 * @return true: DMA is running
 * @return false: DMA is not running
 */
bool DMACtrl::isRunning(void) {

   if(channel == UNKNOWN)
      throw std::runtime_error(std::string(__func__) + ": DMA channel not set");

   return( ~(getRegister(regs["DMASR"]) & 0x0001) );
}

/**
 * @brief Get scatter-gather engine included for DMA channel (DMASR register)
 *
 * @return true: scatter-gather engine is included
 * @return false: scatter-gather engine is not included (direct mode)
 */
bool DMACtrl::isSG(void) {

   if(channel == UNKNOWN)
      throw std::runtime_error(std::string(__func__) + ": DMA channel not set");

   return( getRegister(regs["DMASR"]) & 0x0008 );
}

/**
 * @brief Set address of memory mapped area to a value
 *
 * @param mem_address memory mapped area
 * @param offset address
 * @param value value
 */
void DMACtrl::setMem(volatile unsigned int *mem_address, int offset, unsigned int value) {
   mem_address[offset>>2] = value;
}

/**
 * @brief Get value of memory mapped area address
 *
 * @param mem_address memory mapped area
 * @param offset address
 * @return value
 */
unsigned int DMACtrl::getMem(volatile unsigned int *mem_address, int offset) {
   return(mem_address[offset>>2]);
}

/**
 * @brief Print status of DMA channel (DMASR register)
 *
 */
void DMACtrl::getStatus(void) {

   if(channel == UNKNOWN)
      throw std::runtime_error(std::string(__func__) + ": DMA channel not set");

   unsigned int status = getRegister(regs["DMASR"]);

   if(channel == S2MM)
      fmt::print("Stream to memory-mapped status (0x{:08x}@0x{:02x}): ", status, regs["DMACR"]);
   else if(channel == MM2S)
      fmt::print("Memory-mapped to stream status (0x{:08x}@0x{:02x}): ", status, regs["DMACR"]);

   if (status & 0x00000001) std::cout << " halted"; else std::cout << " running";
   if (status & 0x00000002) std::cout << " idle";
   if (status & 0x00000008) std::cout << " SGIncld";
   if (status & 0x00000010) std::cout << " DMAIntErr";
   if (status & 0x00000020) std::cout << " DMASlvErr";
   if (status & 0x00000040) std::cout << " DMADecErr";
   if (status & 0x00000100) std::cout << " SGIntErr";
   if (status & 0x00000200) std::cout << " SGSlvErr";
   if (status & 0x00000400) std::cout << " SGDecErr";
   if (status & 0x00001000) std::cout << " IOC_Irq";
   if (status & 0x00002000) std::cout << " Dly_Irq";
   if (status & 0x00004000) std::cout << " Err_Irq";
   int nirq = (status & 0x00FF0000) >> 16;
   std::cout << " IRQThresholdSts: " << nirq;

   std::cout << std::endl;
}

/**
 * @brief Get IRQioc (IRQ I/O completed) status of DMA channel (DMASR register)
 *
 * @return true: IRQioc is triggered
 * @return false: IRQioc is not triggered
 */
bool DMACtrl::IRQioc(void) {

   if(channel == UNKNOWN)
      throw std::runtime_error(std::string(__func__) + ": DMA channel not set");

   return( getRegister(regs["DMASR"]) & (1<<12) );
}

/**
 * @brief Clear IRQioc (IRQ I/O completed) status of DMA channel (DMASR register)
 */
void DMACtrl::clearIRQioc(void) {

   if(channel == UNKNOWN)
      throw std::runtime_error(std::string(__func__) + ": DMA channel not set");

   unsigned int status = getRegister(regs["DMASR"]);
   setRegister(regs["DMASR"], status & ~(1<<12));
}

/**
 * @brief Get starting address of the buffer space related to a block descriptor
 *
 * @param desc descriptor number
 * @return address 
 */
long int DMACtrl::getBufferAddress(int desc) {
   // check desc < ndesc
   return ( getMem(bdmem, BUFFER_ADDRESS + (DESC_SIZE * desc)) );
}

/**
 * @brief Initialize DMA channel in direct mode
 *
 * @param blocksize size of DMA transfer (packet size)
 * @param addr PS source/destination address for DMA transfer
 */
void DMACtrl::initDirect(unsigned int blocksize, unsigned int addr) {

   if(channel == UNKNOWN)
      throw std::runtime_error(std::string(__func__) + ": DMA channel not set");

   if(isSG())
      throw std::runtime_error(std::string(__func__) + ": DMA channel not configured for Direct mode");

   if(channel == S2MM)
      setRegister(regs["DESTINATION_ADDRESS"], addr);
   else if(channel == MM2S)
      setRegister(regs["START_ADDRESS"], addr);
   else throw std::runtime_error(std::string(__func__) + ": DMA channel not set");

   size = blocksize;

   // DMACR[0]  = 1 : run dma
   // DMACR[12] = 1 : enable Interrupt on Complete
   // DMACR[13] = 1 : enable Delay Interrupt
   // DMACR[14] = 1 : enable Error Interrupt
   // DMACR[15] = 1 : [reserved] - no effect
   setRegister(regs["DMACR"], 0xF001);
}

/**
 * @brief Start DMA channel direct mode data transfer
 */
void DMACtrl::runDirect(void) {

   if(channel == UNKNOWN)
      throw std::runtime_error(std::string(__func__) + ": DMA channel not set");

   if(isSG())
      throw std::runtime_error(std::string(__func__) + ": DMA channel not configured for Direct mode");

   setRegister(regs["LENGTH"], size);
}

/**
 * @brief Initialize DMA channel in scatter-gather mode
 *
 * @param baseaddr BRAM/RAM memory address dedicated to block descriptors
 * @param n number of block descriptors to initialize
 * @param blocksize size of DMA transfer (packet size)
 * @param tgtaddr PS source/destination address for DMA transfer
 */
void DMACtrl::initSG(unsigned int baseaddr, int n, unsigned int blocksize, unsigned int tgtaddr) {

   if(channel == UNKNOWN)
      throw std::runtime_error(std::string(__func__) + ": DMA channel not set");

   if(!isSG())
      throw std::runtime_error(std::string(__func__) + ": DMA channel not configured for Scatter-Gather mode");

   bdmem = (unsigned int *) mmap(NULL, n * DESC_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, dh, baseaddr);
   descaddr = baseaddr;
   targetaddr = tgtaddr;
   size = blocksize;
   ndesc = n;

   initSGDescriptors();
}

/**
 * @brief Start DMA channel scatter-gather mode data transfer
 */
void DMACtrl::runSG(void) {

   if(!initsg)
      throw std::runtime_error(std::string(__func__) + ": Scatter-Gather not initialized");

   // start channel with complete interrupt and cyclic mode
   setRegister(regs["DMACR"], (ndesc << 16) + 0x1011);
   setRegister(regs["TAILDESC"], descaddr + (DESC_SIZE * (ndesc-1)));

   // reset BD indexes
   blockOffset = 0;
   blockSize = 0;
   bdStartIndex = 0;
   bdStopIndex = 0;
   lastIrqThreshold = ndesc;

   // reset transfer state
   blockTransfer = false;
   bufferTransfer = false;
}

/**
 * @brief Init scatter-gather descriptors
 */
void DMACtrl::initSGDescriptors(void) {

   unsigned int i;

   // Initialization of Descriptors Array
   for(i=0; i < (DESC_SIZE * ndesc); i++)
      setMem(bdmem, i, 0);

   // Write descriptors arrays
   for(i=0; i<ndesc; i++) {
      setMem(bdmem, NXTDESC + (DESC_SIZE * i), descaddr + NXTDESC + (DESC_SIZE * (i+1)));
      setMem(bdmem, BUFFER_ADDRESS + (DESC_SIZE * i), targetaddr + (size * i));
      setMem(bdmem, CONTROL + (DESC_SIZE * i), size);
   }

   setMem(bdmem, NXTDESC + (DESC_SIZE * (ndesc-1)), 0);

   setRegister(regs["CURDESC"], descaddr);

   initsg = true;
}

/**
 * @brief Increment with packet size a buffer address of block descriptor
 *
 * @param desc block descriptor index
 */
void DMACtrl::incSGDescTable(int desc) {

   if(!initsg)
      throw std::runtime_error(std::string(__func__) + ": Scatter-Gather not initialized");

   for(unsigned int i=0; i<ndesc; i++)
      setMem(bdmem, BUFFER_ADDRESS + (DESC_SIZE * i), targetaddr + (size * (ndesc * desc + i)));
}

/**
 * @brief Print block descriptor table
 */
void DMACtrl::dumpSGDescTable(void) {

   if(!initsg)
      throw std::runtime_error(std::string(__func__) + ": Scatter-Gather not initialized");

   for(unsigned int i=0; i<ndesc; i++) {
      unsigned int bdaddr = descaddr + (DESC_SIZE * i);
      unsigned int nxtdesc = getMem(bdmem, NXTDESC + (DESC_SIZE * i));
      unsigned int buffer_address = getMem(bdmem, BUFFER_ADDRESS + (DESC_SIZE * i));
      unsigned int control =  getMem(bdmem, CONTROL + (DESC_SIZE * i));
      unsigned int status = getMem(bdmem, STATUS + (DESC_SIZE * i));
      fmt::print("BD{}: addr {:04X} NXTDESC {:04X}, BUFFER_ADDRESS {:04X}, CONTROL {:04X}, STATUS {:04X}\n", \
         i, bdaddr, nxtdesc, buffer_address, control, status);
   }
}

/**
 * @brief Print block descriptors status register
 */
void DMACtrl::dumpSGDescAllStatus(void) {

   if(!initsg)
      throw std::runtime_error(std::string(__func__) + ": Scatter-Gather not initialized");

   for(unsigned int i=0; i<ndesc; i++) {
      unsigned int status = getMem(bdmem, STATUS + (DESC_SIZE * i));
      fmt::print("BD{}: STATUS {:04X}\n", i, status);
   }
}

/**
 * @brief Clear status register of all block descriptors
 *
 * @note This method must be used when cyclic mode is not enabled
 */
void DMACtrl::clearSGDescAllStatus(void) {
   
   if(!initsg)
      throw std::runtime_error(std::string(__func__) + ": Scatter-Gather not initialized");

   for(unsigned int i=0; i<ndesc; i++)
      setMem(bdmem, STATUS + (DESC_SIZE * i), 0);
}

/**
 * @brief Get buffer address of block descriptor
 *
 * @param desc block descriptor index
 * @return buffer address
 */
long int DMACtrl::getSGDescBufferAddress(int desc) {

   if(!initsg)
      throw std::runtime_error(std::string(__func__) + ": Scatter-Gather not initialized");

   return ( getMem(bdmem, BUFFER_ADDRESS + (DESC_SIZE * desc)) );
}

/**
 * @brief Get DMA transfer buffer address
 *
 * @return buffer address
 *
 * @note This method can be used after a S2MM DMA transfer
 */
unsigned int DMACtrl::getBlockOffset(void) {
   return(blockOffset);
}

/**
 * @brief Get DMA transfer buffer size
 *
 * @return buffer size
 *
 * @note This method can be used after a S2MM DMA transfer
 */
unsigned int DMACtrl::getBlockSize(void) {
   return(blockSize);
}

void DMACtrl::calibrateWaitTime(unsigned int count) {

   if(count > maxLoop) {
      curWait *= 2;
      if(curWait > maxWait) curWait = maxWait;
   } else if(count < minLoop) {
      curWait /= 2;
      if(curWait < minWait) curWait = minWait;
   }
}

/**
 * @brief Start a DMA S2MM data transfer
 *
 * DMA mode (direct or scatter-gather) is checked and related method is invoked.
 * In case of long wait time during scatter-gather transfer (when timeout is not specified)
 * the transfer is switched from buffer (all descriptors) to single block descriptor.
 *
 * @param timeout timeout value (us) for non-blocking call (0: infinite)
 *
 * @return true: data transfer completed
 * @return false: timeout expired
 */
bool DMACtrl::rx(unsigned int timeout) {

   // check if DMA mode is scatter-gather or direct
   if(!isSG()) return(directRx(timeout));

   // check if block or buffer transfer is in progress
   if(blockTransfer) return(blockRx(timeout));
   if(bufferTransfer) return(bufferRx(timeout));

   if(curWait == maxWait) {
      // in case of low rate send ready BDs and don't wait all BDs
      return(blockRx(timeout));
   } else return(bufferRx(timeout));
}

/**
 * @brief Start a direct mode DMA S2MM data transfer
 *
 * In case of long wait time (when timeout is not specified)
 * the wait time is calibrated within two limit values. 
 *
 * @param timeout timeout value (us) for non-blocking call (0: infinite)
 *
 * @return true: data transfer completed
 * @return false: timeout expired
 */
bool DMACtrl::directRx(unsigned int timeout) {
   
   if(isSG())
      throw std::runtime_error(std::string(__func__) + ": DMA channel not configured for Direct mode");

   if(channel != S2MM)
      throw std::runtime_error(std::string(__func__) + ": DMA Scatter-Gather channel != S2MM");

   if(!isRunning())
      throw std::runtime_error(std::string(__func__) + ": DMA channel not running");

   int nloops = 0;
   unsigned int waitTime = 0;
   unsigned int step;

   if(timeout == 0)
      step = curWait;
   else
      step = minWait;

   do {

      if(isIdle()) {
         if(timeout == 0) 
            calibrateWaitTime(nloops);

         // send whole buffer
         blockOffset = 0;
         blockSize = size;

         return true;
      }

      // relax CPU
      usleep(step);

      waitTime += step;
      nloops++; 
         
   } while ( (waitTime < timeout) || (timeout == 0) );

   return false;
}

/**
 * @brief Start a scatter-gather mode DMA S2MM data transfer
 *
 * DMA transfer will include one or more block descriptors.
 *
 * @param timeout timeout value (us) for non-blocking call (0: infinite)
 *
 * @return true: data transfer completed
 * @return false: timeout expired
 */
bool DMACtrl::blockRx(unsigned int timeout) {

   if(!initsg)
      throw std::runtime_error(std::string(__func__) + ": Scatter-Gather not initialized");

   if(channel != S2MM)
      throw std::runtime_error(std::string(__func__) + ": DMA Scatter-Gather channel != S2MM");

   if(!isRunning())
      throw std::runtime_error(std::string(__func__) + ": DMA channel not running");

   int nloops = 0;
   unsigned int waitTime = 0;
   unsigned int step;
   unsigned int status;
   unsigned int irqThreshold = 0;
   unsigned int readyBlocks = 0;

   if(timeout == 0)
      step = curWait;
   else
      step = minWait;

   blockTransfer = true;

   do {
      
      status = getRegister(regs["DMASR"]);

      readyBlocks = 0;

      if(isIdle()) {
         bdStopIndex = ndesc - 1;
         readyBlocks = bdStopIndex - bdStartIndex + 1;
         lastIrqThreshold = ndesc;
         blockTransfer = false;
      } else {
         irqThreshold = (status & 0x00FF0000) >> 16;
         if(irqThreshold < lastIrqThreshold) {      // there is an increment on ready BDs...
            readyBlocks = (ndesc - irqThreshold - bdStartIndex);
            lastIrqThreshold = irqThreshold;
         }
      }
      
      /*
      fmt::print("{}) irqThreshold: {}  lastIrqThreshold: {}  readyBlocks: {}  bdStartIndex: {}  bdStopIndex: {}\n", \
         nloops, irqThreshold, lastIrqThreshold, readyBlocks, bdStartIndex, bdStopIndex);
      */

      if(readyBlocks > 0) {

         bdStopIndex = bdStartIndex + readyBlocks - 1;

         if(timeout == 0)
            calibrateWaitTime(nloops);

         // SG mode: a subset of BDs are available 
         blockOffset = getBufferAddress(bdStartIndex) - targetaddr;
         blockSize = size * (bdStopIndex - bdStartIndex + 1);

#ifdef DEBUG
         fmt::print("BDs ready from BD{} to BD{} - offset: {} size: {}\n", \
            bdStartIndex, bdStopIndex, blockOffset, blockSize);
#endif

         if(bdStopIndex < (ndesc-1))
            bdStartIndex = bdStopIndex + 1;

         return true;
      }

      // relax CPU
      usleep(step);

      waitTime += step;
      nloops++; 
      
   } while ( (waitTime < timeout) || (timeout == 0) );

   return false;
}

/**
 * @brief Start a scatter-gather mode DMA S2MM data transfer
 *
 * DMA transfer will include all block descriptors.
 *
 * @param timeout timeout value (us) for non-blocking call (0: infinite)
 *
 * @return true: data transfer completed
 * @return false: timeout expired
 */
bool DMACtrl::bufferRx(unsigned int timeout) {

   // timout: 0=infinite

   if(!initsg)
      throw std::runtime_error(std::string(__func__) + ": Scatter-Gather not initialized");

   if(channel != S2MM)
      throw std::runtime_error(std::string(__func__) + ": DMA Scatter-Gather channel != S2MM");

   if(!isRunning())
      throw std::runtime_error(std::string(__func__) + ": DMA channel not running");

   int nloops = 0;
   unsigned int waitTime = 0;
   unsigned int step;
   
   if(timeout == 0)
      step = curWait;
   else
      step = minWait;

   bufferTransfer = true;

   do {

      if(isIdle()) {
         if(timeout == 0) 
            calibrateWaitTime(nloops);

         // send whole buffer
         blockOffset = 0;
         blockSize = size * ndesc;

         bufferTransfer = false;

         return true;
      }

      // relax CPU
      usleep(step);

      waitTime += step;
      nloops++; 
 
   } while( (waitTime < timeout) || (timeout == 0) );

   return false;
}

