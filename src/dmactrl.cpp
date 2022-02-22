#include <iostream>
#include <iomanip>
#include <string>
#include <thread>
#include <fcntl.h>
#include <unistd.h>  // usleep
#include <stdexcept>
#include <sys/mman.h>

#include "dmactrl.h"

//#define DEBUG

/**
 * @brief DMACtrl constructor
 *
 * Create a memory mapped area for AXI DMA device
 *
 * @param baseaddr AXI DMA base address
 */
DMACtrl::DMACtrl(uint32_t baseaddr) {

   dh = open("/dev/mem", O_RDWR | O_SYNC);
   // check return value

   mem = (uint32_t *) mmap(NULL, AXI_DMA_DEPTH, PROT_READ | PROT_WRITE, MAP_SHARED, dh, baseaddr);
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
   munmap((uint32_t *) mem, AXI_DMA_DEPTH);
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
void DMACtrl::setRegister(uint8_t offset, uint32_t value) {
   mem[offset>>2] = value;
}

/**
 * @brief Get register value of DMA controller
 *
 * @param offset address
 */
uint32_t DMACtrl::getRegister(uint8_t offset) {
   return (mem[offset>>2]);
}

/**
 * @brief Halt AXI DMA controller
 *
 * @throws runtime_error if DMA channel is not set
 *
 */
void DMACtrl::halt(void) {

   if(channel == UNKNOWN)
      throw std::runtime_error(std::string(__func__) + ": DMA channel is not set");

   setRegister(regs["DMACR"], 0);
}

/**
 * @brief Reset AXI DMA controller
 *
 * @throws runtime_error if DMA channel is not set
 */
void DMACtrl::reset(void) {

   if(channel == UNKNOWN)
      throw std::runtime_error(std::string(__func__) + ": DMA channel is not set");

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
 * @throws runtime_error if DMA channel is not set
 *
 * @note After a successful DMA transfer idle flag reports end of transfer
 */
bool DMACtrl::isIdle(void) {

   if(channel == UNKNOWN)
      throw std::runtime_error(std::string(__func__) + ": DMA channel is not set");

   return( getRegister(regs["DMASR"]) & 0x0002 );
}

/**
 * @brief Get running state of DMA channel (DMASR register)
 *
 * @return true: DMA is running
 * @return false: DMA is not running
 *
 * @throws runtime_error if DMA channel is not set
 */
bool DMACtrl::isRunning(void) {

   if(channel == UNKNOWN)
      throw std::runtime_error(std::string(__func__) + ": DMA channel is not set");

   return( ~(getRegister(regs["DMASR"]) & 0x0001) );
}

/**
 * @brief Get scatter-gather engine included for DMA channel (DMASR register)
 *
 * @return true: scatter-gather engine is included
 * @return false: scatter-gather engine is not included (direct mode)
 *
 * @throws runtime_error if DMA channel is not set
 */
bool DMACtrl::isSG(void) {

   if(channel == UNKNOWN)
      throw std::runtime_error(std::string(__func__) + ": DMA channel is not set");

   return( getRegister(regs["DMASR"]) & 0x0008 );
}

/**
 * @brief Set address of memory mapped area to a value
 *
 * @param mem_address memory mapped area
 * @param offset address
 * @param value value
 */
void DMACtrl::setMem(volatile uint32_t *mem_address, uint32_t offset, uint32_t value) {
   mem_address[offset>>2] = value;
}

/**
 * @brief Get value of memory mapped area address
 *
 * @param mem_address memory mapped area
 * @param offset address
 * @return value
 */
uint32_t DMACtrl::getMem(volatile uint32_t *mem_address, uint32_t offset) {
   return(mem_address[offset>>2]);
}

/**
 * @brief Print status of DMA channel (DMASR register)
 *
 * @throws runtime_error if DMA channel is not set
 */
void DMACtrl::getStatus(void) {

   if(channel == UNKNOWN)
      throw std::runtime_error(std::string(__func__) + ": DMA channel is not set");

   uint32_t status = getRegister(regs["DMASR"]);

   std::ios::fmtflags f(std::cout.flags());
   std::cout.setf(std::ios::hex, std::ios::basefield);  // set hex as the basefield
   std::cout.setf(std::ios::showbase);                  // activate showbase

   if(channel == S2MM)
      std::cout << "Stream to memory-mapped status (" << status << "@" << regs["DMACR"] << "): ";
   else if(channel == MM2S)
      std::cout << "Memory-mapped to stream status (" << status << "@" << regs["DMACR"] << "): ";

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

   // restore ios:fmtflags
   std::cout.flags(f);

   if(isSG()) {
      uint8_t nirq = (status & 0x00FF0000) >> 16;
      std::cout << " IRQThresholdSts: " << unsigned(nirq);
   }

   std::cout << std::endl;
}

/**
 * @brief Get IRQioc (IRQ I/O completed) status of DMA channel (DMASR register)
 *
 * @return true: IRQioc is triggered
 * @return false: IRQioc is not triggered
 *
 * @throws runtime_error if DMA channel is not set
 */
bool DMACtrl::IRQioc(void) {

   if(channel == UNKNOWN)
      throw std::runtime_error(std::string(__func__) + ": DMA channel is not set");

   return( getRegister(regs["DMASR"]) & (1<<12) );
}

/**
 * @brief Clear IRQioc (IRQ I/O completed) status of DMA channel (DMASR register)
 *
 * @throws runtime_error if DMA channel is not set
 */
void DMACtrl::clearIRQioc(void) {

   if(channel == UNKNOWN)
      throw std::runtime_error(std::string(__func__) + ": DMA channel is not set");

   uint32_t status = getRegister(regs["DMASR"]);
   setRegister(regs["DMASR"], status & ~(1<<12));
}

/**
 * @brief Get starting address of the buffer space related to a block descriptor
 *
 * @param desc descriptor number
 *
 * @return address 
 *
 * @throws runtime_error descriptor index is out of bound
 */
uint32_t DMACtrl::getBufferAddress(uint8_t desc) {

   if(desc > ndesc-1)
      throw std::runtime_error(std::string(__func__) + ": descriptor is out of bound");

   return ( getMem(bdmem, BUFFER_ADDRESS + (DESC_SIZE * desc)) );
}

/**
 * @brief Initialize DMA channel in direct mode
 *
 * @param blocksize size of DMA transfer (packet size)
 * @param addr PS source/destination address for DMA transfer
 *
 * @throws runtime_error if DMA channel is not set 
 * @throws runtime_error if DMA channel is not configured for direct mode
 *
 */
void DMACtrl::initDirect(uint32_t blocksize, uint32_t addr) {

   if(isSG())
      throw std::runtime_error(std::string(__func__) + ": DMA channel is not configured for Direct mode");

   if(channel == S2MM)
      setRegister(regs["DESTINATION_ADDRESS"], addr);
   else if(channel == MM2S)
      setRegister(regs["START_ADDRESS"], addr);
   else throw std::runtime_error(std::string(__func__) + ": DMA channel is not set");

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
 *
 * @throws runtime_error if DMA channel is not set
 * @throws runtime_error if DMA channel is not configured for direct mode
 */
void DMACtrl::runDirect(void) {

   if(channel == UNKNOWN)
      throw std::runtime_error(std::string(__func__) + ": DMA channel is not set");

   if(isSG())
      throw std::runtime_error(std::string(__func__) + ": DMA channel is not configured for Direct mode");

   setRegister(regs["LENGTH"], size);
}

/**
 * @brief Initialize DMA channel in scatter-gather mode
 *
 * @param baseaddr BRAM/RAM memory address dedicated to block descriptors
 * @param n number of block descriptors to initialize
 * @param blocksize size of DMA transfer (packet size)
 * @param tgtaddr PS source/destination address for DMA transfer
 *
 * @throws runtime_error if DMA channel is not set
 * @throws runtime_error if DMA channel is not configured for scatter gather mode
 */
void DMACtrl::initSG(uint32_t baseaddr, uint8_t n, uint32_t blocksize, uint32_t tgtaddr) {

   if(channel == UNKNOWN)
      throw std::runtime_error(std::string(__func__) + ": DMA channel is not set");

   if(!isSG())
      throw std::runtime_error(std::string(__func__) + ": DMA channel is not configured for Scatter-Gather mode");

   bdmem = (uint32_t *) mmap(NULL, n * DESC_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, dh, baseaddr);
   descaddr = baseaddr;
   targetaddr = tgtaddr;
   size = blocksize;
   ndesc = n;

   initSGDescriptors();
}

/**
 * @brief Start DMA channel scatter-gather mode data transfer
 *
 * @throws runtime_error if DMA channel in scatter-gather mode is not initialized
 */
void DMACtrl::runSG(void) {

   if(!initsg)
      throw std::runtime_error(std::string(__func__) + ": Scatter-Gather is not initialized");

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

   uint32_t i;

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
 *
 * @throws runtime_error if DMA channel in scatter-gather mode is not initialized
 */
void DMACtrl::incSGDescTable(uint8_t desc) {

   if(!initsg)
      throw std::runtime_error(std::string(__func__) + ": Scatter-Gather is not initialized");

   for(uint8_t i=0; i<ndesc; i++)
      setMem(bdmem, BUFFER_ADDRESS + (DESC_SIZE * i), targetaddr + (size * (ndesc * desc + i)));
}

/**
 * @brief Print block descriptor table
 *
 * @throws runtime_error if DMA channel in scatter-gather mode is not initialized
 */
void DMACtrl::dumpSGDescTable(void) {

   if(!initsg)
      throw std::runtime_error(std::string(__func__) + ": Scatter-Gather is not initialized");

   for(uint8_t i=0; i<ndesc; i++) {
      uint32_t bdaddr = descaddr + (DESC_SIZE * i);
      uint32_t nxtdesc = getMem(bdmem, NXTDESC + (DESC_SIZE * i));
      uint32_t buffer_address = getMem(bdmem, BUFFER_ADDRESS + (DESC_SIZE * i));
      uint32_t control =  getMem(bdmem, CONTROL + (DESC_SIZE * i));
      uint32_t status = getMem(bdmem, STATUS + (DESC_SIZE * i));
      std::cout << "BD" << i << ": addr " << std::hex << bdaddr << " NXTDESC " << nxtdesc << ", BUFFER_ADDRESS " << buffer_address << \
         ", CONTROL " << control << " , STATUS " << status << std::dec << std::endl;
   }
}

/**
 * @brief Print block descriptors status register
 *
 * @throws runtime_error if DMA channel in scatter-gather mode is not initialized
 */
void DMACtrl::dumpSGDescAllStatus(void) {

   if(!initsg)
      throw std::runtime_error(std::string(__func__) + ": Scatter-Gather is not initialized");

   for(uint8_t i=0; i<ndesc; i++) {
      uint32_t status = getMem(bdmem, STATUS + (DESC_SIZE * i));
      std::cout << "BD" << i << ": STATUS " << std::hex << status << std::dec << std::endl;
   }
}

/**
 * @brief Clear status register of all block descriptors
 *
 * @throws runtime_error if DMA channel in scatter-gather mode is not initialized
 *
 * @note This method must be used when cyclic mode is not enabled
 */
void DMACtrl::clearSGDescAllStatus(void) {
   
   if(!initsg)
      throw std::runtime_error(std::string(__func__) + ": Scatter-Gather is not initialized");

   for(uint8_t i=0; i<ndesc; i++)
      setMem(bdmem, STATUS + (DESC_SIZE * i), 0);
}

/**
 * @brief Get buffer address of block descriptor
 *
 * @param desc block descriptor index
 * @return buffer address
 *
 * @throws runtime_error if DMA channel in scatter-gather mode is not initialized
 */
uint32_t DMACtrl::getSGDescBufferAddress(uint8_t desc) {

   if(!initsg)
      throw std::runtime_error(std::string(__func__) + ": Scatter-Gather is not initialized");

   return ( getMem(bdmem, BUFFER_ADDRESS + (DESC_SIZE * desc)) );
}

/**
 * @brief Get DMA transfer buffer address
 *
 * @return buffer address
 *
 * @note This method can be used after a S2MM DMA transfer
 */
uint32_t DMACtrl::getBlockOffset(void) {
   return(blockOffset);
}

/**
 * @brief Get DMA transfer buffer size
 *
 * @return buffer size
 *
 * @note This method can be used after a S2MM DMA transfer
 */
uint32_t DMACtrl::getBlockSize(void) {
   return(blockSize);
}

void DMACtrl::calibrateWaitTime(uint16_t count) {

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
bool DMACtrl::rx(uint32_t timeout) {

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
 *
 * @throws runtime_error if DMA channel is not configured for direct mode
 * @throws runtime_error if DMA channel is not S2MM
 * @throws runtime_error if DMA channel is not running
 */
bool DMACtrl::directRx(uint32_t timeout) {
   
   if(isSG())
      throw std::runtime_error(std::string(__func__) + ": DMA channel is not configured for Direct mode");

   if(channel != S2MM)
      throw std::runtime_error(std::string(__func__) + ": DMA Scatter-Gather channel != S2MM");

   if(!isRunning())
      throw std::runtime_error(std::string(__func__) + ": DMA channel not running");

   uint16_t nloops = 0;
   uint32_t waitTime = 0;
   uint32_t step;

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
bool DMACtrl::blockRx(uint32_t timeout) {

   if(!initsg)
      throw std::runtime_error(std::string(__func__) + ": Scatter-Gather is not initialized");

   if(channel != S2MM)
      throw std::runtime_error(std::string(__func__) + ": DMA Scatter-Gather channel != S2MM");

   if(!isRunning())
      throw std::runtime_error(std::string(__func__) + ": DMA channel is not running");

   uint16_t nloops = 0;
   uint32_t waitTime = 0;
   uint32_t step;
   uint32_t status;
   uint16_t irqThreshold = 0;
   uint8_t readyBlocks = 0;

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
      
#ifdef DEBUG
      std::cout << nloops << ") irqThreshold: " << irqThreshold << " lastIrqThreshold: " << lastIrqThreshold << \
         " readyBlocks: " << unsigned(readyBlocks) << " bdStartIndex: " << unsigned(bdStartIndex) << \
         " bdStopIndex: " << unsigned(bdStopIndex) << std::endl;
#endif

      if(readyBlocks > 0) {

         bdStopIndex = bdStartIndex + readyBlocks - 1;

         if(timeout == 0)
            calibrateWaitTime(nloops);

         // SG mode: a subset of BDs are available 
         blockOffset = getBufferAddress(bdStartIndex) - targetaddr;
         blockSize = size * (bdStopIndex - bdStartIndex + 1);

#ifdef DEBUG
         std::cout << "BDs ready from " << unsigned(bdStartIndex) << " to " << unsigned(bdStopIndex) << \
            " - offset: " << blockOffset << " size: " << blockSize << std::endl;
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
 *
 * @throws runtime_error if DMA channel is not initialized
 * @throws runtime_error if DMA channel is not S2MM
 * @throws runtime_error if DMA channel is not running
 */
bool DMACtrl::bufferRx(uint32_t timeout) {

   // timout: 0=infinite

   if(!initsg)
      throw std::runtime_error(std::string(__func__) + ": Scatter-Gather is not initialized");

   if(channel != S2MM)
      throw std::runtime_error(std::string(__func__) + ": DMA Scatter-Gather channel != S2MM");

   if(!isRunning())
      throw std::runtime_error(std::string(__func__) + ": DMA channel is not running");

   uint16_t nloops = 0;
   uint32_t waitTime = 0;
   uint32_t step;
   
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

