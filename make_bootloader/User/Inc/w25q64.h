// Copyright 2022 HaoNan
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_qspi.h"

#define W25Q64_H
#ifdef W25Q64_H

#define QSPI true
#define SPI false

class Flash_T
{
private:
    bool m_QSPI_mode;
    uint16_t m_id;
    void m_reset(void);
    bool m_write_enable(void);
    void m_exit_quad_mode(void);
    void m_set_quad_mode(void);
    uint16_t m_readJEDECID(void);
    bool m_read_register(uint8_t * rbuffer, uint16_t RegisterN);
    bool m_write_register(uint8_t data, uint16_t RegisterN);
    bool m_wait(void);
public:
    Flash_T(void);
    ~Flash_T();
    void init(void);
    bool read_N_bytes(uint32_t N, uint32_t address, uint8_t * rbuffer);
    bool write_N_bytes(uint32_t N, uint32_t address, uint8_t * sbuffer);
    bool sector_erase(uint32_t start, uint32_t end);
    void memory_map(void);
};


void LED_Blink(uint8_t times);



#endif

