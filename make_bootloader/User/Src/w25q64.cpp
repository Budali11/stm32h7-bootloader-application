// Copyright 2022 Budali11
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

#include "w25q64.h"
extern QSPI_HandleTypeDef hqspi;
/**
 * @brief	reset the w25q64 controller 
 * @param	none
 * 
 */
void Flash_T::m_reset(void)
{
	QSPI_CommandTypeDef cmd = {0};
	
	cmd.Instruction = 0x66;
    cmd.AddressSize = QSPI_ADDRESS_24_BITS;
	if(m_QSPI_mode)
		cmd.InstructionMode = QSPI_INSTRUCTION_4_LINES;
	else
		cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	
	if(HAL_QSPI_Command(&hqspi, &cmd, 100) != HAL_OK)
		while(1);
	
	m_wait();
	cmd.Instruction = 0x99;
	if(HAL_QSPI_Command(&hqspi, &cmd, 100) != HAL_OK)
		while(1);

}

/*********************************************************************************************************************/
bool Flash_T::m_write_enable(void)
{
	QSPI_CommandTypeDef cmd = {0};
	QSPI_AutoPollingTypeDef cfg = {0};
		
	cmd.Instruction = 0x06;
    cmd.AddressSize = QSPI_ADDRESS_24_BITS;
	if(m_QSPI_mode == QSPI)
		cmd.InstructionMode = QSPI_INSTRUCTION_4_LINES;
	else
		cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	if(HAL_QSPI_Command(&hqspi, &cmd, 100) != HAL_OK)
		return false;
	cfg.Match = 0x02;
	cfg.Mask = 0x02;
	cfg.Interval = 0x10;
	cfg.MatchMode = QSPI_MATCH_MODE_AND;
	cfg.StatusBytesSize = 1;
	cfg.AutomaticStop = QSPI_AUTOMATIC_STOP_ENABLE;
	cmd.Instruction = 0x05;
	if(m_QSPI_mode == QSPI)
		cmd.DataMode = QSPI_DATA_4_LINES;
	else
		cmd.DataMode = QSPI_DATA_1_LINE;
	
	cmd.NbData = 1;
	
	if(HAL_QSPI_AutoPolling(&hqspi, &cmd, &cfg, 100) != HAL_OK){
		return false;
	}
	return true;
}
/**
 * @brief	exit quad mode, set member QSPI_mode to SPI(false)
 * @param	none
 */
void Flash_T::m_exit_quad_mode(void)
{
	QSPI_CommandTypeDef cmd = {0};
	cmd.InstructionMode = QSPI_INSTRUCTION_4_LINES;
    cmd.Instruction = 0xFF;
    HAL_QSPI_Command(&hqspi, &cmd, 100);
	m_QSPI_mode = SPI;
}
/**
 * @brief	enter quad spi mode and set member QSPI_mode to QSPI(true)
 * @param	none
 *  
 */
void Flash_T::m_set_quad_mode(void)
{
	uint8_t tmp = 0;
	m_read_register(&tmp, 2);
	if((tmp & 0x2) == 0)
	{
		tmp |= 0x02;
		m_write_register(tmp, 2);
	}
	if(((tmp >> 1) & 0x1) != 1)
		while(1);

	//enter quad mode
	QSPI_CommandTypeDef cmd = {0};
	cmd.Instruction = 0x38;
	cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressSize = QSPI_ADDRESS_24_BITS;
	if(HAL_QSPI_Command(&hqspi, &cmd, 100) != HAL_OK)
		while(1);
	m_QSPI_mode = QSPI;

	//set ReadParam
	cmd.InstructionMode = QSPI_INSTRUCTION_4_LINES;
    cmd.Instruction = 0xC0;
    cmd.DataMode = QSPI_DATA_4_LINES;
    cmd.NbData = 1;
	tmp = 0x03 << 4;
	m_write_enable();
	if(HAL_QSPI_Command(&hqspi, &cmd, 100) == HAL_OK)
    {
        HAL_QSPI_Transmit(&hqspi, &tmp, 100);
    }
}
/**
 * @brief	read w25q64 id
 * @param	none
 * @retval	id if read success and HAL_ERROR if sending message error
 * @note	you can add something to detect whether what you have read is the correct id 
 */
uint16_t Flash_T::m_readJEDECID(void)
{
	QSPI_CommandTypeDef cmd = {0};
	uint8_t tmp[2] = {0};
	uint16_t ret = 0;
	cmd.Instruction = 0x90;
	if(m_QSPI_mode == QSPI)
		{cmd.InstructionMode = QSPI_INSTRUCTION_4_LINES;cmd.DataMode = QSPI_DATA_4_LINES;}
	else
		{cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;cmd.DataMode = QSPI_DATA_1_LINE;}
	cmd.Address = 0;
	cmd.AddressMode = QSPI_ADDRESS_4_LINES;
	cmd.AddressSize = QSPI_ADDRESS_24_BITS;		
	
	cmd.NbData = 2;
	if(HAL_QSPI_Command(&hqspi, &cmd, 100) != HAL_OK)
		return HAL_ERROR;
	
	if(HAL_QSPI_Receive(&hqspi, tmp, 100) != HAL_OK)
		return HAL_ERROR;
	ret |= tmp[0] << 8;
	ret |= tmp[1] << 0;
	return ret;
}

