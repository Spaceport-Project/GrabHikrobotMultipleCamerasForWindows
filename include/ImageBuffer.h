#include <vector>
#include <condition_variable>
//#include "CircularBuffer.h"
#include <boost/circular_buffer.hpp>

template <class DataType>
class ImageBuffer
{
  public:
    ImageBuffer () {};

    bool
    pushBack(const DataType &);

    DataType
    getFront ();
    bool pop_back();

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
    boost::circular_buffer<DataType> buffer_;
   // CircularBuffer< DataType>  buffer_;


};


template <class DataType>
bool
ImageBuffer<DataType>::pushBack (const DataType& imageBuffers)
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
template <class DataType>
DataType 
ImageBuffer<DataType>::getFront ()
{

	DataType imageBuffers;
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

template <class DataType>
bool 
ImageBuffer<DataType>::pop_back ()
{

	DataType imageBuffers;
  {
    std::unique_lock<std::mutex> buff_lock(bmutex_);
    while (buffer_.empty ())
    {
     // if (is_done) break;

      buff_empty_.wait(buff_lock);
    }

    buffer_.pop_back ();
  }
  return true;
}


