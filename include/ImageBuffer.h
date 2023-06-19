#include <vector>
#include <condition_variable>
#include "CircularBuffer.h"
//#include <boost/circular_buffer.hpp>

class ImageBuffer
{
  public:
    ImageBuffer () {};

    bool
    pushBack(const std::vector< std::vector<uint8_t> >&);

    std::vector<std::vector<uint8_t> >
    getFront ();

    inline bool
    isFull ()
    {
      std::lock_guard<std::mutex> buff_lock (bmutex_);
      return (buffer_.full ());
    }

    inline bool
    isEmpty ()
    {
      std::lock_guard<std::mutex> buff_lock (bmutex_);
      return (buffer_.empty ());
    }

    inline int
    getSize ()
    {
      std::lock_guard<std::mutex> buff_lock (bmutex_);
      return (int (buffer_.size ()));
    }

    inline int
    getCapacity ()
    {
      return (int (buffer_.capacity ()));
    }

    inline void
    setCapacity (int buff_size)
    {
      std::lock_guard<std::mutex> buff_lock (bmutex_);
      buffer_.set_capacity (buff_size);
    }




  private:
    ImageBuffer (const ImageBuffer&); // Disabled copy constructor
    ImageBuffer& operator = (const ImageBuffer&); // Disabled assignment operator

    std::mutex bmutex_;
    std::condition_variable buff_empty_;
   // boost::circular_buffer<std::vector<std::vector<uint8_t>  >> buffer_;
    CircularBuffer<std::vector<std::vector<uint8_t>  >>  buffer_;


};
