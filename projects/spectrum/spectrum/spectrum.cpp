/// (c) Koheron

#include "spectrum.hpp"

#include <thread>
#include <chrono>

Spectrum::Spectrum(Klib::DevMem& dev_mem_)
: dev_mem(dev_mem_)
{
    status = CLOSED;
}

Spectrum::~Spectrum()
{
    Close();
}

int Spectrum::Open()
{
    if(status == CLOSED) {
        config_map = dev_mem.AddMemoryMap(CONFIG_ADDR, CONFIG_RANGE);
        
        if(static_cast<int>(config_map) < 0) {
            status = FAILED;
            return -1;
        }
        
        status_map = dev_mem.AddMemoryMap(STATUS_ADDR, STATUS_RANGE);
        
        if(static_cast<int>(status_map) < 0) {
            status = FAILED;
            return -1;
        }
        
        spectrum_map = dev_mem.AddMemoryMap(SPECTRUM_ADDR, SPECTRUM_RANGE);
        
        if(static_cast<int>(spectrum_map) < 0) {
            status = FAILED;
            return -1;
        }
        
        demod_map = dev_mem.AddMemoryMap(DEMOD_ADDR, DEMOD_RANGE);
        
        if(static_cast<int>(demod_map) < 0) {
            status = FAILED;
            return -1;
        }

        noise_floor_map = dev_mem.AddMemoryMap(NOISE_FLOOR_ADDR, NOISE_FLOOR_RANGE);
        
        if(static_cast<int>(noise_floor_map) < 0) {
            status = FAILED;
            return -1;
        }
        
        raw_data = reinterpret_cast<float*>(dev_mem.GetBaseAddr(spectrum_map));
        Klib::ClearBit(dev_mem.GetBaseAddr(config_map)+AVG_OFF_OFF, 0);
        status = OPENED;
    }
    
    return 0;
}

void Spectrum::Close()
{
    if(status == OPENED) {
        dev_mem.RmMemoryMap(config_map);
        dev_mem.RmMemoryMap(status_map);
        dev_mem.RmMemoryMap(spectrum_map);
        dev_mem.RmMemoryMap(demod_map);
        dev_mem.RmMemoryMap(noise_floor_map);
        status = CLOSED;
    }
}

void Spectrum::set_scale_sch(uint32_t scale_sch)
{
    // LSB at 1 for forward FFT
    Klib::WriteReg32(dev_mem.GetBaseAddr(config_map) + CFG_FFT_OFF, 
                     1 + 2 * scale_sch);
}

void Spectrum::set_offset(uint32_t offset_real, uint32_t offset_imag)
{
    Klib::WriteReg32(dev_mem.GetBaseAddr(config_map) + SUBSTRACT_MEAN_OFF, offset_real + 16384 * offset_imag);
}

void Spectrum::set_demod_buffer(const uint32_t *data, uint32_t len)
{
    for (uint32_t i=0; i<len; i++)
        Klib::WriteReg32(dev_mem.GetBaseAddr(demod_map)+sizeof(uint32_t)*i, data[i]);
}

void Spectrum::set_noise_floor_buffer(const uint32_t *data, uint32_t len)
{
    for (uint32_t i=0; i<len; i++)
        Klib::WriteReg32(dev_mem.GetBaseAddr(noise_floor_map)+sizeof(uint32_t)*i, data[i]);
}

void Spectrum::_wait_for_acquisition()
{
    // The overhead of sleep_for might be of the order of our waiting time:
    // http://stackoverflow.com/questions/18071664/stdthis-threadsleep-for-and-nanoseconds
    std::this_thread::sleep_for(std::chrono::microseconds(ACQ_TIME_US));
}


std::array<float, WFM_SIZE>& Spectrum::get_spectrum()
{
    Klib::SetBit(dev_mem.GetBaseAddr(config_map)+ADDR_OFF, 1);    
    _wait_for_acquisition();    
    uint32_t n_avg = get_num_average();
    for(unsigned int i=0; i<WFM_SIZE; i++)
        spectrum_data[i] = raw_data[i] / float(n_avg);
    Klib::ClearBit(dev_mem.GetBaseAddr(config_map)+ADDR_OFF, 1);
    return spectrum_data;
}

uint32_t Spectrum::get_num_average()
{
    return Klib::ReadReg32(dev_mem.GetBaseAddr(status_map)+N_AVG_OFF);
}

uint32_t Spectrum::get_peak_address()
{
    return Klib::ReadReg32(dev_mem.GetBaseAddr(status_map)+PEAK_ADDRESS_OFF);
}

uint32_t Spectrum::get_peak_maximum()
{
    return Klib::ReadReg32(dev_mem.GetBaseAddr(status_map)+PEAK_MAXIMUM_OFF);
}

void Spectrum::set_address_range(uint32_t address_low, uint32_t address_high)
{
    Klib::WriteReg32(dev_mem.GetBaseAddr(config_map) + ADDRESS_LOW_OFF, address_low);
    Klib::WriteReg32(dev_mem.GetBaseAddr(config_map) + ADDRESS_HIGH_OFF, address_high);
}
