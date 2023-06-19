#include "ImageBuffer.h"

bool
ImageBuffer::pushBack (const std::vector< std::vector<uint8_t> >& imageBuffers)
{
  bool retVal = false;
  {
   std::lock_guard<std::mutex> buff_lock (bmutex_);
    if (!buffer_.full ()) retVal = true;
    buffer_.push_back(imageBuffers);

  }
  buff_empty_.notify_one ();
  return (retVal);
}

//////////////////////////////////////////////////////////////////////////////////////////
std::vector<std::vector<uint8_t>> 
ImageBuffer::getFront ()
{

	std::vector<std::vector<uint8_t>  >  imageBuffers;
  {
    std::unique_lock<std::mutex> buff_lock(bmutex_);
    while (buffer_.empty ())
    {
     // if (is_done) break;

      buff_empty_.wait(buff_lock);
    }

    imageBuffers = buffer_.front();
    buffer_.pop_front ();
  }
  return (imageBuffers);
}
