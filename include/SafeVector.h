#pragma once
#include <vector>
#include <mutex>
template<typename T>
class SafeVector {
private:
   
    mutable std::mutex mtx; // mutable to allow locking in const member functions

public:
    std::vector<T> data;
    SafeVector():data( ) {} 
// Copy constructor
    SafeVector(const SafeVector& other) {
        std::lock_guard<std::mutex> lock(other.mtx); // Lock the source object's mutex
        data = other.data;
    }

    void set(size_t index, const T & ptr) {
        std::lock_guard<std::mutex> lock(mtx);
        if (index >= data.size()) {
            throw std::out_of_range("Index out of range");
        }
        data[index] = ptr;
    }

    void erase(size_t pos){
         std::lock_guard<std::mutex> lock(mtx);
         data.erase(data.begin() + pos);
    }

    void push_back(const T& value) {
        std::lock_guard<std::mutex> lock(mtx);
        data.push_back(value);
    }

    bool pop_back(T& value) {
        std::lock_guard<std::mutex> lock(mtx);
        if(data.empty()) {
            return false;
        }
        value = data.back();
        data.pop_back();
        return true;
    }

    typename std::vector<T>::size_type size() const {
        std::lock_guard<std::mutex> lock(mtx);
        return data.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx);
        return data.empty();
    }

    T operator[](typename std::vector<T>::size_type index) const {
        std::lock_guard<std::mutex> lock(mtx);
        return data[index];
    }
    void resize(size_t newSize) {
        std::lock_guard<std::mutex> lock(mtx);
        data.resize(newSize);
    }


    // Add other required methods...
};
