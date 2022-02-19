/** @file */
#pragma once

#include <string>

#define  CPU_OWNER               0x01
#define  DEVICE_OWNER            0x02

/**
 * @brief User space DMA buffer
 *
 * Manage user DMA buffers allocated in CMA (contiguous memory).
 * Implementation based on udmabuf (https://github.com/ikwzm/udmabuf)
 */
class DMABuffer {

private:
   std::string    name;
   std::string    sys_class_path;
   int            fd;
   unsigned int   buf_size;
   unsigned long  phys_addr;
   unsigned long  sync_mode;
   bool           cache_on;

public:
   DMABuffer(void);
   ~DMABuffer(void);

   unsigned char* buf;     ///< Buffer for data transfer

   bool open(std::string bufname, bool cache_on);
   bool close(void);
   /** Get physical address of udmabuf buffer */
   unsigned long getPhysicalAddress(void) { return phys_addr; };
   /** Get size of udmabuf buffer */
   unsigned int getBufferSize(void) { return buf_size; };
   bool setSyncArea(unsigned int offset, unsigned int size, unsigned int direction);
   bool setBufferOwner(unsigned int owner);
   bool setSyncMode(unsigned int mode);
};

