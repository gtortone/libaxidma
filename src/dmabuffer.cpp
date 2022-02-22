#include <iostream>
#include <vector>
#include <fcntl.h>      // open
#include <unistd.h>     // close
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sys/mman.h>

#include "dmabuffer.h"

/**
 * @brief DMABuffer constructor
 */
DMABuffer::DMABuffer(void) {
   fd = -1;
}

/**
 * @brief DMABuffer destructor
 */
DMABuffer::~DMABuffer(void) {
   if(fd != -1)
      close();
}

/**
 * @brief Open udmabuf from /dev 
 *
 * @param bufname filename (e.g. udmabuf0)
 * @param cache_on 
 * @parblock
 * - true: disable CPU cache on the DMA buffer allocated by udmabuf (O_SYNC flag not used)
 * - false: enable CPU cache on the DMA buffer allocated by udmabuf (O_SYNC flag used)
 * @endparblock
 *
 * @note O_SYNC 
 *
 * @return true: open success
 * @return false: open failure
 */
bool DMABuffer::open(std::string bufname, bool cache_on) {

   std::vector<std::string> sys_class_path_list = {"/sys/class/u-dma-buf", "/sys/class/udmabuf"};

   bool found = false;
   for (auto& dir : sys_class_path_list) {

      std::string subdir = std::string(dir) + "/" + std::string(bufname);
      std::filesystem::directory_entry entry(subdir.data());
      if(entry.is_directory()) {
         found = true;
         sys_class_path = subdir;
         name = bufname;
      }
   }

   if(!found) {
      std::cout << "E: sys class not found" << std::endl;
      return false;
   }

   std::string filename;
   std::fstream f;
   std::string line;

   filename = sys_class_path + "/phys_addr";
   f.open(filename, std::fstream::in);
   if(!f.is_open()) {
      std::cout << "E: can not open " << filename << std::endl;
      return false;
   }
   std::getline(f, line);
   phys_addr = std::stoul(line, nullptr, 16);
   f.close();

   filename = sys_class_path + "/size";
   f.open(filename, std::fstream::in);
   if(!f.is_open()) {
      std::cout << "E: can not open " << filename << std::endl;
      return false;
   }
   std::getline(f, line);
   buf_size = std::stoul(line);
   f.close();

   filename = "/dev/" + name;
   if((fd = ::open(filename.data(), O_RDWR | ((cache_on == 0)? O_SYNC : 0))) == -1) {
      std::cout << "E: can not open " << filename << std::endl;
      return false;
   }

   buf = (uint8_t *) mmap(NULL, buf_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
   sync_mode = 1;

   return true;
}

/**
 * @brief Close udmabuf
 *
 * @return true: close success
 * @return false: close failure
 */
bool DMABuffer::close(void) {

   if(fd < 0)
      return false;

   ::close(fd);
   fd = -1;
   return true;
}

/**
 * @brief Set a sync area when CPU cache is manually managed
 *
 * @param offset area address
 * @param size area size
 * @param direction 1: DMA_TO_DEVICE (for PS->PL transfer), 2: DMA_FROM_DEVICE owner (for PL->PS transfer)
 *
 * @return true: set sync area success
 * @return false: set sync area failure
 */
bool DMABuffer::setSyncArea(uint32_t offset, uint32_t size, uint8_t direction) {

   std::string filename;
   std::fstream f;

   filename = sys_class_path + "/sync_offset";
   f.open(filename, std::fstream::out);
   if(!f.is_open()) {
      std::cout << "E: can not open " << filename << std::endl;
      return false;
   }
   f << offset;
   f.close();

   filename = sys_class_path + "/sync_size";
   f.open(filename, std::fstream::out);
   if(!f.is_open()) {
      std::cout << "E: can not open " << filename << std::endl;
      return false;
   }
   f << size;
   f.close();

   filename = sys_class_path + "/sync_direction";
   f.open(filename, std::fstream::out);
   if(!f.is_open()) {
      std::cout << "E: can not open " << filename << std::endl;
      return false;
   }
   f << direction;
   f.close();

   return true;
}

/**
 * @brief Set a buffer owner (CPU or DEVICE) when CPU cache is manually managed
 *
 * @param owner 0: CPU owns buffer, 1: DEVICE owns buffer
 *
 * @return true: set buffer owner success
 * @return false: set buffer owner failure
 */
bool DMABuffer::setBufferOwner(uint8_t owner) {

   std::string filename;
   std::fstream f;

   if(owner == CPU_OWNER) {
      filename = sys_class_path + "/sync_for_cpu";
   } else if(owner == DEVICE_OWNER) {
      filename = sys_class_path + "/sync_for_device";
   } else {
      std::cout << "E: owner not valid" << std::endl;
      return false;
   }

   f.open(filename, std::fstream::out);
   if(!f.is_open()) {
      std::cout << "E: can not open " << filename << std::endl;
      return false;
   }
   f << 1;
   f.close();

   return true;
}

/**
 * @brief Set sync mode (CPU cache strategy)
 *
 * @param mode
 * @parblock
 * - 0: CPU cache is enabled regardless of the O_SYNC flag presence. 
 * - 1: If O_SYNC is specified, CPU cache is disabled. If O_SYNC is not specified, CPU cache is enabled.
 * - 2: If O_SYNC is specified, CPU cache is disabled but CPU uses write-combine when writing data to 
 *   DMA buffer improves performance by combining multiple write accesses. If O_SYNC is not specified, 
 *   CPU cache is enabled.
 * - 3: If O_SYNC is specified, DMA coherency mode is used. If O_SYNC is not specified, CPU cache is enabled. 
 * - 4: CPU cache is enabled regardless of the O_SYNC flag presence.
 * - 5: CPU cache is disabled regardless of the O_SYNC flag presence.
 * - 6: CPU uses write-combine to write data to DMA buffer regardless of O_SYNC presence.
 * - 7: DMA coherency mode is used regardless of O_SYNC presence.
 * @endparblock
 *
 * @return true: set sync mode success
 * @return false: set sync mode failure
 */
bool DMABuffer::setSyncMode(uint8_t mode) {
   std::string filename;
   std::fstream f;

   if(mode < 0 || mode > 7)
      return false;

   filename = sys_class_path + "/sync_mode";
   f.open(filename, std::fstream::out);
   if(!f.is_open()) {
      std::cout << "E: can not open " << filename << std::endl;
      return false;
   }
   f << mode;
   f.close();

   return true;
}
