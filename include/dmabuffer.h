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
   uint32_t       buf_size;
   uint32_t       phys_addr;
   uint8_t        sync_mode;
   bool           cache_on;

public:
   DMABuffer(void);
   ~DMABuffer(void);

   uint8_t *buf;     ///< Buffer for data transfer

   bool open(std::string bufname, bool cache_on);
   bool close(void);
   /** Get physical address of udmabuf buffer */
   uint32_t getPhysicalAddress(void) { return phys_addr; };
   /** Get size of udmabuf buffer */
   uint32_t getBufferSize(void) { return buf_size; };
   bool setSyncArea(uint32_t offset, uint32_t size, uint8_t direction);
   bool setBufferOwner(uint8_t owner);
   bool setSyncMode(uint8_t mode);
};

