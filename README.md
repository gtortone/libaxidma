# AXI-DMA C/C++ library

The Xilinx LogiCORE IP AXI Direct Memory Access (AXI DMA) core is a soft Xilinx IP core for use with the Xilinx Vivado Design Suite. The AXI DMA provides high-bandwidth direct memory access between memory and AXI4-Stream target peripherals. Its optional scatter/gather capabilities also offload data movement tasks from the Central Processing Unit (CPU).

This library provides C/C++ classes to handle DMA transfers. User DMA buffers implementation is based on udmabuf project (https://github.com/ikwzm/udmabuf)

#### Example of S2MM (PL -> PS) DMA transfer in direct or scatter-gather mode:

```cpp
  
#include "dmactrl.h"
#include "dmabuffer.h"

#define AXI_DMA_BASEADDR      0x40400000     // AXI DMA base address
#define DESC_BASEADDR         0x40000000     // scatter-gather block descriptors memory area
#define NDESC                 8              // only for scatter-gather mode
#define RXSIZE                8192           // DMA packet size

DMACtrl dmac(AXI_DMA_BASEADDR);
DMABuffer dbuf;
uint16_t *bufusint;

// open and initialize udmabuf
if(!dbuf.open("udmabuf0", true)) {
   fmt::print("E: error opening /dev/udmabuf0\n");
   return EXIT_FAILURE;
}
memset(dbuf.buf, 0, dbuf.size());

// local buffer pointer to udmabuf (16 bit words)
bufusint = reinterpret_cast<uint16_t *>(dbuf.buf);

// set DMA channel RX (S2MM)
dmac.setChannel(DMACtrl::Channel::S2MM);

dmac.reset();
dmac.halt();

if(dmac.isSG())
   dmac.initSG(DESC_BASEADDR, NDESC, RXSIZE, dbuf.getPhysicalAddress());
else
   dmac.initDirect(RXSIZE, dbuf.getPhysicalAddress()); 

dmac.run();

while(!dmac.isIdle()) {

   if(dmac.rx()) { 

      // dump buffer on screen
      for(uint16_t i=0; i<RXSIZE/2; i++)
         fmt::print("{:04X} ", bufusint[i]);
      fmt::print("\n"); 
   }
}
  
```


