/// (c) Koheron

#include "oscillo.hpp"
#include <string.h>
#include <thread>
#include <chrono>

Oscillo::Oscillo(Klib::DevMem& dev_mem_)
: dev_mem(dev_mem_)
, data_decim(0)
{
    avg_on = false;
    status = CLOSED;
}
 
Oscillo::~Oscillo()
{
    Close();
}

int Oscillo::Open()
{
    // Reopening
    if(status == OPENED) {
        Close();
    }

    if(status == CLOSED) {       
   
        config_map = dev_mem.AddMemoryMap(CONFIG_ADDR, CONFIG_RANGE);
        
        if (static_cast<int>(config_map) < 0) {
            status = FAILED;
            return -1;
        }
        
        status_map = dev_mem.AddMemoryMap(STATUS_ADDR, STATUS_RANGE);
        
        if (static_cast<int>(status_map) < 0) {
            status = FAILED;
            return -1;
        }
        
        adc_1_map = dev_mem.AddMemoryMap(ADC1_ADDR, ADC1_RANGE);
        
        if (static_cast<int>(adc_1_map) < 0) {
            status = FAILED;
            return -1;
        }
        
        adc_2_map = dev_mem.AddMemoryMap(ADC2_ADDR, ADC2_RANGE);
        
        if (static_cast<int>(adc_2_map) < 0) {
            status = FAILED;
            return -1;
        }

        raw_data_1 = reinterpret_cast<uint32_t*>(dev_mem.GetBaseAddr(adc_1_map));
        raw_data_2 = reinterpret_cast<uint32_t*>(dev_mem.GetBaseAddr(adc_2_map));

        status = OPENED;
        
        // Reset averaging
        set_averaging(false);
    }
    
    return 0;
}

void Oscillo::Close()
{
    if(status == OPENED) {
        dev_mem.RmMemoryMap(config_map);
        dev_mem.RmMemoryMap(status_map);
        dev_mem.RmMemoryMap(adc_1_map);
        dev_mem.RmMemoryMap(adc_2_map);
        status = CLOSED;
    }
}

void Oscillo::_wait_for_acquisition()
{
    // The overhead of sleep_for might be of the order of our waiting time:
    // http://stackoverflow.com/questions/18071664/stdthis-threadsleep-for-and-nanoseconds
    std::this_thread::sleep_for(std::chrono::microseconds(ACQ_TIME_US));
}

// http://stackoverflow.com/questions/12276675/modulus-with-negative-numbers-in-c
inline long long int mod(long long int k, long long int n) 
{
    return ((k %= n) < 0) ? k+n : k;
}

#define POW_2_31 2147483648 // 2^31
#define POW_2_32 4294967296 // 2^32

inline float _raw_to_float(uint32_t raw) 
{
    return float(mod(raw - POW_2_31, POW_2_32) - POW_2_31);
}

// Read only one channel
std::array<float, WFM_SIZE>& Oscillo::read_data(bool channel)
{
    Klib::MemMapID adc_map;
    channel ? adc_map = adc_1_map : adc_map = adc_2_map;
    Klib::SetBit(dev_mem.GetBaseAddr(config_map)+ADDR_OFF, 1);
    _wait_for_acquisition();
    uint32_t *raw_data = reinterpret_cast<uint32_t*>(dev_mem.GetBaseAddr(adc_map));

    if(avg_on) {
        float num_avg = float(Klib::ReadReg32(dev_mem.GetBaseAddr(status_map)+N_AVG1_OFF));  
        for(unsigned int i=0; i < WFM_SIZE; i++)
            data[i] = _raw_to_float(raw_data[i]) / num_avg;
    } else {
        for(unsigned int i=0; i < WFM_SIZE; i++)
            data[i] = _raw_to_float(raw_data[i]);
    }
    Klib::ClearBit(dev_mem.GetBaseAddr(config_map)+ADDR_OFF, 1);
    return data;
}

// Read the two channels
std::array<float, 2*WFM_SIZE>& Oscillo::read_all_channels()
{
    Klib::SetBit(dev_mem.GetBaseAddr(config_map)+ADDR_OFF, 1);
    _wait_for_acquisition();

    if(avg_on) {
        float num_avg = float(Klib::ReadReg32(dev_mem.GetBaseAddr(status_map)+N_AVG1_OFF)); 
        for(unsigned int i=0; i<WFM_SIZE; i++) {
            data_all[i] = _raw_to_float(raw_data_1[i]) / num_avg;
            data_all[i + WFM_SIZE] = _raw_to_float(raw_data_2[i]) / num_avg;
        }
    } else {
        for(unsigned int i=0; i<WFM_SIZE; i++) {
            data_all[i] = _raw_to_float(raw_data_1[i]);
            data_all[i + WFM_SIZE] = _raw_to_float(raw_data_2[i]);
        }
    }
    Klib::ClearBit(dev_mem.GetBaseAddr(config_map)+ADDR_OFF, 1);
    return data_all;
}


// Read the two channels but take only one point every decim_factor points
std::vector<float>& Oscillo::read_all_channels_decim(uint32_t decim_factor)
{
    Klib::SetBit(dev_mem.GetBaseAddr(config_map)+ADDR_OFF, 1);
    uint32_t n_pts = WFM_SIZE/decim_factor;
    data_decim.resize(2*n_pts);
    _wait_for_acquisition();

    if(avg_on) {
        float num_avg = float(Klib::ReadReg32(dev_mem.GetBaseAddr(status_map)+N_AVG1_OFF)); 
        for(unsigned int i=0; i<n_pts; i++) {
            data_decim[i] = _raw_to_float(raw_data_1[decim_factor * i])/num_avg;
            data_decim[i + n_pts] = _raw_to_float(raw_data_2[decim_factor * i])/num_avg;
        }
    } else {
        for(unsigned int i=0; i<n_pts; i++) {
            data_decim[i] = _raw_to_float(raw_data_1[decim_factor * i]);
            data_decim[i + n_pts] = _raw_to_float(raw_data_2[decim_factor * i]);
        }
    }
    Klib::ClearBit(dev_mem.GetBaseAddr(config_map)+ADDR_OFF, 1);
    return data_decim;
}

void Oscillo::set_averaging(bool avg_status)
{
    avg_on = avg_status;
    
    if(avg_on) {
        Klib::ClearBit(dev_mem.GetBaseAddr(config_map)+AVG0_OFF, 0);
        Klib::ClearBit(dev_mem.GetBaseAddr(config_map)+AVG1_OFF, 0);
    } else {
        Klib::SetBit(dev_mem.GetBaseAddr(config_map)+AVG0_OFF, 0);
        Klib::SetBit(dev_mem.GetBaseAddr(config_map)+AVG1_OFF, 0);
    }
}

uint32_t Oscillo::get_num_average()
{
    return avg_on ? Klib::ReadReg32(dev_mem.GetBaseAddr(status_map)+N_AVG1_OFF) : 0;
}