bool Flash_T::m_read_register(uint8_t * rbuffer, uint16_t RegisterN)
{
	QSPI_CommandTypeDef cmd = {0};
	
	switch(RegisterN)
	{
		case 1:
			cmd.Instruction = 0x05;break;
		case 2:
			cmd.Instruction = 0x35;break;
		case 3:
			cmd.Instruction = 0x15;break;
	}
	if(m_QSPI_mode == QSPI)
		{cmd.InstructionMode = QSPI_INSTRUCTION_4_LINES;cmd.DataMode = QSPI_DATA_4_LINES;}
	else
		{cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;cmd.DataMode = QSPI_DATA_1_LINE;}
	cmd.NbData = 1;
    cmd.AddressSize = QSPI_ADDRESS_24_BITS;
	
	if(HAL_QSPI_Command(&hqspi, &cmd, 100) != HAL_OK)
		return false;
	
	if(HAL_QSPI_Receive(&hqspi, rbuffer, 100) != HAL_OK)
		return false;
	
	return true;
	
}

bool Flash_T::m_write_register(uint8_t data, uint16_t RegisterN)
{
	QSPI_CommandTypeDef cmd = {0};
	
	switch(RegisterN)
	{
		case 1:
			cmd.Instruction = 0x01;break;
		case 2:
			cmd.Instruction = 0x31;break;
		case 3:
			cmd.Instruction = 0x11;break;
		default:
			return false;
	}
	m_write_enable();
	if(m_QSPI_mode == QSPI)
		{cmd.InstructionMode = QSPI_INSTRUCTION_4_LINES;cmd.DataMode = QSPI_DATA_4_LINES;}
	else
		{cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;cmd.DataMode = QSPI_DATA_1_LINE;}
	cmd.NbData = 1;
	
    cmd.AddressSize = QSPI_ADDRESS_24_BITS;
	if(HAL_QSPI_Command(&hqspi, &cmd, 10) != HAL_OK)
		return false;
	uint8_t tmp = data;
	if(HAL_QSPI_Transmit(&hqspi, &tmp, 1000) != HAL_OK)
		return false;
	return true;

}

bool Flash_T::m_wait(void)
{
	QSPI_CommandTypeDef cmd = {0};
	QSPI_AutoPollingTypeDef cfg = {0};
	if(m_QSPI_mode == QSPI)
		{cmd.InstructionMode = QSPI_INSTRUCTION_4_LINES;cmd.DataMode = QSPI_DATA_4_LINES;}
	else
		{cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;cmd.DataMode = QSPI_DATA_1_LINE;}
	cmd.Instruction = 0x05; //read register 1
	cmd.NbData = 1; //read one byte
    cmd.AddressSize = QSPI_ADDRESS_24_BITS;

	//mask setting
	cfg.Mask = 0x01; //detect the busy bit
	cfg.Match = 0x00; //device is idle if bit busy is set as 0
	cfg.AutomaticStop = QSPI_AUTOMATIC_STOP_ENABLE; //stop sending cmd if match
	cfg.Interval = 0x10; //time between two send
	cfg.MatchMode = QSPI_MATCH_MODE_AND; //don't care when detect only one bit
	cfg.StatusBytesSize = 1; //one byte 
	if(HAL_QSPI_AutoPolling(&hqspi, &cmd, &cfg, 1000) != HAL_OK)
		return false;
	return true;
}

Flash_T::Flash_T(void)
{
	m_QSPI_mode = SPI;
	m_id = 0;
}

Flash_T::~Flash_T()
{
}

void Flash_T::init(void)
{
	m_exit_quad_mode();
	m_reset();
	m_set_quad_mode();
	m_id = m_readJEDECID();
	if(m_id == 0xef16){
		LED_Blink(5);
	}
}

bool Flash_T::read_N_bytes(uint32_t N, uint32_t address, uint8_t * rbuffer)
{
	QSPI_CommandTypeDef cmd = {0};
	
	if(address > 0xFFFFFF)
		return false;
	
	cmd.Instruction = 0x0B;
	if(m_QSPI_mode == QSPI)
		cmd.InstructionMode = QSPI_INSTRUCTION_4_LINES;
	else
		cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	
	cmd.AddressSize = QSPI_ADDRESS_24_BITS;
	cmd.Address = address;
	cmd.AddressMode = QSPI_ADDRESS_4_LINES;
	
	cmd.DataMode = QSPI_DATA_4_LINES;
	cmd.NbData = N;
	
	cmd.DummyCycles = 8;
	
	if(HAL_QSPI_Command(&hqspi, &cmd, 100) != HAL_OK)
		return false;
	if(HAL_QSPI_Receive(&hqspi, rbuffer, 100) != HAL_OK)
		return false;
	
	return true;
}

bool Flash_T::write_N_bytes(uint32_t N, uint32_t address, uint8_t * sbuffer)
{
	QSPI_CommandTypeDef cmd = {0};
	uint32_t end_addr, current_addr = 0x00, current_size;
	uint8_t * current_buffer = sbuffer;
	
	if(address > 0xFFFFFF) //detect if address bigger than max address value
		return false;
	while(current_addr <= address){ //current address increats until bigger than the passed address
		current_addr += 0x100; //increment is a page 256 bytes
	}
	current_size = current_addr - address; //now current_size is the blank space of last page to be programed
	if(current_size >= N)
	{
		current_size = N;
	}
	
	current_addr = address;
	end_addr = address + N;
	
	/* cmd config */
	if(m_QSPI_mode == QSPI)
		{cmd.Instruction = 0x02;cmd.InstructionMode = QSPI_INSTRUCTION_4_LINES;}
	else
		{cmd.Instruction = 0x32;cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;}
	
	cmd.AddressSize = QSPI_ADDRESS_24_BITS;
	cmd.Address = current_addr;
	if(m_QSPI_mode == QSPI)
		cmd.AddressMode = QSPI_ADDRESS_4_LINES;
	else
		cmd.AddressMode = QSPI_ADDRESS_1_LINE;
	
	cmd.DataMode = QSPI_DATA_4_LINES;
	cmd.NbData = current_size;
	
	
	do
	{
		m_write_enable();
  		m_set_quad_mode();
		
		if(HAL_QSPI_Command(&hqspi, &cmd, 100) != HAL_OK)
			return false;
		if(HAL_QSPI_Transmit(&hqspi, current_buffer, 10000) != HAL_OK)
			return false;
		current_addr += current_size;
		current_buffer += current_size;
		
		current_size = end_addr - current_addr;
		if(current_size > 0x100)
		{
			/*check if the rest bytes bigger than 0x100*/
			current_size = 0x100;
		}
		
		cmd.NbData = current_size;
		cmd.Address = current_addr;
	}while(end_addr > cmd.Address);
	return true;
}

bool Flash_T::sector_erase(uint32_t start, uint32_t end)
{
	QSPI_CommandTypeDef cmd = {0};
	uint16_t sector_start = 0, sector_end = 0;
	sector_start = start / 4096; //start is the num of the first sector
	sector_end = end / 4096; //end is the num of the last sector
	cmd.Instruction = 0x20;	
	cmd.AddressSize = QSPI_ADDRESS_24_BITS;
	if(m_QSPI_mode == QSPI)
		{cmd.InstructionMode = QSPI_INSTRUCTION_4_LINES;cmd.AddressMode = QSPI_ADDRESS_4_LINES;}
	else
		{cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;cmd.AddressMode = QSPI_ADDRESS_1_LINE;}
	do
	{
		m_write_enable();
		cmd.Address = sector_start * 4096; //sector increse
		if(HAL_QSPI_Command(&hqspi, &cmd, 100) != HAL_OK)
			return false;
		sector_start++;
		m_wait();
	}while(sector_start <= sector_end);
	
	return true;
}
/**
 * @brief	enter memory map mode
 * @param	none
 * @note	you can choose 0xEB or 0x0B for read command 
 */
void Flash_T::memory_map(void)
{
	QSPI_CommandTypeDef cmd = {0};
	QSPI_MemoryMappedTypeDef cfg = {0};

	cmd.InstructionMode = QSPI_INSTRUCTION_4_LINES;
	cmd.Instruction = 0xEB; //quad fast read
	cmd.AddressMode = QSPI_ADDRESS_4_LINES;
	cmd.AddressSize = QSPI_ADDRESS_24_BITS;
	cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	cmd.DataMode = QSPI_DATA_4_LINES;
	cmd.DummyCycles = 8;

	cfg.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE;
  	cfg.TimeOutPeriod = 0;
 
	if (HAL_QSPI_MemoryMapped(&hqspi, &cmd, &cfg) != HAL_OK)
	{
		LED_Blink(10); //if memory map error, blink to indicate
	}
}



void LED_Blink(uint8_t times)
{
  for (uint8_t i = 0; i < times; i++)
  {
    HAL_Delay(50);
    HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_3);
    HAL_Delay(50);
    HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_3);
  }
  
}